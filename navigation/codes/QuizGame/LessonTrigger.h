// LessonTrigger.h — header-only
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>

#include "Manager/TaskManager.h"
#include "QuizGame.h"

// 小工具：把测验奖励应用到 TaskManager（写成 inline，放头文件不会多重定义）
namespace {
inline void applyQuizRewards(TaskManager& tm, const QuizGame::Effects& eff) {
    if (eff.points != 0) {
        std::string autoId = "__quiz_reward__" + std::to_string(std::rand());
        tm.addTask(autoId, "Class Quiz Reward", "", "", eff.points, 0);
        tm.completeTask(autoId);
    }
    if (eff.energy != 0) {
        tm.modifyEnergy(static_cast<float>(eff.energy));
    }
}
}

class LessonTrigger {
public:
    struct Slot {
        std::string course;    // 课程名
        std::string location;  // 楼名（需与 Entrance layer/你的日程 JSON 一致）
        std::string timeStr;   // "09:00-10:15"
        int startMin = 0;      // 分钟
        int endMin   = 0;
    };
    struct DaySchedule { std::vector<Slot> slots; };

    // 读取课程表（形如 { "schedule": { "Mon": [ {course, location, time}, ... ] , ... } }）
    inline bool loadSchedule(const std::string& jsonPath) {
        using nlohmann::json;
        std::ifstream ifs(jsonPath);
        if (!ifs) { std::cerr << "[LessonTrigger] Cannot open: " << jsonPath << "\n"; return false; }
        json j;
        try { ifs >> j; }
        catch (const std::exception& e) {
            std::cerr << "[LessonTrigger] JSON parse error: " << e.what() << "\n"; return false;
        }

        schedules.clear();
        if (!j.contains("schedule") || !j["schedule"].is_object()) {
            std::cerr << "[LessonTrigger] JSON missing 'schedule' object\n"; return false;
        }
        const auto& S = j["schedule"];
        for (auto it = S.begin(); it != S.end(); ++it) {
            const std::string weekday = it.key();
            DaySchedule day;
            if (!it.value().is_array()) continue;

            for (const auto& obj : it.value()) {
                Slot s;
                s.course   = obj.value("course",   "");
                s.location = obj.value("location", "");
                s.timeStr  = obj.value("time",     "");
                if (s.course.empty() || s.location.empty() || s.timeStr.empty()) continue;
                if (!parseTimeRange(s.timeStr, s.startMin, s.endMin)) continue;
                day.slots.push_back(std::move(s));
            }
            schedules[weekday] = std::move(day);
        }
        schedulePath = jsonPath;
        return true;
    }

    // 触发结果：用最少枚举 + outHint 说明原因
    enum class Result {
        NoTrigger,              // 没有触发（包括未到/已过/今天没课），请看 outHint
        TriggeredQuiz,          // 成功打开测验
        WrongBuildingHintShown, // 时间匹配但楼不对（outHint 告诉去哪里）
        AlreadyFired            // 这节课测验做过了（outHint 告知）
    };

