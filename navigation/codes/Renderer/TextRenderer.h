// TextRenderer.h
#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <memory>
#include "MapLoader/MapObjects.h"

/**
 * @file TextRenderer.h
 * @brief Utilities for rendering MapObjects::TextObject to an SFML window.
 *
 * The TextRenderer wraps an SFML font and provides helper routines to create
 * styled sf::Text instances from TextObject descriptors and to draw them with
 * simple outline styling for readability on complex maps.
 */
/**
 * @class TextRenderer
 * @brief Handles text rendering for building names and other text objects.
 */
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();
    
    bool initialize(const std::string& fontPath);
    void cleanup();
    
    void renderTextObjects(
        const std::vector<TextObject>& textObjects, 
        sf::RenderWindow& window
    );

    void renderText(
        const TextObject& textObj, 
        sf::RenderWindow& window
    );
    
    bool isFontLoaded() const { return fontLoaded; }

private:
    std::unique_ptr<sf::Font> font;
    bool fontLoaded = false;
    
    sf::Text createText(const TextObject& textObj);

    void applyTextAlignment(
        sf::Text& text, 
        const TextObject& textObj
    );
};