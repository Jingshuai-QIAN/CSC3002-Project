#include "DialogSystem.h"
#include <SFML/Graphics.hpp>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "Utils/Logger.h"

/**
 * @brief Wrap text to fit within specified maximum width.
 * 
 * @param str Input text string.
 * @param font Font used for text rendering.
 * @param characterSize Font character size.
 * @param maxWidth Maximum allowed width in pixels.
 * @return std::string Wrapped text string.
 */
static std::string wrapText(
    const std::string& str,
    const sf::Font& font,
    unsigned int characterSize,
    float maxWidth
) {
    sf::Text measure(font, "", characterSize);
    float spaceWidth = measure.getFont().getGlyph(' ', characterSize, false).advance;

    std::string result;
    std::string word;
    float lineWidth = 0.f;

    auto wordWidthCalc = [&](const std::string& w)->float {
        float ww = 0.f;
        for (unsigned char ch : w) {
            ww += measure.getFont().getGlyph(ch, characterSize, false).advance;
        }
        return ww;
    };

    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (c == ' ' || c == '\n') {
            if (!word.empty()) {
                float w = wordWidthCalc(word);
                if (lineWidth > 0.f && lineWidth + w > maxWidth) {
                    result.push_back('\n');
                    lineWidth = 0.f;
                } else if (lineWidth > 0.f) {
                    result.push_back(' ');
                    lineWidth += spaceWidth;
                }
                result += word;
                lineWidth += w;
                word.clear();
            }
            if (c == '\n') {
                result.push_back('\n');
                lineWidth = 0.f;
            }
        } else {
            word.push_back(c);
        }
    }

    if (!word.empty()) {
        float w = wordWidthCalc(word);
        if (lineWidth > 0.f && lineWidth + w > maxWidth) {
            result.push_back('\n');
        } else if (lineWidth > 0.f) {
            result.push_back(' ');
        }
        result += word;
    }

    return result;
}

/**
 * @brief Initialize dialog system with textures and fonts.
 * 
 * @param bgPath Path to dialog background texture.
 * @param btnPath Path to button texture.
 * @param font Font for dialog text.
 * @param fontSize Font size for dialog text.
 * @throws std::runtime_error if textures fail to load.
 */
void DialogSystem::initialize(const std::string& bgPath, 
                              const std::string& btnPath, 
                              const sf::Font& font, 
                              unsigned int fontSize) {
    m_fontSize = fontSize;
    m_font = font; 

    // Load background texture + create texture-aware Sprite
    if (!m_bgTexture.loadFromFile(bgPath)) {
        throw std::runtime_error("Failed to load dialog bg: " + bgPath);
    }

    m_dialogBgSprite = std::make_unique<sf::Sprite>(m_bgTexture);
    m_dialogSize = sf::Vector2f(m_bgTexture.getSize().x, m_bgTexture.getSize().y);
    
    // Load button texture + create texture-aware Sprite
    if (!m_btnTexture.loadFromFile(btnPath)) {
        throw std::runtime_error("Failed to load dialog btn: " + btnPath);
    }
    m_btnSprite = std::make_unique<sf::Sprite>(m_btnTexture);
    m_btnTex = m_btnTexture;
    m_btnSize = sf::Vector2f(m_btnTexture.getSize().x, m_btnTexture.getSize().y);
    
    m_dialogTitle.setFont(m_font);
    m_dialogTitle.setCharacterSize(m_fontSize);
    m_dialogTitle.setFillColor(sf::Color::White);
    m_dialogTitle.setLineSpacing(1.2f); 
}

/**
 * @brief Calculate centered position for dialog (legacy method).
 * 
 * @param window Render window reference.
 * @return sf::Vector2f Centered dialog position.
 */
sf::Vector2f DialogSystem::getCenteredPosition(const sf::RenderWindow& window) {
    if (!m_dialogBgSprite) return {0, 0}; 

    sf::Vector2u windowSize = window.getSize();
    float windowW = static_cast<float>(windowSize.x);
    float windowH = static_cast<float>(windowSize.y);
    
    float scaleFactor = windowW * 0.6f / m_dialogSize.x;
    m_dialogBgSprite->setScale(sf::Vector2f(scaleFactor, scaleFactor)); 
    
    sf::Vector2f scaledDialogSize = sf::Vector2f(
        m_dialogSize.x * scaleFactor,
        m_dialogSize.y * scaleFactor
    );
    
    float posX = (windowW - scaledDialogSize.x) / 2.0f;
    float posY = (windowH - scaledDialogSize.y) / 2.0f;
    
    return sf::Vector2f(posX, posY);
}

