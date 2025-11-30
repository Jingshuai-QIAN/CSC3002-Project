// QuizGame.cpp

#include "QuizGame.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include "Utils/Logger.h"

// ---------------- OptionButton 实现 ----------------
QuizGame::OptionButton::OptionButton(const sf::Font& font,
                                     const std::string& str,
                                     const sf::Vector2f& position,
                                     const sf::Vector2f& size)
    : text(font)   // ✅ 关键：必须在初始化列表中构造
{
    shape.setSize(size);
    shape.setFillColor(sf::Color(70, 130, 255));
    shape.setOutlineColor(sf::Color(40, 100, 220));
    shape.setOutlineThickness(2.f);
    shape.setPosition(position);

    text.setString(str);
    text.setCharacterSize(22);
    text.setFillColor(sf::Color::White);
    text.setStyle(sf::Text::Bold);

    float textY = position.y + (size.y - static_cast<float>(text.getCharacterSize())) / 2.f - 4.f;
    text.setPosition(sf::Vector2f(position.x + 15.f, textY));
}

bool QuizGame::OptionButton::isClicked(const sf::Vector2f& mousePos) const {
    return shape.getGlobalBounds().contains(mousePos);
}

void QuizGame::OptionButton::setFont(const sf::Font& f) {
    text.setFont(f);
}

// ---------------- QuizGame 私有方法 ----------------
std::string QuizGame::wrapText(const std::string& text, size_t lineLength) const {
    std::stringstream ss(text);
    std::string word;
    std::string result;
    std::string line;

    while (ss >> word) {
        if (line.length() + word.length() + 1 > lineLength) {
            if (!line.empty()) {
                result += line + "\n";
                line.clear();
            }
        }
        if (!line.empty()) line += " ";
        line += word;
    }
    if (!line.empty()) result += line;
    return result;
}

void QuizGame::loadQuestions() {
    questions.clear();

    Question q1;
    q1.text = "Which bus route passes by the Student Center?";
    q1.options = {"Line 1", "Line 2", "Line 3"};
    q1.correctIndex = 1;
    questions.push_back(q1);

    Question q2;
    q2.text = "When is the time for the library's closing music to be played?";
    q2.options = {"23:00 PM", "23:15 PM", "23:25 PM"};
    q2.correctIndex = 2;
    questions.push_back(q2);

    Question q3;
    q3.text = "Which building houses the AI department?";
    q3.options = {"TX Building", "Le Tian Building", "ResearchA Building"};
    q3.correctIndex = 0;
    questions.push_back(q3);

    totalQuestions = questions.size();
}

bool QuizGame::loadQuestionsFromFile(const std::string& path) {
    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            std::cerr << "QuizGame: failed to open questions file: " << path << std::endl;
            return false;
        }
        nlohmann::json j;
        ifs >> j;

        // UI config (optional)
        if (j.contains("ui") && j["ui"].is_object()) {
            auto ui = j["ui"];
            uiWindowW = ui.value("windowWidth", uiWindowW);
            uiWindowH = ui.value("windowHeight", uiWindowH);
            if (ui.contains("backgroundColor") && ui["backgroundColor"].is_array() && ui["backgroundColor"].size() >= 3) {
                int r = ui["backgroundColor"][0].get<int>();
                int g = ui["backgroundColor"][1].get<int>();
                int b = ui["backgroundColor"][2].get<int>();
                int a = 255;
                if (ui["backgroundColor"].size() >= 4) a = ui["backgroundColor"][3].get<int>();
                uiBackgroundColor = sf::Color(static_cast<unsigned char>(r),
                                              static_cast<unsigned char>(g),
                                              static_cast<unsigned char>(b),
                                              static_cast<unsigned char>(a));
            }
        }

        // categories: if present, pick one category at random and sample up to 5 questions from it
        questions.clear();
        if (j.contains("categories") && j["categories"].is_object()) {
            auto cats = j["categories"];
            std::vector<std::string> keys;
            for (auto it = cats.begin(); it != cats.end(); ++it) keys.push_back(it.key());
            if (!keys.empty()) {
                // choose category randomly
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<size_t> dist(0, keys.size() - 1);
                std::string chosen = keys[dist(gen)];
                Logger::info("QuizGame: chosen category = " + chosen);

                const auto& qarr = cats[chosen];
                if (qarr.is_array()) {
                    // build index vector and shuffle for sampling
                    std::vector<size_t> idx(qarr.size());
                    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
                    std::shuffle(idx.begin(), idx.end(), gen);
                    size_t take = std::min<size_t>(5, qarr.size());
                    for (size_t t = 0; t < take; ++t) {
                        const auto& qj = qarr[idx[t]];
                        if (!qj.is_object()) continue;
                        Question q;
                        q.text = qj.value("text", std::string());
                        if (qj.contains("options") && qj["options"].is_array()) {
                            for (const auto& opt : qj["options"]) q.options.push_back(opt.get<std::string>());
                        }
                        q.correctIndex = qj.value("correctIndex", 0u);
                        questions.push_back(std::move(q));
                    }
                }
            }
        } else {
            // fallback to flat questions array (backwards compatibility)
            if (j.contains("questions") && j["questions"].is_array()) {
                for (const auto& qj : j["questions"]) {
                    if (!qj.is_object()) continue;
                    Question q;
                    q.text = qj.value("text", std::string());
                    if (qj.contains("options") && qj["options"].is_array()) {
                        for (const auto& opt : qj["options"]) q.options.push_back(opt.get<std::string>());
                    }
                    q.correctIndex = qj.value("correctIndex", 0u);
                    questions.push_back(std::move(q));
                }
            }
        }

        // optional effects (exp / energy)
        if (j.contains("effects") && j["effects"].is_object()) {
            auto effs = j["effects"];
            if (effs.contains("perfect") && effs["perfect"].is_object()) {
                auto p = effs["perfect"];
                perfectEffect.exp = p.value("exp", perfectEffect.exp);
                perfectEffect.energy = p.value("energy", perfectEffect.energy);
            }
            if (effs.contains("good") && effs["good"].is_object()) {
                auto g = effs["good"];
                goodEffect.exp = g.value("exp", goodEffect.exp);
                goodEffect.energy = g.value("energy", goodEffect.energy);
            }
            if (effs.contains("poor") && effs["poor"].is_object()) {
                auto p = effs["poor"];
                poorEffect.exp = p.value("exp", poorEffect.exp);
                poorEffect.energy = p.value("energy", poorEffect.energy);
            }
        }

        totalQuestions = questions.size();
        return true;
    } catch (const std::exception& ex) {
        std::cerr << "QuizGame: exception parsing JSON: " << ex.what() << std::endl;
        return false;
    }
}

