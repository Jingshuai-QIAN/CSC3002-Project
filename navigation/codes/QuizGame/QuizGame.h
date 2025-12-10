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
    // Question structure
    struct Question {
        std::string text;
        std::vector<std::string> options;
        size_t correctIndex;
    };

    // Option button (nested type)
    struct OptionButton {
        sf::RectangleShape shape;
        sf::Text text;

        /**
         * @brief Construct an option button.
         * 
         * @param font Font for the button text.
         * @param str Text displayed on the button.
         * @param position Button position.
         * @param size Button size.
         */
        OptionButton(const sf::Font& font, const std::string& str,
                     const sf::Vector2f& position, const sf::Vector2f& size);

        /**
         * @brief Check if mouse click is within button bounds.
         * 
         * @param mousePos Mouse position.
         * @return true if button was clicked, false otherwise.
         */
        bool isClicked(const sf::Vector2f& mousePos) const;

        /**
         * @brief Set font for button text (fallback if needed later).
         * 
         * @param f Font to set.
         */
        void setFont(const sf::Font& f); 
    };

    // Window 
    sf::RenderWindow window;

    // Font and text members
    sf::Font font;
    sf::Text titleText;
    sf::Text questionText;
    sf::Text resultText;
    sf::Text scoreText;
    sf::Text continueText;

    // Controls
    sf::RectangleShape continueButton;

    // Data
    std::vector<OptionButton> options;
    std::vector<Question> questions;

    // State
    size_t currentQuestionIndex;
    size_t totalQuestions;
    size_t correctAnswers;
    bool answered;
    bool gameCompleted;
    bool showContinueButton;

    // Private methods
    void loadQuestions();
    bool loadQuestionsFromFile(const std::string& path); 
    bool loadQuestionsFromFile(const std::string& path, const std::string& forcedCategory); 
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
    Effects lastEffect; 

public:
    /**
     * @brief Default constructor with built-in questions.
     */
    QuizGame();

    /**
     * @brief Constructor that loads questions from JSON file.
     * 
     * @param jsonPath Path to JSON file containing questions and UI config.
     */
    explicit QuizGame(const std::string& jsonPath);

    /**
     * @brief Constructor that loads from JSON and forces specific category selection.
     * 
     * @param jsonPath Path to JSON file containing categorized questions.
     * @param forcedCategory Category name to force selection from.
     */
    QuizGame(const std::string& jsonPath, const std::string& forcedCategory);

    ~QuizGame() = default;

    /**
     * @brief Run the quiz game (blocking, until window closes).
     */
    void run();

    /**
     * @brief Get resulting effects after quiz completion.
     * 
     * @return Effects Points and energy changes from quiz.
     */
    Effects getResultEffects() const { return lastEffect; }
};

#endif // QUIZ_GAME_H
