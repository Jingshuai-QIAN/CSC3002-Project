// Renderer.cpp
#include "Renderer/Renderer.h"
#include "Utils/Logger.h"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <algorithm>

/**
 * @file Renderer.cpp
 * @brief Implementation of the Renderer class which wraps SFML windowing and drawing.
 *
 * The Renderer is responsible for:
 * - Creating and managing the SFML window.
 * - Handling events and simple input-related window actions.
 * - Exposing convenience drawing functions used by higher-level systems.
 *
 * Notes:
 * - The class keeps a small cache of textures allocated via loadTexture.
 * - Text rendering is delegated to TextRenderer.
 */
/*
 * Implementation details:
 * - Uses a small helper clampViewToMap to restrict camera view to the map bounds.
 * - Exposes simple drawing helpers used by TMJMap and higher-level app code.
 */


/**
 * Clamps the view to stay within map boundaries.
 * @param view Reference to the SFML view to adjust.
 * @param mapW The width of the map in pixels.
 * @param mapH The height of the map in pixels.
 */
namespace {
    void clampViewToMap(sf::View& view, int mapW, int mapH) {
        // Get the current size of the view
        sf::Vector2f size = view.getSize();
        // Calculate half size for boundary calculations
        sf::Vector2f half = { size.x * 0.5f, size.y * 0.5f };
        // Get the current center of the view
        sf::Vector2f center = view.getCenter();

        // Clamp horizontal position if view is smaller than map width
        if (size.x >= mapW) center.x = std::max(0.f, float(mapW) * 0.5f);
        else center.x = std::clamp(center.x, half.x, float(mapW) - half.x);

        // Clamp vertical position if view is smaller than map height
        if (size.y >= mapH) center.y = std::max(0.f, float(mapH) * 0.5f);
        else center.y = std::clamp(center.y, half.y, float(mapH) - half.y);

        // Apply the new clamped center to the view
        view.setCenter(center);
    }
}


/**
 * Renderer class constructor.
 * Initializes the renderer and sets running flag to true.
 */
Renderer::Renderer() : running(true) {
    textRenderer = std::make_unique<TextRenderer>();
    Logger::debug("Renderer constructor called");
    modalActive = false;
}


/**
 * Renderer class destructor.
 * Performs cleanup and logs destruction.
 */
Renderer::~Renderer() {
    // Clean up resources
    cleanup();
    // Log destructor call for debugging
    Logger::debug("Renderer destructor called");
}


/**
 * Initializes the renderer with provided configuration.
 * @param appConfig The application configuration settings.
 * @param renderConfig The rendering configuration settings.
 * @return True if initialization succeeded, false otherwise.
 */
bool Renderer::initialize(
    const AppConfig& appConfig, 
    const RenderConfig& renderConfig
) {
    // Log initialization start
    Logger::info("Initializing Renderer");
    
    // Store the provided configurations
    currentAppConfig = appConfig;
    currentRenderConfig = renderConfig;
    
    // Calculate window dimensions with maximum limits
    const int winW = std::min(appConfig.window.width, 1200);
    const int winH = std::min(appConfig.window.height, 800);
    
    try {
        // Create SFML window with calculated dimensions
        window.create(
            sf::VideoMode({
                static_cast<unsigned int>(winW), 
                static_cast<unsigned int>(winH)
            }), 
            appConfig.window.title
        );
        
        // Check if window creation was successful
        if (!window.isOpen()) {
            Logger::error("Failed to create SFML window");
            return false;
        }
        
    } catch (const std::exception& e) {
        // Log any exceptions during window creation
        Logger::error("Exception creating window: " + std::string(e.what()));
        return false;
    }
    
    // Set frame rate limit from configuration
    window.setFramerateLimit(appConfig.performance.targetFPS);
    
    // Initialize the game view
    const float tilesW = 40.f, tilesH = 40.f;
    const float viewW = tilesW * 16; // Assuming default tile width of 16 pixels
    const float viewH = tilesH * 16; // Assuming default tile height of 16 pixels
    
    // Create and set the initial view
    view = sf::View(sf::Vector2f(viewW * 0.5f, viewH * 0.5f), sf::Vector2f(viewW, viewH));
    window.setView(view);

    // Initialize text renderer with font from config
    if (!textRenderer->initialize(renderConfig.text.fontPath)) {
        Logger::warn("Failed to initialize text renderer, building names will not be displayed");
    }
    // Also load a UI font for button rendering (fallback to same font)
    uiFont = std::make_unique<sf::Font>();
    if (!uiFont->openFromFile(renderConfig.text.fontPath)) {
        Logger::warn("UI font not found at " + renderConfig.text.fontPath);
    }

    return true;
    
    // Log successful initialization
    Logger::info("Renderer initialized successfully");
    return true;
}


