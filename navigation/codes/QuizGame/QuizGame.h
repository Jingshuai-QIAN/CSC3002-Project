// QuizGame.h

#ifndef QUIZ_GAME_H
#define QUIZ_GAME_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <optional>

class QuizGame {
public:
    // Public type for effects so callers can read result values
    struct Effects {
        int points = 0;
        int energy = 0;
    };
private:
    // 题目结构
    struct Question {
        std::string text;
        std::vector<std::string> options;
        size_t correctIndex;
    };

    // 选项按钮（嵌套类型）
    struct OptionButton {
        sf::RectangleShape shape;
        sf::Text text;

        OptionButton(const sf::Font& font, const std::string& str,
                     const sf::Vector2f& position, const sf::Vector2f& size);

        bool isClicked(const sf::Vector2f& mousePos) const;
        void setFont(const sf::Font& f); // 备用：若需要后续设置字体
    };

    // 窗口（对象持有）
    sf::RenderWindow window;

    // 字体和文本成员（注意顺序：font 在前，text 在后）
    sf::Font font;
    sf::Text titleText;
    sf::Text questionText;
    sf::Text resultText;
    sf::Text scoreText;
    sf::Text continueText;

    // 控件
    sf::RectangleShape continueButton;

    // 数据
    std::vector<OptionButton> options;
    std::vector<Question> questions;

    // 状态
    size_t currentQuestionIndex;
    size_t totalQuestions;
    size_t correctAnswers;
    bool answered;
    bool gameCompleted;
    bool showContinueButton;

    // 私有方法
    void loadQuestions();
    bool loadQuestionsFromFile(const std::string& path); // 从 JSON 文件加载题目与 UI 配置
    bool loadQuestionsFromFile(const std::string& path, const std::string& forcedCategory); // overload with forced category
    void displayCurrentQuestion();
    void updateScoreDisplay();
    std::string wrapText(const std::string& text, size_t lineLength) const;
    // UI config loaded from JSON (optional)
    unsigned int uiWindowW = 800;
    unsigned int uiWindowH = 600;
    // background color RGBA
    sf::Color uiBackgroundColor = sf::Color(30,30,60);
    // Effects for result (exp, energy)
    Effects perfectEffect;
    Effects goodEffect;
    Effects poorEffect;
    Effects lastEffect; // populated when quiz completes

public:
    QuizGame();
    explicit QuizGame(const std::string& jsonPath);
    // Load from jsonPath and force a specific category (if present in file)
    QuizGame(const std::string& jsonPath, const std::string& forcedCategory);
    ~QuizGame() = default;

    // 运行游戏（阻塞，直到此窗口关闭）
    void run();
    // After run() returns, caller can query resulting effects
    Effects getResultEffects() const { return lastEffect; }
};

#endif // QUIZ_GAME_H