void QuizGame::displayCurrentQuestion() {
    options.clear();
    resultText.setString("");
    answered = false;
    showContinueButton = false;

    if (currentQuestionIndex >= questions.size()) return;

    const Question& cur = questions[currentQuestionIndex];
    questionText.setString(wrapText(cur.text, 50));

    const float startX = 100.f;
    const float startY = 200.f;
    const float width = 600.f;
    const float height = 60.f;
    const float gap = 20.f;

    for (size_t i = 0; i < cur.options.size(); ++i) {
        float y = startY + i * (height + gap);
        options.emplace_back(font, cur.options[i], sf::Vector2f(startX, y), sf::Vector2f(width, height));
    }
}

void QuizGame::updateScoreDisplay() {
    std::ostringstream ss;
    ss << "Question: " << (currentQuestionIndex + 1) << "/" << totalQuestions
       << "  Correct: " << correctAnswers;
    scoreText.setString(ss.str());
}

// ---------------- 构造函数 ----------------
QuizGame::QuizGame()
    : font()
    , titleText(font)
    , questionText(font)
    , resultText(font)
    , scoreText(font)
    , continueText(font)
    , continueButton()
    , currentQuestionIndex(0)
    , totalQuestions(0)
    , correctAnswers(0)
    , answered(false)
    , gameCompleted(false)
    , showContinueButton(false)
{
    // default effects (can be overridden by JSON)
    perfectEffect.exp = 20; perfectEffect.energy = 10;
    goodEffect.exp = 10;    goodEffect.energy = 5;
    poorEffect.exp = 0;     poorEffect.energy = -5;
    lastEffect.exp = 0;     lastEffect.energy = 0;

    // 默认窗口尺寸由 uiWindowW/uiWindowH 提供（可由 JSON 覆盖）
    window.create(sf::VideoMode({uiWindowW, uiWindowH}), "Campus Quiz Game");

    // 尝试加载字体（SFML 3: openFromFile）
    bool fontLoaded = false;
    const std::vector<std::string> fontPaths = {"./fonts/arial.ttf", "fonts/arial.ttf", "arial.ttf"};
    for (const auto& p : fontPaths) {
        if (font.openFromFile(p)) {
            std::cout << "Loaded font: " << p << std::endl;
            fontLoaded = true;
            break;
        }
    }
    if (!fontLoaded) {
        std::cerr << "Warning: failed to load font. Text output may be invisible or fallback.\n";
    }

    // 标题
    titleText.setString("Campus Knowledge Quiz");
    titleText.setCharacterSize(32);
    titleText.setFillColor(sf::Color(255, 215, 0));
    titleText.setStyle(sf::Text::Bold);
    titleText.setPosition(sf::Vector2f(20.f, 15.f));

    // 问题文本
    questionText.setCharacterSize(24);
    questionText.setFillColor(sf::Color(240, 240, 240));
    questionText.setStyle(sf::Text::Bold);
    questionText.setPosition(sf::Vector2f(60.f, 120.f));

    // 结果文本
    resultText.setCharacterSize(28);
    resultText.setFillColor(sf::Color::Green);
    resultText.setPosition(sf::Vector2f(60.f, 480.f));

    // 分数文本
    scoreText.setCharacterSize(20);
    scoreText.setFillColor(sf::Color(200, 200, 100));
    scoreText.setStyle(sf::Text::Bold);
    scoreText.setPosition(sf::Vector2f(520.f, 25.f));

    // continue 按钮
    continueButton.setSize(sf::Vector2f(250.f, 48.f));
    continueButton.setFillColor(sf::Color(100, 200, 100));
    continueButton.setOutlineColor(sf::Color(80, 180, 80));
    continueButton.setOutlineThickness(2.f);
    continueButton.setPosition(sf::Vector2f(500.f, 500.f));

    continueText.setString("Continue to the next");
    continueText.setCharacterSize(20);
    continueText.setStyle(sf::Text::Bold);
    continueText.setFillColor(sf::Color::White);
    continueText.setPosition(sf::Vector2f(continueButton.getPosition().x + 20.f,
                                          continueButton.getPosition().y + 8.f));

    // 加载题目并显示第一题（默认硬编码）
    loadQuestions();
    displayCurrentQuestion();
    updateScoreDisplay();
}