/**
 * @brief Calculate centered position for dialog (improved method).
 * 
 * @param window Render window reference.
 * @return sf::Vector2f Centered dialog position.
 */
sf::Vector2f DialogSystem::getDialogCenterPosition(const sf::RenderWindow& window) {
    if (!m_dialogBgSprite || m_bgTexture.getSize().x == 0) {
        std::cerr << "Dialog sprite/texture not initialized!" << std::endl;
        return {0, 0};
    }

    // Get default view dimensions (full screen)
    sf::Vector2u windowSize = window.getSize();
    float windowW = static_cast<float>(windowSize.x);
    float windowH = static_cast<float>(windowSize.y);

    // Force scaling: dialog width = 50% of window width 
    float targetWidth = windowW * 0.5f;
    sf::Vector2u texSize = m_bgTexture.getSize();
    float originalW = static_cast<float>(texSize.x);
    float originalH = static_cast<float>(texSize.y);
    float scaleFactor = targetWidth / originalW;

    scaleFactor = std::max(0.3f, std::min(scaleFactor, 1.0f));

    // Apply scaling
    m_dialogBgSprite->setScale(sf::Vector2f(scaleFactor, scaleFactor));

    // Calculate absolute centering (window center - dialog center)
    sf::FloatRect scaledBounds = m_dialogBgSprite->getGlobalBounds();
    float posX = (windowW - scaledBounds.size.x) / 2.0f;
    float posY = (windowH - scaledBounds.size.y) / 2.0f;

    return {posX, posY};
}

/**
 * @brief Get dialog background size.
 * 
 * @return sf::Vector2f Dialog background size (scaled).
 */
sf::Vector2f DialogSystem::getDialogBgSize() const {
    if (!m_dialogBgSprite) {
        return {450, 300}; 
    }

    const sf::FloatRect bgBounds = m_dialogBgSprite->getGlobalBounds();
    return {bgBounds.size.x, bgBounds.size.y};
}

/**
 * @brief Set dialog with index-based callback.
 * 
 * @param title Dialog title text.
 * @param options List of option strings.
 * @param selectCallback Callback function receiving option index and text.
 */
void DialogSystem::setDialogWithIndex(const std::string& title,
                                      const std::vector<std::string>& options,
                                      const OptionCallback& selectCallback) {
    m_isActive = true;
    m_optionCallback = selectCallback;
    m_simpleCallback = nullptr;  
    m_useIndexCallback = true;   
    m_buttons.clear();

    m_dialogTitle.setFont(m_font);
    m_dialogTitle.setCharacterSize(m_fontSize);
    m_dialogTitle.setFillColor(sf::Color::White);
    m_dialogTitle.setLineSpacing(1.2f);
    m_dialogTitle.setString(title);

    // Create buttons - using index-based callback
    for (size_t i = 0; i < options.size(); ++i) {
        const auto& option = options[i];
        Button btn(m_font, m_fontSize);
        btn.text = option;
        
        // Index-based callback: pass option index and text
        btn.callback = [selectCallback, i, option]() {
            if (selectCallback) {
                selectCallback(static_cast<int>(i), option);
            }
        };

        btn.textObj.setFont(m_font);
        btn.textObj.setCharacterSize(m_fontSize);
        btn.textObj.setString(option);
        btn.textObj.setFillColor(sf::Color::Black);

        btn.sprite = std::make_unique<sf::Sprite>(m_btnTex);
        m_buttons.push_back(std::move(btn));
    }
}

/**
 * @brief Set dialog with text-based callback.
 * 
 * @param title Dialog title text.
 * @param dishOptions List of dish option strings.
 * @param selectCallback Callback function receiving selected text.
 */
