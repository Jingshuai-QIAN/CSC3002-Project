// TextRenderer.cpp
#include "TextRenderer.h"
#include "Utils/Logger.h"
#include <filesystem>

/*
 * File: TextRenderer.cpp
 * Description: Implements text rendering helpers used to draw MapObjects::TextObject.
 *
 * The TextRenderer wraps font loading and provides routines to convert
 * TextObject descriptors into styled sf::Text instances and to render them
 * to an sf::RenderWindow.
 */

TextRenderer::TextRenderer() {
    font = std::make_unique<sf::Font>();
}


TextRenderer::~TextRenderer() {
    cleanup();
}

/**
 * @brief Initialize text renderer with specified font.
 * 
 * @param fontPath Path to font file to load.
 * @return true if font loaded successfully, false otherwise.
 */
bool TextRenderer::initialize(const std::string& fontPath) {
    cleanup();
    
    // Try to load the specified font
    if (font->openFromFile(fontPath)) {
        fontLoaded = true;
        Logger::info("TextRenderer initialized with font: " + fontPath);
        return true;
    }
    
    // Fallback font candidates
    std::vector<std::string> fallbackFonts = {
        "C:\\Windows\\Fonts\\arial.ttf",  // Windows
        "/System/Library/Fonts/Arial.ttf", // macOS
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf" // Linux
    };
    
    for (const auto& fallback : fallbackFonts) {
        if (std::filesystem::exists(fallback) && font->openFromFile(fallback)) {
            fontLoaded = true;
            Logger::info("TextRenderer using fallback font: " + fallback);
            return true;
        }
    }
    
    Logger::error("TextRenderer failed to load any font");
    return false;
}

/**
 * @brief Clean up resources used by text renderer.
 */
void TextRenderer::cleanup() {
    fontLoaded = false;
}

/**
 * @brief Render multiple text objects.
 * 
 * @param textObjects Vector of text objects to render.
 * @param window Render window to draw text to.
 */
void TextRenderer::renderTextObjects(
    const std::vector<TextObject>& textObjects, 
    sf::RenderWindow& window
) {
    if (!fontLoaded) return;
    
    for (const auto& textObj : textObjects) {
        renderText(textObj, window);
    }
}

/**
 * @brief Render single text object.
 * 
 * @param textObj Text object to render.
 * @param window Render window to draw text to.
 */
void TextRenderer::renderText(
    const TextObject& textObj, 
    sf::RenderWindow& window
) {
    if (!fontLoaded || textObj.text.empty()) return;
    
    sf::Text text = createText(textObj);
    applyTextAlignment(text, textObj);
    
    // Draw text outline for better visibility
    if (text.getOutlineThickness() > 0) {
        sf::Text outlineText = text;
        outlineText.setFillColor(sf::Color::Black);
        outlineText.setOutlineColor(sf::Color::Black);
        window.draw(outlineText);
    }
    
    window.draw(text);
}

/**
 * @brief Create sf::Text from TextObject descriptor.
 * 
 * @param textObj Text object descriptor.
 * @return sf::Text Configured text object.
 */
sf::Text TextRenderer::createText(const TextObject& textObj) {
    sf::Text text(*font, textObj.text, textObj.fontSize);
    text.setFillColor(textObj.color);
    
    // Set text style
    unsigned int style = sf::Text::Regular;
    if (textObj.bold) style |= sf::Text::Bold;
    if (textObj.italic) style |= sf::Text::Italic;
    text.setStyle(style);
    
    // Add outline for better readability on maps
    text.setOutlineColor(sf::Color(0, 0, 0, 160));
    text.setOutlineThickness(1.0f);
    
    return text;
}

/**
 * @brief Apply horizontal and vertical alignment to text.
 * 
 * @param text Text object to apply alignment to.
 * @param textObj Text object descriptor containing alignment settings.
 */
void TextRenderer::applyTextAlignment(
    sf::Text& text, 
    const TextObject& textObj
) {
    sf::FloatRect localBounds = text.getLocalBounds();
    float textWidth = localBounds.size.x;
    float textHeight = localBounds.size.y;
    
    float posX = textObj.x;
    float posY = textObj.y;
    sf::Vector2f origin(0.f, 0.f);
    
    // Horizontal alignment
    if (textObj.halign == "center") {
        posX += textObj.width * 0.5f;
        origin.x = textWidth * 0.5f;
    } else if (textObj.halign == "right") {
        posX += textObj.width;
        origin.x = textWidth;
    }
    
    // Vertical alignment  
    if (textObj.valign == "center") {
        posY += textObj.height * 0.5f;
        origin.y = textHeight * 0.5f;
    } else if (textObj.valign == "bottom") {
        posY += textObj.height;
        origin.y = textHeight;
    }
    
    text.setOrigin(origin);
    text.setPosition(sf::Vector2f{posX, posY});

}