/**
 * Cleans up renderer resources and closes the window.
 */
void Renderer::cleanup() {
    // Log cleanup process
    Logger::debug("Cleaning up Renderer resources");
    
    // Clear all loaded textures (SFML textures auto-manage memory)
    for (auto& texture : loadedTextures) {
        // SFML textures are automatically managed
    }
    loadedTextures.clear();
    
    // Close the window if it's open
    if (window.isOpen()) {
        window.close();
    }
    
    // Set running flag to false
    running = false;
}


/**
 * Handles window events including input and window state changes.
 */
void Renderer::handleEvents() {
    // Return early if window is not open
    if (!window.isOpen()) return;
    
    // Process all pending events
    while (const std::optional event = window.pollEvent())
    {
        // Forward event handling for UI buttons could be added here if desired.
    // Handle window close event
    if (event->is<sf::Event::Closed>()) {
        // Stop the application and close window
        running = false;
        window.close();
    }
    // Handle Escape key as close only when no modal is active
    if (!modalActive) {
        if (event->is<sf::Event::KeyPressed>()) {
            if (event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape) {
                running = false;
                window.close();
            }
        }
    }

        // Handle window resize event
        if (const auto* resized = event->getIf<sf::Event::Resized>())
        {
            // Update view size to match new window dimensions
            view.setSize({
                static_cast<float>(resized->size.x),
                static_cast<float>(resized->size.y)
            });
            window.setView(view);
        }

        // Handle mouse button press events
        if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
            // Process left mouse button clicks
            if (mousePressed->button == sf::Mouse::Button::Left)
            {
                // Left button click handling (placeholder)
            }
        }

        // Handle keyboard key press events
        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            // Process specific key presses
            switch (keyPressed->code)
            {
                case sf::Keyboard::Key::F11:
                    // F11 fullscreen toggle (placeholder)
                    break;
                case sf::Keyboard::Key::F:
                    // F key handling (placeholder)
                    break;
                default:
                    break;
            }
        }

        // Handle mouse movement events
        if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            // Mouse movement handling (placeholder)
        }

        // Handle mouse wheel scroll events
        if (const auto* mouseWheel = event->getIf<sf::Event::MouseWheelScrolled>())
        {
            // Mouse wheel handling (placeholder)
        }
    }
}


/**
 * Clears the window with the configured clear color.
 */
void Renderer::clear() {
    // Return early if window is not open
    if (!window.isOpen()) return;
    
    // Get clear color from configuration and convert to SFML color
    const auto& clearColor = currentRenderConfig.clearColor;

    window.clear(sf::Color(
        static_cast<uint8_t>(clearColor.r * 255),
        static_cast<uint8_t>(clearColor.g * 255),
        static_cast<uint8_t>(clearColor.b * 255),
        static_cast<uint8_t>(clearColor.a * 255)
    ));
}


/**
 * Presents the rendered content to the screen.
 */
void Renderer::present() {
    if (!window.isOpen()) return;
    window.display();
}

