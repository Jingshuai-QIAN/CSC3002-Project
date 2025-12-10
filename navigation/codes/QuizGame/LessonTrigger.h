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

// Apply quiz rewards to TaskManager
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
        std::string course;    ///< Course name
        std::string location;  ///< Building name 
        std::string timeStr;   ///< Time string like "09:00-10:15"
        int startMin = 0;      ///< Start time in minutes since midnight
        int endMin   = 0;      ///< End time in minutes since midnight
    };
    struct DaySchedule { std::vector<Slot> slots; };

    /**
     * @brief Load schedule from JSON file.
     * 
     * Expected JSON format: { "schedule": { "Mon": [ {course, location, time}, ... ] , ... } }
     * 
     * @param jsonPath Path to the schedule JSON file.
     * @return true if schedule loaded successfully, false otherwise.
     */
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

     /**
     * @enum Result
     * @brief Trigger result enumeration.
     * 
     * Use minimal enum + outHint to explain reasons.
     */
    enum class Result {
        NoTrigger,              ///< No trigger (including not started/already passed/no class today), check outHint
        TriggeredQuiz,          ///< Successfully opened quiz
        WrongBuildingHintShown, ///< Time matches but building is wrong (outHint tells where to go)
        AlreadyFired            ///< This lesson quiz has already been completed (outHint informs)
    };

    /**
     * @brief Main trigger function.
     * 
     * @param weekday Weekday string like "Mon".
     * @param heroBuilding Building name from EntranceArea record.
     * @param minutesSinceMidnight Current time in minutes since midnight.
     * @param quizJsonPath Path to quiz JSON file.
     * @param tm TaskManager reference.
     * @param outHint Optional pointer to receive hint message.
     * @return Result Trigger result.
     */
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

        // Find class slots that match current time (including boundaries)
        std::vector<const Slot*> timeMatched;
        for (const auto& s : it->second.slots) {
            if (minutesSinceMidnight >= s.startMin && minutesSinceMidnight <= s.endMin) {
                timeMatched.push_back(&s);
            }
        }

        // No ongoing classes: provide "not started/already passed/no class today" hint
        if (timeMatched.empty()) {
            if (outHint) {
                // Find "next class not started yet" and "most recent ended class"
                const Slot* nextSlot = nullptr; // Minimum startMin where minutes < startMin
                const Slot* prevSlot = nullptr; // Maximum endMin where minutes > endMin
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

        // Have ongoing classes, check if building name matches
        const std::string heroNorm = normalizeBuilding(heroBuilding);
        for (const Slot* ps : timeMatched) {
            const std::string locNorm = normalizeBuilding(ps->location);
            if (locNorm == heroNorm) {
                const std::string key = makeSlotKey(weekday, ps->location, ps->startMin, ps->endMin, ps->course);
                if (fired.count(key)) {
                    if (outHint) *outHint = "You've already completed this class quiz.";
                    return Result::AlreadyFired;
                }

                // Open quiz
                QuizGame quiz(quizJsonPath, ps->course);
                quiz.run();
                auto eff = quiz.getResultEffects();
                applyQuizRewards(tm, eff);

                fired.insert(key);
                return Result::TriggeredQuiz;
            }
        }

        // Time matches but building doesn't → organize hint text
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
    /**
     * @brief Parse time range string like "09:00-10:15".
     * 
     * @param s Time range string.
     * @param startMin Output start time in minutes.
     * @param endMin Output end time in minutes.
     * @return true if parsing succeeded, false otherwise.
     */
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

    /**
     * @brief Normalize building name by removing spaces and converting to uppercase.
     * 
     * @param s Building name string.
     * @return Normalized building name.
     */
    static inline std::string normalizeBuilding(std::string s) {
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); }), s.end());
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        return s;
    }

    /**
     * @brief Create unique key for a class slot.
     * 
     * @param weekday Weekday string.
     * @param location Building location.
     * @param startMin Start time in minutes.
     * @param endMin End time in minutes.
     * @param course Course name.
     * @return Unique slot key string.
     */
    static inline std::string makeSlotKey(const std::string& weekday,
                                          const std::string& location,
                                          int startMin, int endMin,
                                          const std::string& course) {
        std::ostringstream oss;
        oss << weekday << "|" << location << "|" << startMin << "-" << endMin << "|" << course;
        return oss.str();
    }

    /**
     * @brief Remove duplicates from string vector.
     * 
     * @param v Vector to deduplicate.
     */
    static inline void dedup(std::vector<std::string>& v) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }

private:
    std::unordered_map<std::string, DaySchedule> schedules;    ///< Loaded schedules by weekday
        std::unordered_set<std::string> fired;     ///< Already triggered slots (weekday|location|start-end|course)
    std::string schedulePath;    ///< Path to the schedule file
};
