// TextRenderer.h
#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <memory>
#include "MapLoader/MapObjects.h"

/*
 * File: TextRenderer.h
 * Description: Utilities for rendering MapObjects::TextObject to an SFML window.
 *
 * The TextRenderer wraps an SFML font and provides helper routines to create
 * styled sf::Text instances from TextObject descriptors and to draw them with
 * simple outline styling for readability on complex maps.
 *
 * It exposes simple initialize/cleanup and render helpers used by the app.
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