// Simple helper: parse "#RRGGBB" or "#RRGGBBAA" into sf::Color. Returns white on parse error.
static sf::Color colorFromHex(const std::string& s) {
    if (s.size() != 7 && s.size() != 9) return sf::Color::White;

    try {
        int r = std::stoi(s.substr(1,2), nullptr, 16);
        int g = std::stoi(s.substr(3,2), nullptr, 16);
        int b = std::stoi(s.substr(5,2), nullptr, 16);
        int a = 255;

        if (s.size() == 9) a = std::stoi(s.substr(7,2), nullptr, 16);

        return sf::Color(
            static_cast<uint8_t>(r),
            static_cast<uint8_t>(g),
            static_cast<uint8_t>(b),
            static_cast<uint8_t>(a)
        );
    } catch (...) {
        return sf::Color::White;
    }
}

void Renderer::setMapButtonConfig(const AppConfig::MapButton& cfg) {
    mapButtonConfig = cfg;
}

bool Renderer::mapButtonContainsPoint(const sf::Vector2i& mousePos) const {
    if (!mapButtonConfig.enabled) return false;

    sf::Vector2u ws = getWindowSize();
    int px = mapButtonConfig.x;

    if (mapButtonConfig.anchorRight && px < 0) {
        px = static_cast<int>(ws.x) + px - mapButtonConfig.width;
    }

    int py = mapButtonConfig.y;
    int left = px;
    int top = py;
    int right = left + mapButtonConfig.width;
    int bottom = top + mapButtonConfig.height;

    return (
        mousePos.x >= left && 
        mousePos.x <= right && 
        mousePos.y >= top && 
        mousePos.y <= bottom
    );
}

void Renderer::drawMapButton() {
    if (!mapButtonConfig.enabled || !window.isOpen()) return;

    sf::Vector2u ws = getWindowSize();
    int px = mapButtonConfig.x;

    if (mapButtonConfig.anchorRight && px < 0) {
        px = static_cast<int>(ws.x) + px - mapButtonConfig.width;
    }

    int py = mapButtonConfig.y;

    // Draw in window (screen) coordinates: switch to default view temporarily.
    sf::View prevView = window.getView();
    window.setView(window.getDefaultView());

    sf::RectangleShape rect(sf::Vector2f(
        static_cast<float>(mapButtonConfig.width),
        static_cast<float>(mapButtonConfig.height)
    ));

    rect.setPosition(sf::Vector2f(
        static_cast<float>(px), 
        static_cast<float>(py)
    ));

    // hover detection
    sf::Vector2i mpos = sf::Mouse::getPosition(window);
    bool hover = mapButtonContainsPoint(mpos);

    rect.setFillColor(
        hover ? 
        colorFromHex(mapButtonConfig.hoverColor) : 
        colorFromHex(mapButtonConfig.bgColor)
    );
    rect.setOutlineThickness(0);

    window.draw(rect);

    // Draw label
    if (uiFont && mapButtonConfig.fontSize > 0) {
        sf::Text txt(
            *uiFont, 
            mapButtonConfig.label, 
            mapButtonConfig.fontSize
        );
        txt.setFillColor(colorFromHex(mapButtonConfig.textColor));

        // center text inside button
        sf::FloatRect local = txt.getLocalBounds();

        sf::Vector2f localPos = local.position;
        float localLeft = localPos.x;
        float localTop = localPos.y;

        sf::Vector2f localSize = local.size;
        float localWidth = localSize.x;
        float localHeight = localSize.y;

        txt.setOrigin(sf::Vector2f(
            localLeft + localWidth*0.5f, 
            localTop + localHeight*0.5f
        ));
        
        txt.setPosition(sf::Vector2f(
            static_cast<float>(px) + mapButtonConfig.width*0.5f,
            static_cast<float>(py) + mapButtonConfig.height*0.5f
        ));

        window.draw(txt);
    }

    // restore previous view
    window.setView(prevView);
}

sf::Vector2i Renderer::getMousePosition() const {
    if (!window.isOpen()) return {0,0};
    return sf::Mouse::getPosition(window);
}

/**
 * Loads a texture from file and returns a pointer to it.
 * @param filepath The path to the texture file.
 * @return Pointer to the loaded texture, or nullptr if failed.
 */