void DialogSystem::setDialog(const std::string& title,
                             const std::vector<std::string>& dishOptions,
                             const std::function<void(const std::string&)>& selectCallback) {
    m_isActive = true;
    m_simpleCallback = selectCallback; 
    m_optionCallback = nullptr; 
    m_useIndexCallback = false;  
    m_buttons.clear();

    if (!m_dialogBgSprite) return; 

    sf::Vector2f dummyWindowSize(1200, 800); 
    float targetWidth = dummyWindowSize.x * 0.5f;
    float scaleFactor = targetWidth / static_cast<float>(m_bgTexture.getSize().x);
    scaleFactor = std::max(0.3f, std::min(scaleFactor, 1.0f));
    m_dialogBgSprite->setScale(sf::Vector2f(scaleFactor, scaleFactor));
    
    sf::Vector2f centeredPos(
        (dummyWindowSize.x - m_dialogBgSprite->getGlobalBounds().size.x) / 2,
        (dummyWindowSize.y - m_dialogBgSprite->getGlobalBounds().size.y) / 2
    );
    m_dialogBgSprite->setPosition(centeredPos);

    // Set title
    m_dialogTitle.setFont(m_font);
    m_dialogTitle.setString(title);
    m_dialogTitle.setCharacterSize(m_fontSize);
    m_dialogTitle.setFillColor(sf::Color::White);
    m_dialogTitle.setLineSpacing(1.2f);

    // Get background position and dimensions
    sf::Vector2f dialogPos = m_dialogBgSprite->getPosition();
    sf::Vector2f bgSize = getDialogBgSize();

    // Create dish buttons
    for (const auto& dish : dishOptions) {
        Button btn(m_font, m_fontSize);
        btn.text = dish;
        
        // Simple callback: only pass text
        btn.callback = [this, dish]() {
            if (m_simpleCallback) {
                m_simpleCallback(dish);
            }
        };

        // Set button text
        btn.textObj.setFont(m_font);
        btn.textObj.setCharacterSize(m_fontSize);
        btn.textObj.setString(dish);
        btn.textObj.setFillColor(sf::Color::Black);
        
        // Create button Sprite
        btn.sprite = std::make_unique<sf::Sprite>(m_btnTex);
        
        m_buttons.push_back(std::move(btn));
    }

    // Layout buttons
    layoutButtons();
}

/**
 * @brief Layout buttons based on dialog position and title.
 */
void DialogSystem::layoutButtons() {
    if (m_buttons.empty() || !m_dialogBgSprite) return;

    sf::Vector2f dialogPos = m_dialogBgSprite->getPosition();
    sf::Vector2f bgSize = getDialogBgSize();

    float btnSpacing = 15.0f;
    float btnWidthRatio = 0.7f;
    float btnTopOffset = 80.0f; 
    
    if (!m_dialogTitle.getString().isEmpty()) {
        sf::FloatRect titleGlobal = m_dialogTitle.getGlobalBounds();
        float titleBottom = titleGlobal.position.y + titleGlobal.size.y;
        btnTopOffset = (titleBottom - dialogPos.y) + 20.0f; 
    }

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        Button& btn = m_buttons[i];
        if (!btn.sprite) continue;

        float targetBtnWidth = bgSize.x * btnWidthRatio;
        sf::Vector2u btnTexSize = m_btnTexture.getSize();
        float btnScale = targetBtnWidth / static_cast<float>(btnTexSize.x);
        btnScale = std::clamp(btnScale, 0.5f, 1.0f);
        btn.sprite->setScale(sf::Vector2f(btnScale, btnScale));

        sf::FloatRect btnBounds = btn.sprite->getGlobalBounds();
        float btnX = dialogPos.x + (bgSize.x - btnBounds.size.x) / 2.0f;
        float btnY = dialogPos.y + btnTopOffset + i * (btnBounds.size.y + btnSpacing);

        btn.sprite->setPosition(sf::Vector2f(btnX, btnY));
        btn.bounds = btn.sprite->getGlobalBounds();

        sf::FloatRect textBounds = btn.textObj.getLocalBounds();
        btn.textObj.setOrigin(sf::Vector2f(
            textBounds.position.x + textBounds.size.x / 2.0f,
            textBounds.position.y + textBounds.size.y / 2.0f
        ));
        btn.textObj.setPosition(sf::Vector2f(
            btnX + btnBounds.size.x / 2.0f,
            btnY + btnBounds.size.y / 2.0f
        ));
    }
}

/**
 * @brief Handle SFML events for dialog system.
 * 
 * @param event SFML event to process.
 * @param window Render window reference.
 */
