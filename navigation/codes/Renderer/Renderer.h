// Renderer.h
#pragma once

// Standard library headers used by the renderer interface.
#include <string>
#include <vector>
#include <memory>

// SFML types for window, view and drawable primitives.
#include <SFML/Graphics.hpp>

// Project configuration types and object definitions.
#include "Config/ConfigManager.h"
#include "MapLoader/MapObjects.h"
#include "Renderer/TextRenderer.h"

/*
 * File: Renderer.h
 * Description: High-level rendering abstraction for the application.
 *
 * The Renderer provides window creation, event handling, and simplified
 * drawing helpers used across the engine. It owns an SFML window and a
 * TextRenderer instance used to draw building names.
 *
 * Important functions:
 *   - initialize: creates the window and initializes text rendering.
 *   - drawSprite / drawText / drawRectangle: simple drawing helpers.
 *
 * Notes:
 *   - The class stores textures loaded via loadTexture and returns raw pointers.
 *   - For TMJMap rendering, sprites should reference textures owned by TMJMap.
 */


/*
 * Class: Renderer
 * Description: Handles rendering operations and graphics management.
 *
 * The Renderer class is responsible for initializing the graphics system,
 * managing textures, and handling rendering operations for the application.
 */
class Renderer {
public:
    /**
     * @brief Constructs a new Renderer object.
     */
    Renderer();
    
    /**
     * @brief Destroys the Renderer object and cleans up resources.
     */
    ~Renderer();
    
    /**
     * @brief Initializes the renderer with application and rendering configurations.
     * 
     * @param appConfig The application configuration settings.
     * @param renderConfig The rendering-specific configuration settings.
     * 
     * @return true if initialization was successful.
     * @return false if initialization failed.
     */
    bool initialize(
        const AppConfig& appConfig, 
        const RenderConfig& renderConfig
    );
    
    /**
     * @brief Cleans up all resources used by the renderer.
     */
    void cleanup();
    
    /**
     * @brief Checks if the renderer is currently running.
     * 
     * @return true if the renderer is active and running.
     * @return false if the renderer is shutting down.
     */
    inline bool isRunning() const { return running && window.isOpen(); }
    
    /**
     * @brief Handles system events such as input and window events.
     */
    void handleEvents();
    
    /**
     * @brief Clears the rendering buffer to prepare for new frame.
     */
    void clear();
    
    /**
     * @brief Presents the rendered frame to the display.
     */
    void present();
    
    // ===== Texture management functions =====
    
    /**
     * @brief Loads a texture from the specified file path.
     * 
     * @param filepath The path to the texture file to load.
     * @return void* A pointer to the loaded texture resource.
     */
    void* loadTexture(const std::string& filepath);
    
    /**
     * @brief Destroys a previously loaded texture.
     * 
     * @param texture Pointer to the texture to destroy.
     */
    void destroyTexture(void* texture);
    
    // ===== Rendering functions =====
    
    /**
     * @brief Draws a texture at the specified position.
     * 
     * @param texture Pointer to the texture to draw.
     * @param srcX Source X coordinate in the texture.
     * @param srcY Source Y coordinate in the texture.
     * @param destX Destination X coordinate on screen.
     * @param destY Destination Y coordinate on screen.
     */
    void drawTexture(
        void* texture, 
        int srcX, 
        int srcY, 
        int destX, 
        int destY
    );
    
    /**
     * @brief Draws a sprite.
     * 
     * @param sprite The sprite to draw.
     */
    void drawSprite(const sf::Sprite& sprite);
    
    /**
     * @brief Draws a rectangle shape.
     * 
     * @param rect The rectangle to draw.
     */
    void drawRectangle(const sf::RectangleShape& rect);
    
    /**
     * @brief Draws text.
     * 
     * @param text The text to draw.
     */
    void drawText(const sf::Text& text);
    
    /**
     * @brief Sets the current view for rendering.
     * 
     * @param newView The new view to set.
     */
    void setView(const sf::View& newView);
    
    /**
     * @brief Gets the default view of the window.
     * 
     * @return sf::View The default view.
     */
    sf::View getDefaultView() const;
    
    /**
     * @brief Updates the camera position and clamps it to map boundaries.
     * 
     * @param position The new camera position.
     * @param mapWidth The width of the map in pixels.
     * @param mapHeight The height of the map in pixels.
     */
    void updateCamera(
        const sf::Vector2f& position, 
        int mapWidth, 
        int mapHeight
    );
    
    /**
     * @brief Checks if the render window is open.
     * 
     * @return true if the window is open.
     * @return false if the window is closed.
     */
    bool isWindowOpen() const;
    
    /**
     * @brief Gets the size of the render window.
     * 
     * @return sf::Vector2u The window size.
     */
    sf::Vector2u getWindowSize() const;
    
    /**
     * @brief Gets the current view.
     * 
     * @return const sf::View& The current view.
     */
    const sf::View& getCurrentView() const;

    /**
     * @brief Renders text objects (building names, etc.)
     */
    void renderTextObjects(const std::vector<TextObject>& textObjects);
    
    /**
     * @brief Renders entrance areas as semi-transparent rectangles
     */
    void renderEntranceAreas(const std::vector<EntranceArea>& areas);
    
    /**
     * @brief Render a simple modal prompt overlay using the UI/default view.
     * @param prompt The text to display.
     * @param font Font to use for rendering the prompt.
     * @param fontSize Font size in pixels.
     */
    void renderModalPrompt(const std::string& prompt, const sf::Font& font, unsigned int fontSize);

    /**
     * @brief When true, renderer will ignore Escape key as "close window".
     * Use this to prevent Escape from closing the app while a modal is active.
     */
    void setModalActive(bool active) { modalActive = active; }
    
    /**
     * @brief Configure the on-screen map button using AppConfig::MapButton settings.
     */
    void setMapButtonConfig(const AppConfig::MapButton& cfg);

    /**
     * @brief Draw the configured map button in screen coordinates (HUD).
     */
    void drawMapButton();

    /**
     * @brief Test whether a given mouse position (window coords) is inside the map button.
     */
    bool mapButtonContainsPoint(const sf::Vector2i& mousePos) const;
    
    /**
     * @brief Get current mouse position relative to the renderer window.
     * @return Position in window coordinates.
     */
    sf::Vector2i getMousePosition() const;

private:
    // Flag indicating whether the renderer is currently running
    bool running = true;
    // When true, ignore Escape key to close window (modal dialog active)
    bool modalActive = false;
    // Stores the current application configuration
    AppConfig currentAppConfig;
    // Stores the current rendering configuration
    RenderConfig currentRenderConfig;
    
    // SFML graphics objects
    sf::RenderWindow window;
    sf::View view;
    std::vector<std::unique_ptr<sf::Texture>> loadedTextures;
    std::unique_ptr<TextRenderer> textRenderer;
    std::unique_ptr<sf::Font> uiFont;                // font used for UI (map button)
    AppConfig::MapButton mapButtonConfig;            // active button configuration
};