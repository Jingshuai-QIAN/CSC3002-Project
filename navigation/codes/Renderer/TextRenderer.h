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
    /**
     * @brief Constructs a new TextRenderer object.
     */
    TextRenderer();

    /**
     * @brief Destroys the TextRenderer object and cleans up resources.
     */
    ~TextRenderer();

    /**
     * @brief Initialize text renderer with specified font.
     * 
     * @param fontPath Path to font file to load.
     * @return true if font loaded successfully, false otherwise.
     */
    bool initialize(const std::string& fontPath);

     /**
     * @brief Clean up resources used by text renderer.
     */
    void cleanup();

    /**
     * @brief Render multiple text objects.
     * 
     * @param textObjects Vector of text objects to render.
     * @param window Render window to draw text to.
     */
    void renderTextObjects(
        const std::vector<TextObject>& textObjects, 
        sf::RenderWindow& window
    );

    /**
     * @brief Render single text object.
     * 
     * @param textObj Text object to render.
     * @param window Render window to draw text to.
     */
    void renderText(
        const TextObject& textObj, 
        sf::RenderWindow& window
    );

    /**
     * @brief Check if font is loaded.
     * 
     * @return true if font is loaded, false otherwise.
     */
    bool isFontLoaded() const { return fontLoaded; }

private:
    std::unique_ptr<sf::Font> font;    ///< Font used for text rendering
    bool fontLoaded = false;    ///< Flag indicating whether font is loaded

    /**
     * @brief Create sf::Text from TextObject descriptor.
     * 
     * @param textObj Text object descriptor.
     * @return sf::Text Configured text object.
     */
    sf::Text createText(const TextObject& textObj);

    /**
     * @brief Apply horizontal and vertical alignment to text.
     * 
     * @param text Text object to apply alignment to.
     * @param textObj Text object descriptor containing alignment settings.
     */
    void applyTextAlignment(
        sf::Text& text, 
        const TextObject& textObj
    );

};