void DialogSystem::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!m_isActive) return;

    // Handle left mouse button click
    if (event.is<sf::Event::MouseButtonPressed>()) {
        const auto& mouseEvent = *event.getIf<sf::Event::MouseButtonPressed>();
        if (mouseEvent.button == sf::Mouse::Button::Left) {
            // Get raw mouse pixel coordinates
            sf::Vector2i mousePixelPos(mouseEvent.position.x, mouseEvent.position.y);
            // Convert to window default view coordinates 
            sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos, window.getDefaultView());
            
            std::function<void()> pendingCallback = nullptr;

            // Iterate buttons to detect clicks
            for (auto& btn : m_buttons) {
                if (!btn.sprite) continue;
                if (btn.bounds.contains(mouseWorldPos)) {
                    if (btn.callback)
                        pendingCallback = btn.callback;

                    m_isActive = false;
                    break;
                }
            }

            if (pendingCallback) {
                m_buttons.clear();          
                m_pendingCallback = pendingCallback;  
            }
        }
    }

    // Handle ESC key to close
    if (event.is<sf::Event::KeyPressed>()) {
        const auto& keyEvent = *event.getIf<sf::Event::KeyPressed>();
        if (keyEvent.code == sf::Keyboard::Key::Escape) {
            m_isActive = false;
        }
    }

    if (event.is<sf::Event::MouseMoved>()) {
        const auto& mouseMoveEvent = *event.getIf<sf::Event::MouseMoved>();
        sf::Vector2i mousePixelPos(mouseMoveEvent.position.x, mouseMoveEvent.position.y);
        sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos, window.getDefaultView());
        
        for (auto& btn : m_buttons) {
            if (!btn.sprite) continue;
            if (btn.bounds.contains(mouseWorldPos)) {
                btn.sprite->setColor(sf::Color(220, 220, 220)); 
            } else {
                btn.sprite->setColor(sf::Color::White); 
            }
        }
    }
}

/**
 * @brief Check if there's a pending callback to execute.
 * 
 * @return true if pending callback exists, false otherwise.
 */
bool DialogSystem::hasPendingCallback() const {
    return m_pendingCallback != nullptr;
}

/**
 * @brief Consume and return pending callback.
 * 
 * @return std::function<void()> Pending callback function.
 */
std::function<void()> DialogSystem::consumePendingCallback() {
    auto callback = m_pendingCallback;
    m_pendingCallback = nullptr;
    return callback;
}

/**
 * @brief Close dialog and clear all resources.
 */
void DialogSystem::close() {
    m_isActive = false;
    m_buttons.clear();
    m_simpleCallback = nullptr;
    m_optionCallback = nullptr;
    m_pendingCallback = nullptr;  
}

/**
 * @brief Render dialog to window.
 * 
 * @param window Render window reference.
 */
void DialogSystem::render(sf::RenderWindow& window) {
    if (!m_isActive || !m_dialogBgSprite) return;
    sf::View originalView = window.getView(); 
    window.setView(window.getDefaultView()); 

    sf::Vector2f centeredPos = getDialogCenterPosition(window);
    m_dialogBgSprite->setPosition(centeredPos);
    
    sf::Vector2f bgSize = getDialogBgSize();
    
    float padding = 30.f;
    float maxTextWidth = bgSize.x - padding * 2.0f;
    std::string currentTitle = m_dialogTitle.getString().toAnsiString();
    std::string wrappedTitle = wrapText(currentTitle, m_font, m_fontSize, maxTextWidth);
    m_dialogTitle.setString(wrappedTitle);
    
    sf::FloatRect titleBounds = m_dialogTitle.getLocalBounds();
    
    m_dialogTitle.setPosition(sf::Vector2f(
        centeredPos.x + bgSize.x / 2.0f,  
        centeredPos.y + 30.0f           
    ));
    
    m_dialogTitle.setOrigin(sf::Vector2f(
        titleBounds.size.x / 2.0f + titleBounds.position.x,
        titleBounds.size.y / 2.0f + titleBounds.position.y
    ));

    layoutButtons();

    window.draw(*m_dialogBgSprite);
    window.draw(m_dialogTitle);
    for (const auto& btn : m_buttons) {
        if (btn.sprite) {
            window.draw(*btn.sprite);
        }
        window.draw(btn.textObj);
    }

    window.setView(originalView);
}