void* Renderer::loadTexture(const std::string& filepath) {
    // Log texture loading attempt
    Logger::debug("Loading texture: " + filepath);
    
    // Create a new texture object
    auto texture = std::make_unique<sf::Texture>();
    // Attempt to load texture from file
    if (!texture->loadFromFile(filepath)) {
        // Log failure and return null
        Logger::error("Failed to load texture: " + filepath);
        return nullptr;
    }
    
    // Set texture smoothing based on configuration
    texture->setSmooth(currentAppConfig.performance.textureFilter == 1);
    
    // Store texture and return raw pointer
    void* texturePtr = texture.get();
    loadedTextures.push_back(std::move(texture));
    
    return texturePtr;
}


/**
 * Destroys a previously loaded texture.
 * @param texture Pointer to the texture to destroy.
 */
void Renderer::destroyTexture(void* texture) {
    // Return early if texture pointer is null
    if (!texture) return;
    
    // Log texture destruction
    Logger::debug("Destroying texture");
    
    // Remove texture from the managed container
    loadedTextures.erase(
        std::remove_if(loadedTextures.begin(), loadedTextures.end(),
            [texture](const std::unique_ptr<sf::Texture>& tex) {
                return tex.get() == texture;
            }),
        loadedTextures.end()
    );
}


/**
 * Draws a texture at the specified position.
 * @param texture Pointer to the texture to draw.
 * @param srcX Source X coordinate in the texture.
 * @param srcY Source Y coordinate in the texture.
 * @param destX Destination X coordinate on screen.
 * @param destY Destination Y coordinate on screen.
 */
void Renderer::drawTexture(
    void* texture, 
    int srcX, 
    int srcY, 
    int destX, 
    int destY
) {
    // Return early if texture is null or window is not open
    if (!texture || !window.isOpen()) return;
    
    // Cast to SFML texture and create sprite
    sf::Texture* sfmlTexture = static_cast<sf::Texture*>(texture);
    sf::Sprite sprite(*sfmlTexture);
    
    // Set texture source rectangle if source coordinates are provided
    sf::Vector2u textureSize = sfmlTexture->getSize();

    if (srcX >= 0 && srcY >= 0) {
        sprite.setTextureRect(sf::IntRect(
            {
                srcX, 
                srcY
            }, 
            {
                static_cast<int>(textureSize.x), 
                static_cast<int>(textureSize.y)
            }
        ));
    }
    
    // Set sprite position and draw it
    sprite.setPosition(sf::Vector2f{
        static_cast<float>(destX), 
        static_cast<float>(destY)
    });
    window.draw(sprite);
}


/**
 * Draws an SFML sprite to the window.
 * @param sprite The sprite to draw.
 */
void Renderer::drawSprite(const sf::Sprite& sprite) {
    // Return early if window is not open
    if (!window.isOpen()) return;

    static int drawCount = 0;
    drawCount++;
    if (drawCount % 1000 == 0) {
        Logger::debug("Drawn " + std::to_string(drawCount) + " sprites");
    }
    
    // Draw the sprite
    window.draw(sprite);
}


/**
 * Draws an SFML rectangle shape to the window.
 * @param rect The rectangle shape to draw.
 */
void Renderer::drawRectangle(const sf::RectangleShape& rect) {
    // Return early if window is not open
    if (!window.isOpen()) return;
    // Draw the rectangle
    window.draw(rect);
}


/**
 * Draws SFML text to the window.
 * @param text The text object to draw.
 */
void Renderer::drawText(const sf::Text& text) {
    // Return early if window is not open
    if (!window.isOpen()) return;
    // Draw the text
    window.draw(text);
}


/**
 * Sets the current view for rendering.
 * @param newView The new view to set as active.
 */
void Renderer::setView(const sf::View& newView) {
    // Return early if window is not open
    if (!window.isOpen()) return;
    // Update and apply the new view
    view = newView;
    window.setView(view);
}


/**
 * Gets the default view of the window.
 * @return The default SFML view.
 */
sf::View Renderer::getDefaultView() const {
    // Return empty view if window is closed, otherwise return default view
    if (!window.isOpen()) return sf::View();
    return window.getDefaultView();
}


/**
 * Updates the camera position and clamps it to map boundaries.
 * @param position The new camera center position.
 * @param mapWidth The width of the map in pixels.
 * @param mapHeight The height of the map in pixels.
 */