QuizGame::QuizGame(const std::string& jsonPath)
    : font()
    , titleText(font)
    , questionText(font)
    , resultText(font)
    , scoreText(font)
    , continueText(font)
    , continueButton()
    , currentQuestionIndex(0)
    , totalQuestions(0)
    , correctAnswers(0)
    , answered(false)
    , gameCompleted(false)
    , showContinueButton(false)
{
    // default effects (can be overridden by JSON)
    perfectEffect.exp = 20; perfectEffect.energy = 10;
    goodEffect.exp = 10;    goodEffect.energy = 5;
    poorEffect.exp = 0;     poorEffect.energy = -5;
    lastEffect.exp = 0;     lastEffect.energy = 0;

    // 先尝试从文件加载 UI/题目；如果失败则回退到硬编码
    bool loaded = loadQuestionsFromFile(jsonPath);
    // 根据可能来自 JSON 的 uiWindowW/uiWindowH 创建窗口
    window.create(sf::VideoMode({uiWindowW, uiWindowH}), "Campus Quiz Game");

    // 尝试加载字体
    bool fontLoaded = false;
    const std::vector<std::string> fontPaths = {"./fonts/arial.ttf", "fonts/arial.ttf", "arial.ttf"};
    for (const auto& p : fontPaths) {
        if (font.openFromFile(p)) {
            std::cout << "Loaded font: " << p << std::endl;
            fontLoaded = true;
            break;
        }
    }
    if (!fontLoaded) {
        std::cerr << "Warning: failed to load font. Text output may be invisible or fallback.\n";
    }

    // 标题
    titleText.setString("Campus Knowledge Quiz");
    titleText.setCharacterSize(32);
    titleText.setFillColor(sf::Color(255, 215, 0));
    titleText.setStyle(sf::Text::Bold);
    titleText.setPosition(sf::Vector2f(20.f, 15.f));

    // 问题文本
    questionText.setCharacterSize(24);
    questionText.setFillColor(sf::Color(240, 240, 240));
    questionText.setStyle(sf::Text::Bold);
    questionText.setPosition(sf::Vector2f(60.f, 120.f));

    // 结果文本
    resultText.setCharacterSize(28);
    resultText.setFillColor(sf::Color::Green);
    resultText.setPosition(sf::Vector2f(60.f, 480.f));

    // 分数文本
    scoreText.setCharacterSize(20);
    scoreText.setFillColor(sf::Color(200, 200, 100));
    scoreText.setStyle(sf::Text::Bold);
    scoreText.setPosition(sf::Vector2f(520.f, 25.f));

    // continue 按钮
    continueButton.setSize(sf::Vector2f(250.f, 48.f));
    continueButton.setFillColor(sf::Color(100, 200, 100));
    continueButton.setOutlineColor(sf::Color(80, 180, 80));
    continueButton.setOutlineThickness(2.f);
    continueButton.setPosition(sf::Vector2f(500.f, 500.f));

    continueText.setString("Continue to the next");
    continueText.setCharacterSize(20);
    continueText.setStyle(sf::Text::Bold);
    continueText.setFillColor(sf::Color::White);
    continueText.setPosition(sf::Vector2f(continueButton.getPosition().x + 20.f,
                                          continueButton.getPosition().y + 8.f));

    if (!loaded) {
        // 回退到内置题库
        loadQuestions();
    }
    displayCurrentQuestion();
    updateScoreDisplay();
}