    // 主调用：weekday("Mon"), heroBuilding(从 EntranceArea 记录到的楼名), minutesSinceMidnight, quizJsonPath
    inline Result tryTrigger(const std::string& weekday,
                             const std::string& heroBuilding,
                             int minutesSinceMidnight,
                             const std::string& quizJsonPath,
                             TaskManager& tm,
                             std::string* outHint /*= nullptr*/)
    {
        auto it = schedules.find(weekday);
        if (it == schedules.end()) {
            if (outHint) *outHint = "No classes scheduled for today.";
            return Result::NoTrigger;
        }

        // —— 找到当天与当前分钟匹配的课时（含边界）
        std::vector<const Slot*> timeMatched;
        for (const auto& s : it->second.slots) {
            if (minutesSinceMidnight >= s.startMin && minutesSinceMidnight <= s.endMin) {
                timeMatched.push_back(&s);
            }
        }

        // ★ 没有任何进行中的课：给出“未到时间/已过时间/今天没课”的提示
        if (timeMatched.empty()) {
            if (outHint) {
                // 找“下一节尚未开始的课”和“最近已结束的课”
                const Slot* nextSlot = nullptr; // minutes < startMin 中 startMin 最小者
                const Slot* prevSlot = nullptr; // minutes > endMin   中 endMin 最大者
                for (const auto& s : it->second.slots) {
                    if (minutesSinceMidnight < s.startMin) {
                        if (!nextSlot || s.startMin < nextSlot->startMin) nextSlot = &s;
                    } else if (minutesSinceMidnight > s.endMin) {
                        if (!prevSlot || s.endMin > prevSlot->endMin) prevSlot = &s;
                    }
                }

                if (nextSlot) {
                    *outHint = "Class hasn't started yet.\n"
                               "Next: " + nextSlot->course +
                               " at "   + nextSlot->location +
                               "  "    + nextSlot->timeStr;
                } else if (prevSlot) {
                    *outHint = "Classes are over for now.\n"
                               "Last: " + prevSlot->course +
                               "  "    + prevSlot->timeStr;
                } else {
                    *outHint = "No classes scheduled for today.";
                }
            }
            return Result::NoTrigger;
        }

        // 有进行中的课，检查楼名是否匹配
        const std::string heroNorm = normalizeBuilding(heroBuilding);
        for (const Slot* ps : timeMatched) {
            const std::string locNorm = normalizeBuilding(ps->location);
            if (locNorm == heroNorm) {
                const std::string key = makeSlotKey(weekday, ps->location, ps->startMin, ps->endMin, ps->course);
                if (fired.count(key)) {
                    if (outHint) *outHint = "You've already completed this class quiz.";
                    return Result::AlreadyFired;
                }

                // 打开测验
                QuizGame quiz(quizJsonPath, ps->course);
                quiz.run();
                auto eff = quiz.getResultEffects();
                applyQuizRewards(tm, eff);

                fired.insert(key);
                return Result::TriggeredQuiz;
            }
        }

        // 时间匹配但楼不匹配 → 组织提示文本返回
        if (outHint) {
            std::vector<std::string> need;
            need.reserve(timeMatched.size());
            for (const Slot* ps : timeMatched) need.push_back(ps->location);
            dedup(need);

            std::ostringstream oss;
            oss << "You are in the wrong building.\nPlease go to: ";
            for (size_t i = 0; i < need.size(); ++i) {
                if (i) oss << " / ";
                oss << need[i];
            }
            *outHint = oss.str();
        }
        return Result::WrongBuildingHintShown;
    }

private:
    // —— 工具函数（全部 inline）
    static inline bool parseTimeRange(const std::string& s, int& startMin, int& endMin) {
        auto trim = [](std::string x){
            size_t a = x.find_first_not_of(" \t");
            size_t b = x.find_last_not_of(" \t");
            if (a==std::string::npos) return std::string{};
            return x.substr(a, b-a+1);
        };
        auto pos = s.find('-'); if (pos == std::string::npos) return false;
        std::string L = trim(s.substr(0, pos));
        std::string R = trim(s.substr(pos+1));
        auto toMin = [](const std::string& hhmm, int& m)->bool{
            int h=0, mi=0; if (std::sscanf(hhmm.c_str(), "%d:%d", &h, &mi)!=2) return false;
            m = h*60 + mi; return true;
        };
        if (!toMin(L, startMin)) return false;
        if (!toMin(R, endMin))   return false;
        return true;
    }

    static inline std::string normalizeBuilding(std::string s) {
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); }), s.end());
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        return s;
    }

    static inline std::string makeSlotKey(const std::string& weekday,
                                          const std::string& location,
                                          int startMin, int endMin,
                                          const std::string& course) {
        std::ostringstream oss;
        oss << weekday << "|" << location << "|" << startMin << "-" << endMin << "|" << course;
        return oss.str();
    }

    static inline void dedup(std::vector<std::string>& v) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }

private:
    std::unordered_map<std::string, DaySchedule> schedules;
    std::unordered_set<std::string> fired; // 已触发过的(weekday|location|start-end|course)
    std::string schedulePath;
};