void Renderer::updateCamera(
    const sf::Vector2f& position, 
    int mapWidth, 
    int mapHeight
) {
    // Return early if window is not open
    if (!window.isOpen()) return;
    
    // Set new camera center position
    view.setCenter(position);
    
    // Adjust view size if it exceeds map boundaries
    sf::Vector2f size = view.getSize();
    bool adjust = false;
    if (size.x > static_cast<float>(mapWidth)) { 
        size.x = static_cast<float>(mapWidth); 
        adjust = true; 
    }
    if (size.y > static_cast<float>(mapHeight)) { 
        size.y = static_cast<float>(mapHeight); 
        adjust = true; 
    }
    if (adjust) view.setSize(size);
    
    // Clamp view to stay within map boundaries and apply
    clampViewToMap(view, mapWidth, mapHeight);
    window.setView(view);
}


/**
 * Checks if the render window is currently open.
 * @return True if window is open, false otherwise.
 */
bool Renderer::isWindowOpen() const {
    return window.isOpen();
}


/**
 * Checks if the renderer is running (window open and running flag true).
 * @return True if renderer is running, false otherwise.
 */
// bool Renderer::isRunning() const { 
//     return running && window.isOpen(); 
// }


/**
 * Gets the current size of the render window.
 * @return The window size as SFML vector, or zero vector if window closed.
 */
sf::Vector2u Renderer::getWindowSize() const {
    // Return zero size if window closed, otherwise actual size
    if (!window.isOpen()) return sf::Vector2u(0, 0);
    return window.getSize();
}


/**
 * Gets the current active view.
 * @return Reference to the current SFML view.
 */
const sf::View& Renderer::getCurrentView() const {
    return view;
}


void Renderer::renderTextObjects(const std::vector<TextObject>& textObjects) {
    if (textObjects.empty()) return;
    
    textRenderer->renderTextObjects(textObjects, window);
}


void Renderer::renderEntranceAreas(const std::vector<EntranceArea>& areas) {
    for (const auto& area : areas) {
        sf::RectangleShape rect(sf::Vector2f(area.width, area.height));
        rect.setPosition(sf::Vector2f{area.x, area.y});
        rect.setFillColor(sf::Color(0, 100, 255, 100)); // Semi-transparent blue
        rect.setOutlineThickness(0);
        window.draw(rect);
    }
}


void Renderer::renderGameTriggerAreas(const std::vector<GameTriggerArea>& areas) {
    if (!window.isOpen()) return;

    for (const auto& area : areas) {
        sf::RectangleShape rect(sf::Vector2f(area.width, area.height));
        rect.setPosition(sf::Vector2f(area.x, area.y));
        rect.setOutlineThickness(2.f);

        // 你现在只有一个 quiz game，我只保留这一种颜色逻辑
        // Quiz 触发区 —— 金色半透明
        rect.setFillColor(sf::Color(255, 215, 0, 140));
        rect.setOutlineColor(sf::Color(200, 170, 0));

        window.draw(rect);
    }
}


void Renderer::renderModalPrompt(const std::string& prompt, const sf::Font& font, unsigned int fontSize) {
    if (!window.isOpen()) return;

    // draw overlay using default UI view
    sf::View prevView = window.getView();
    window.setView(window.getDefaultView());

    sf::Vector2u winSz = getWindowSize();
    sf::RectangleShape bg(sf::Vector2f(static_cast<float>(winSz.x), static_cast<float>(winSz.y)));
    bg.setPosition(sf::Vector2f(0.f, 0.f));
    bg.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(bg);

    sf::Text t(font, prompt, static_cast<unsigned int>(fontSize));
    t.setFillColor(colorFromHex(currentRenderConfig.text.textColor));

    sf::FloatRect tb = t.getLocalBounds();
    t.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
    t.setPosition(sf::Vector2f(static_cast<float>(winSz.x) * 0.5f, static_cast<float>(winSz.y) * 0.5f));
    window.draw(t);

    // restore view
    window.setView(prevView);
}