// ---------------- run() ----------------
void QuizGame::run() {
    while (window.isOpen()) {
        // 使用 SFML 3 的 pollEvent() 返回 optional<Event>
        for (auto ev = window.pollEvent(); ev.has_value(); ev = window.pollEvent()) {
            const auto& e = ev.value();

            // 关闭事件
            if (e.is<sf::Event::Closed>()) {
                window.close();
            }

            // 鼠标按下事件
            if (auto* mouseEvent = e.getIf<sf::Event::MouseButtonPressed>()) {
                if (mouseEvent->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mousePos(static_cast<float>(mouseEvent->position.x),
                                          static_cast<float>(mouseEvent->position.y));

                    // 未答题 -> 选择答案
                    if (!answered && !gameCompleted) {
                        for (size_t i = 0; i < options.size(); ++i) {
                            if (options[i].isClicked(mousePos)) {
                                const Question& cur = questions[currentQuestionIndex];
                                if (i == cur.correctIndex) {
                                    resultText.setString("Correct! Well done!");
                                    resultText.setFillColor(sf::Color(0, 200, 0));
                                    ++correctAnswers;
                                } else {
                                    std::string msg = "Wrong! Correct: " + cur.options[cur.correctIndex];
                                    resultText.setString(msg);
                                    resultText.setFillColor(sf::Color(220, 0, 0));
                                }
                                answered = true;
                                showContinueButton = true;
                                updateScoreDisplay();
                                break;
                            }
                        }
                    }
                    // 已答且显示继续按钮 -> 点击继续
                    else if (answered && !gameCompleted && showContinueButton) {
                        if (continueButton.getGlobalBounds().contains(mousePos)) {
                            showContinueButton = false;
                            ++currentQuestionIndex;
                            if (currentQuestionIndex < totalQuestions) {
                                displayCurrentQuestion();
                                updateScoreDisplay();
                            } else {
                                // 游戏结束
                                gameCompleted = true;
                                questionText.setString("Quiz Completed!");
                                questionText.setFillColor(sf::Color(255, 215, 0));
                                questionText.setCharacterSize(36);
                                questionText.setPosition(sf::Vector2f(220.f, 150.f));

                                std::string finalMessage;
                                if (correctAnswers == totalQuestions) {
                                    finalMessage = "Perfect Score!\nFinal Score: " + std::to_string(correctAnswers) +
                                                   "/" + std::to_string(totalQuestions) + "\nClick to close";
                                } else if (correctAnswers >= totalQuestions / 2) {
                                    finalMessage = "Good Job!\nFinal Score: " + std::to_string(correctAnswers) +
                                                   "/" + std::to_string(totalQuestions) + "\nClick to close";
                                } else {
                                    finalMessage = "Keep Practicing!\nFinal Score: " + std::to_string(correctAnswers) +
                                                   "/" + std::to_string(totalQuestions) + "\nClick to close";
                                }

                                resultText.setString(finalMessage);
                                resultText.setFillColor(sf::Color::White);
                                resultText.setCharacterSize(28);
                                resultText.setPosition(sf::Vector2f(200.f, 220.f));
                                // determine effects based on performance
                                if (correctAnswers == totalQuestions) {
                                    lastEffect = perfectEffect;
                                } else if (correctAnswers >= totalQuestions / 2) {
                                    lastEffect = goodEffect;
                                } else {
                                    lastEffect = poorEffect;
                                }
                                options.clear();
                                showContinueButton = false;
                            }
                        }
                    }
                    // 游戏结束后点击 -> 关闭这个窗口（不会影响 main 的其他窗口）
                    else if (gameCompleted) {
                        window.close();
                    }
                }
            } // end mouse event
        } // end polling events

        // 绘制
        window.clear(sf::Color(30, 30, 60));

        sf::RectangleShape titleBg(sf::Vector2f(800.f, 60.f));
        titleBg.setFillColor(sf::Color(40, 40, 80));
        titleBg.setPosition(sf::Vector2f(0.f, 0.f));
        window.draw(titleBg);

        sf::RectangleShape contentBg(sf::Vector2f(780.f, 500.f));
        contentBg.setFillColor(sf::Color(50, 50, 70, 180));
        contentBg.setPosition(sf::Vector2f(10.f, 70.f));
        window.draw(contentBg);

        window.draw(titleText);
        window.draw(questionText);
        window.draw(resultText);
        window.draw(scoreText);

        for (auto& opt : options) {
            window.draw(opt.shape);
            window.draw(opt.text);
        }

        if (showContinueButton) {
            window.draw(continueButton);
            window.draw(continueText);
        }

        window.display();
    } // end window loop
}
