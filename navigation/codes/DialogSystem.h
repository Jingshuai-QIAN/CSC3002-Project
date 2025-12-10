#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <functional>

class DialogSystem {
public:
    /**
     * @struct Button
     * @brief Dialog button structure containing visual elements and callback.
     */
    struct Button {
        std::string text;
        std::function<void()> callback;
        std::unique_ptr<sf::Sprite> sprite;
        sf::Text textObj;
        sf::FloatRect bounds;
        bool isHovered = false;

        /**
         * @brief Construct a button with font and size.
         * 
         * SFML 3.0 requires explicit construction of Sprite/Text (no default constructors).
         * 
         * @param font Font for button text.
         * @param fontSize Font size for button text.
         */
        Button(const sf::Font& font, unsigned int fontSize)
            : textObj(font, "", fontSize)
        {}

        // Disable copy (unique_ptr is not copyable)
        Button(const Button&) = delete;
        Button& operator=(const Button&) = delete;

        // Enable move operations
        Button(Button&&) = default;
        Button& operator=(Button&&) = default;
    };

    // Legacy callback type (for cafeteria)
    using SimpleCallback = std::function<void(const std::string& optionText)>;
    
    // New callback type (for professor)
    using OptionCallback = std::function<void(int optionIndex, const std::string& optionText)>;

    /**
     * @brief DialogSystem constructor.
     * 
     * @param font Default font for dialog text.
     * @param fontSize Default font size for dialog text.
     */
    DialogSystem(const sf::Font& font, unsigned int fontSize)
        : m_font(font), m_fontSize(fontSize),
          m_dialogTitle(font, "", fontSize),
          m_textObject(font, "", fontSize),
          m_bgSprite(nullptr),    
          m_btnSprite(nullptr),
          m_dialogBgSprite(nullptr)
    {}


    /**
     * @brief Initialize dialog system with textures and fonts.
     * 
     * @param bgPath Path to dialog background texture.
     * @param btnPath Path to button texture.
     * @param font Font for dialog text.
     * @param fontSize Font size for dialog text.
     */
    void initialize(const std::string& bgPath, 
                    const std::string& btnPath, 
                    const sf::Font& font, 
                    unsigned int fontSize);

    /**
     * @brief Set dialog content (cafeteria aunt question + dish options + selection callback).
     * 
     * @param title Dialog title text.
     * @param dishOptions List of dish option strings.
     * @param selectCallback Callback function receiving selected dish text.
     */
    void setDialog(const std::string& title, 
                   const std::vector<std::string>& dishOptions,
                   const std::function<void(const std::string&)>& selectCallback);

    /**
     * @brief Set dialog with index-based callback (for professor dialogs).
     * 
     * @param title Dialog title text.
     * @param options List of option strings.
     * @param selectCallback Callback function receiving option index and text.
     */
    void setDialogWithIndex(const std::string& title, 
                           const std::vector<std::string>& options,
                           const OptionCallback& selectCallback);

    /**
     * @brief Handle mouse/keyboard events (hover/click/ESC close).
     * 
     * @param event SFML event to process.
     * @param window Render window reference.
     */
    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);

    /**
     * @brief Render dialog and buttons.
     * 
     * @param window Render window to draw to.
     */
    void render(sf::RenderWindow& window);

    // State control methods
    /**
     * @brief Check if dialog is active.
     * 
     * @return true if dialog is active, false otherwise.
     */
    bool isActive() const { return m_isActive; }

    /**
     * @brief Close dialog and clear resources.
     */
    void close();

    /**
     * @brief Check if dialog system is initialized.
     * 
     * @return true if dialog system is initialized, false otherwise.
     */
    bool isInitialized() const { 
        return m_dialogBgSprite != nullptr && m_bgTexture.getSize().x > 0; 
    }

    /**
     * @brief Check if there's a pending callback to execute.
     * 
     * @return true if pending callback exists, false otherwise.
     */
    bool hasPendingCallback() const;

    /**
     * @brief Consume and return pending callback.
     * 
     * @return std::function<void()> Pending callback function.
     */
    std::function<void()> consumePendingCallback();

private:
    sf::Vector2f getCenteredPosition(const sf::RenderWindow& window); 
    sf::Vector2f getDialogCenterPosition(const sf::RenderWindow& window); 
    sf::Vector2f getDialogBgSize() const; 
    void layoutButtons(); 


    bool m_isActive = false;
    sf::Font m_font;
    unsigned int m_fontSize;
    SimpleCallback m_simpleCallback;  
    OptionCallback m_optionCallback; 
    bool m_useIndexCallback = false;  
    std::function<void(const std::string&)> m_selectCallback;

    sf::Texture m_bgTexture;
    std::unique_ptr<sf::Sprite> m_bgSprite;
    sf::Vector2f m_dialogSize;
    std::unique_ptr<sf::Sprite> m_dialogBgSprite;

    sf::Texture m_btnTexture;
    std::unique_ptr<sf::Sprite> m_btnSprite;    
    sf::Vector2f m_btnSize;
    sf::Texture m_btnTex;

    sf::Text m_dialogTitle;       
    sf::Text m_textObject;       
    std::vector<sf::Text> m_optionTexts; 

    std::vector<Button> m_buttons;
    std::function<void()> m_pendingCallback;
};
