// QuizGame.h

#ifndef QUIZ_GAME_H
#define QUIZ_GAME_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

class QuizGame {
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
    void displayCurrentQuestion();
    void updateScoreDisplay();
    std::string wrapText(const std::string& text, size_t lineLength) const;

public:
    QuizGame();
    ~QuizGame() = default;

    // 运行游戏（阻塞，直到此窗口关闭）
    void run();
};

#endif // QUIZ_GAME_H
