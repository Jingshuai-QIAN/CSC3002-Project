#include "App.h"

#include "Renderer/Renderer.h"
#include "MapLoader/MapLoader.h"
#include "MapLoader/TMJMap.h"
#include "Renderer/TextRenderer.h"
#include "Input/InputManager.h"
#include "Utils/Logger.h"
#include <filesystem>
#include "Character/Character.h"

// Helper: detect whether character is inside an entrance and facing it.
/**
 * @brief Check whether the character's feet lie inside any entrance area and
 *        whether the character is facing that entrance.
 *
 * If an entrance is found and the character is facing toward its center, the
 * function fills outArea with that entrance and returns true.
 *
 * @param character The player character to test.
 * @param map Pointer to the TMJMap containing entranceAreas (may be nullptr).
 * @param outArea Output parameter filled with the matched EntranceArea on success.
 * @return true if an entrance trigger is detected and the character is facing it.
 */
static bool detectEntranceTrigger(const Character& character, const TMJMap* map, EntranceArea& outArea) {
    if (!map) return false;
    sf::Vector2f feet = character.getFeetPoint();
    for (const auto& a : map->getEntranceAreas()) {
        sf::FloatRect rect(sf::Vector2f(a.x, a.y), sf::Vector2f(a.width, a.height));
        if (!rect.contains(feet)) continue;
        sf::Vector2f center(a.x + a.width * 0.5f, a.y + a.height * 0.5f);
        sf::Vector2f v = center - feet;
        Character::Direction desired;
        if (std::abs(v.x) > std::abs(v.y)) {
            desired = v.x > 0 ? Character::Direction::Right : Character::Direction::Left;
        } else {
            desired = v.y > 0 ? Character::Direction::Down : Character::Direction::Up;
        }
        if (desired == character.getCurrentDirection()) {
            outArea = a;
            return true;
        }
    }
    return false;
}

// Helper: show the full-map modal (blocking) — extracted from main loop.
/**
 * @brief Display an interactive fullscreen map modal window.
 *
 * The modal shows the entire map scaled to the desktop size and supports
 * zooming (mouse wheel) and panning (drag). This is a blocking call that
 * returns when the user closes the modal (Escape or window close).
 *
 * @param renderer Reference to the main Renderer (used only for style/config).
 * @param tmjMap Shared pointer to the map to render inside the modal.
 * @param configManager Configuration manager used to obtain font paths.
 */
static void showFullMapModal(Renderer& renderer, const std::shared_ptr<TMJMap>& tmjMap, const ConfigManager& configManager) {
    // create fullscreen window
    auto dm = sf::VideoMode::getDesktopMode();
    sf::RenderWindow mapWin(dm, sf::String("Full Map"), sf::State::Windowed);
    mapWin.setFramerateLimit(60);

    // prepare a view that covers the whole map in world coords
    int mapW = tmjMap->getWorldPixelWidth();
    int mapH = tmjMap->getWorldPixelHeight();

    // Desktop/window size
    float winW = static_cast<float>(dm.size.x);
    float winH = static_cast<float>(dm.size.y);

    float mapWf = static_cast<float>(mapW);
    float mapHf = static_cast<float>(mapH);

    // Compute uniform scale to preserve tile aspect ratio
    float scale = 1.f;
    if (mapW > 0 && mapH > 0) scale = std::min(winW / mapWf, winH / mapHf);

    float displayW = mapWf * scale;
    float displayH = mapHf * scale;

    // Normalized viewport (left, top, width, height) in [0,1]
    float left = (winW - displayW) * 0.5f / winW;
    float top  = (winH - displayH) * 0.5f / winH;
    float vw   = (displayW / winW);
    float vh   = (displayH / winH);

    sf::View fullView(sf::FloatRect({0.f, 0.f}, {mapWf, mapHf}));
    fullView.setViewport(sf::FloatRect({left, top}, {vw, vh}));
    mapWin.setView(fullView);

    // create a local TextRenderer to draw map text using same font path
    TextRenderer tr;
    tr.initialize(configManager.getRenderConfig().text.fontPath);

    // interactive fullscreen modal: support zoom (mouse wheel) and drag (left button)
    float zoomFactor = 1.f; // 1.0 = no zoom (initial fit)
    const float ZOOM_MIN = 0.25f;
    const float ZOOM_MAX = 8.0f;

    bool dragging = false;
    sf::Vector2i prevDragPixel{0,0};

    // view variable initialized from fullView (keeps viewport)
    sf::View view = fullView;

    while (mapWin.isOpen()) {
        while (auto ev = mapWin.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) { mapWin.close(); break; }
            if (ev->is<sf::Event::KeyPressed>()) {
                if (ev->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape) {
                    mapWin.close(); break;
                }
            }

            if (ev->is<sf::Event::MouseWheelScrolled>()) {
                // mouse wheel zoom: positive -> zoom in
                auto mw = ev->getIf<sf::Event::MouseWheelScrolled>();
                if (mw) {
                    // Get mouse wheel delta
                    float delta = mw->delta;
                    // Zoom in/out based on mouse wheel delta
                    if (delta > 0) zoomFactor *= 1.1f;
                    else if (delta < 0) zoomFactor /= 1.1f;

                    // Clamp zoom factor between min and max
                    zoomFactor = std::clamp(zoomFactor, ZOOM_MIN, ZOOM_MAX);
                    // Update view size to match new zoom factor
                    view.setSize(sf::Vector2f{
                        mapWf / zoomFactor,
                        mapHf / zoomFactor
                    });
                    // Update viewport to match new zoom factor
                    view.setViewport(sf::FloatRect({left, top}, {vw, vh}));
                    // Update viewport to match new zoom factor
                    mapWin.setView(view);
                }
            }

            if (ev->is<sf::Event::MouseButtonPressed>()) {
                auto mb = ev->getIf<sf::Event::MouseButtonPressed>();
                if (mb && mb->button == sf::Mouse::Button::Left) {
                    dragging = true;
                    prevDragPixel = mb->position;
                }
            }
            if (ev->is<sf::Event::MouseButtonReleased>()) {
                auto mr = ev->getIf<sf::Event::MouseButtonReleased>();
                if (mr && mr->button == sf::Mouse::Button::Left) {
                    dragging = false;
                }
            }
            if (ev->is<sf::Event::MouseMoved>()) {
                auto mm = ev->getIf<sf::Event::MouseMoved>();
                if (mm && dragging) {
                    sf::Vector2i curPixel = mm->position;
                    // convert pixel movement to world movement
                    sf::Vector2f prevWorld = mapWin.mapPixelToCoords(prevDragPixel);
                    sf::Vector2f curWorld  = mapWin.mapPixelToCoords(curPixel);
                    sf::Vector2f diff = prevWorld - curWorld;
                    view.setCenter(view.getCenter() + diff);
                    mapWin.setView(view);
                    prevDragPixel = curPixel;
                }
            }
        } // events

        if (!mapWin.isOpen()) break;

        mapWin.clear(sf::Color::Black);
        // draw all map tiles (TMJMap holds sprites positioned in world coords)
        for (const auto& s : tmjMap->getTiles()) mapWin.draw(s);
        // draw entrance areas
        for (const auto& a : tmjMap->getEntranceAreas()) {
            sf::RectangleShape rect(sf::Vector2f(a.width, a.height));
            rect.setPosition(sf::Vector2f(a.x, a.y));
            rect.setFillColor(sf::Color(0, 100, 255, 120));
            rect.setOutlineThickness(0);
            mapWin.draw(rect);
        }
        // draw text objects if font loaded
        if (tr.isFontLoaded()) {
            tr.renderTextObjects(tmjMap->getTextObjects(), mapWin);
        }

    mapWin.display();
    } // modal window loop

} // end showFullMapModal

// Attempt to load target map from entrance; returns true on success and updates tmjMap & character & renderer view.
/**
 * @brief Load the target map specified by an entrance and position the character.
 *
 * This helper resolves the entrance.target relative to the current map directory,
 * loads the target TMJ map via MapLoader and places the character at the resolved
 * spawn position (priority: entrance.targetX/Y -> spawn override -> map spawn -> center).
 *
 * @param mapLoader Reference to the MapLoader used to load the target TMJ map.
 * @param tmjMap Shared pointer reference that will be updated to the newly loaded map.
 * @param entrance The EntranceArea that triggered the transition.
 * @param character The player character to reposition.
 * @param renderer Renderer used to update camera after spawn.
 * @param configManager Configuration manager used for any required paths.
 * @return true on success, false if map loading failed.
 */
static bool tryEnterTarget(
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    const EntranceArea& entrance,
    Character& character,
    Renderer& renderer,
    ConfigManager& configManager
) {
    namespace fs = std::filesystem;
    fs::path targetPath(entrance.target);
    fs::path resolved = targetPath.is_absolute() ? targetPath : fs::path(mapLoader.getMapDirectory()) / targetPath;
    std::string resolvedStr = resolved.generic_string();
    auto newMap = mapLoader.loadTMJMap(resolvedStr);
    if (!newMap) {
        Logger::error("Failed to load target map: " + resolvedStr);
        return false;
    }
    tmjMap = newMap;
    // Debug log entrance explicit coords and resolve path
    Logger::info("Entering target map: " + resolvedStr + " via entrance target='" + entrance.target + "'");
    if (entrance.targetX && entrance.targetY) {
        Logger::info("  entrance provides targetX/Y = " + std::to_string(*entrance.targetX) + ", " + std::to_string(*entrance.targetY));
    } else {
        Logger::info("  entrance has no explicit targetX/Y");
    }
    // resolve spawn: prefer explicit entrance targetX/targetY (if provided),
    // then any stored override, then map spawn, then center
    sf::Vector2f spawnPos;
    if (entrance.targetX && entrance.targetY) {
        spawnPos = sf::Vector2f(*entrance.targetX, *entrance.targetY);
    } else {
        auto ov = mapLoader.getSpawnOverride(resolvedStr);
        if (ov) {
            Logger::info("  using spawn override for map: " + resolvedStr + " -> " + std::to_string(ov->x) + ", " + std::to_string(ov->y));
        } else if (newMap->getSpawnX() && newMap->getSpawnY()) {
            Logger::info("  using map's default spawn: " + std::to_string(*newMap->getSpawnX()) + ", " + std::to_string(*newMap->getSpawnY()));
        } else {
            Logger::info("  no spawn found, will use map center");
        }
        spawnPos = mapLoader.resolveSpawnForMap(resolvedStr, *newMap);
    }
    character.setPosition(spawnPos);
    renderer.updateCamera(spawnPos, tmjMap->getWorldPixelWidth(), tmjMap->getWorldPixelHeight());
    return true;
}

static void cancelEntranceMove(Character& character, const TMJMap& map) {
    int tileW = map.getTileWidth();
    int tileH = map.getTileHeight();
    sf::Vector2f pos = character.getPosition();
    switch (character.getCurrentDirection()) {
        case Character::Direction::Up:    pos.y += static_cast<float>(tileH); break;
        case Character::Direction::Down:  pos.y -= static_cast<float>(tileH); break;
        case Character::Direction::Left:  pos.x += static_cast<float>(tileW); break;
        case Character::Direction::Right: pos.x -= static_cast<float>(tileW); break;
    }
    character.setPosition(pos);
}

/**
 * @brief Main application loop extracted from main to keep main concise.
 *
 * The function drives input sampling, character updates, entrance detection,
 * modal dialogs, and rendering. It takes ownership references to the core
 * subsystems already initialized by main().
 */

void runApp(
    Renderer& renderer,
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    Character& character,
    sf::View& view,
    ConfigManager& configManager
) {
    // Initialize input manager
    auto& inputManager = InputManager::getInstance();
    
    // Modal UI font for simple prompts (loaded once)
    sf::Font modalFont;
    modalFont.openFromFile(configManager.getRenderConfig().text.fontPath);

    // Entrance confirmation state
    bool waitingForEntranceConfirmation = false;
    EntranceArea pendingEntrance;
    // Suppress entrance detection until player leaves the entrance area they spawned inside
    bool hasSuppressedEntrance = false;
    sf::FloatRect suppressedEntranceRect;

    // Main loop
    sf::Clock clock;
    while (renderer.isRunning()) {
        float deltaTime = clock.restart().asSeconds();

        // Process input
        inputManager.update();
        
        // Handle events
        renderer.handleEvents();

        // Update character (skip updating when waiting for modal confirmation)
        sf::Vector2f moveInput = inputManager.getMoveInput();
        if (!waitingForEntranceConfirmation) {
            character.update(deltaTime, moveInput, 
                            tmjMap->getWorldPixelWidth(), 
                            tmjMap->getWorldPixelHeight(),
                            tmjMap.get());
        }

        // Detect entrance triggers (when not already waiting for confirmation)
        if (!waitingForEntranceConfirmation) {
            if (hasSuppressedEntrance) {
                sf::Vector2f feet = character.getFeetPoint();
                if (suppressedEntranceRect.contains(feet)) {
                    // still inside suppressed entrance area -> skip detection
                } else {
                    // player left the entrance area -> lift suppression
                    hasSuppressedEntrance = false;
                }
            }
            if (!hasSuppressedEntrance) {
                EntranceArea detected;
                if (detectEntranceTrigger(character, tmjMap.get(), detected)) {
                    waitingForEntranceConfirmation = true;
                    pendingEntrance = detected;
                    // Log which entrance was detected (helps verify targetX/targetY presence)
                    Logger::info("Detected entrance trigger: '" + detected.name + "' target='" + detected.target + "'");
                    if (detected.targetX && detected.targetY) {
                        Logger::info("  entrance has explicit targetX/Y = " + std::to_string(*detected.targetX) + ", " + std::to_string(*detected.targetY));
                    } else {
                        Logger::info("  entrance has no explicit targetX/Y");
                    }
                    // While overlay is active, prevent renderer from treating Escape as quit
                    renderer.setModalActive(true);
                }
            }
        }

        // Detect map button click (edge detect)
        static bool prevMousePressed = false;
        bool currMousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

        if (currMousePressed && !prevMousePressed) {
            // new click
            sf::Vector2i mpos = renderer.getMousePosition();

            if (renderer.mapButtonContainsPoint(mpos)) {
                // open modal fullscreen map window
                showFullMapModal(renderer, tmjMap, configManager);
            }
        }
        prevMousePressed = currMousePressed;

        // Camera follow character (use Renderer to update & clamp view)
        if (!waitingForEntranceConfirmation) {
            renderer.updateCamera(character.getPosition(),
                                  tmjMap->getWorldPixelWidth(),
                                  tmjMap->getWorldPixelHeight());
        } else {
            // still ensure renderer has a valid view
            renderer.updateCamera(view.getCenter(),
                                  tmjMap->getWorldPixelWidth(),
                                  tmjMap->getWorldPixelHeight());
        }

        Logger::debug(
            "Camera - Center: (" + 
            std::to_string(view.getCenter().x) + ", " + 
            std::to_string(view.getCenter().y) + 
            ") Size: (" + 
            std::to_string(view.getSize().x) + ", " + 
            std::to_string(view.getSize().y) + ")"
        );

        Logger::debug(
            "Character position: (" + 
            std::to_string(character.getPosition().x) + ", " + 
            std::to_string(character.getPosition().y) + ")"
        );

        // Inspect first tile position (if any)
        if (!tmjMap->getTiles().empty()) {
            const auto& firstTile = tmjMap->getTiles()[0];
            Logger::debug(
                "First tile position: (" + 
                std::to_string(firstTile.getPosition().x) + ", " + 
                std::to_string(firstTile.getPosition().y) + ")"
            );
        }

        renderer.clear();
        
        mapLoader.render(&renderer);
        
        renderer.renderTextObjects(tmjMap->getTextObjects());
        renderer.renderEntranceAreas(tmjMap->getEntranceAreas());
        renderer.drawSprite(character.getSprite());
        renderer.drawMapButton();
        // If waiting for entrance confirmation, draw a simple modal overlay and handle input
        if (waitingForEntranceConfirmation) {
            // handle confirm/cancel input
            if (inputManager.isKeyJustPressed(sf::Keyboard::Key::Enter)) {
                // Confirm: attempt to load target map
                // record override for current map before switching
                {
                    std::string fromKey = mapLoader.getCurrentMapPath();
                    if (!fromKey.empty()) {
                        mapLoader.setSpawnOverride(fromKey, character.getPosition().x, character.getPosition().y);
                    }
                }
                bool ok = tryEnterTarget(mapLoader, tmjMap, pendingEntrance, character, renderer, configManager);
                if (!ok) {
                    waitingForEntranceConfirmation = false;
                } else {
                    // If player spawned inside an entrance area in the new map, suppress
                    sf::Vector2f pos = character.getPosition();
                    for (const auto& a : tmjMap->getEntranceAreas()) {
                        sf::FloatRect r(sf::Vector2f(a.x, a.y), sf::Vector2f(a.width, a.height));
                        if (r.contains(pos)) {
                            hasSuppressedEntrance = true;
                            suppressedEntranceRect = r;
                            break;
                        }
                    }
                    // Lift renderer-level Escape->close suppression only while confirmation overlay is active.
                    renderer.setModalActive(false);
                    waitingForEntranceConfirmation = false;
                }
            } else if (inputManager.isKeyJustPressed(sf::Keyboard::Key::Escape)) {
                // Cancel: move character 1 tile away from entrance along opposite facing
                cancelEntranceMove(character, *tmjMap);
                waitingForEntranceConfirmation = false;
            }

            // draw centered modal prompt via renderer helper
            std::string prompt = "Do you want to enter " + pendingEntrance.name + "?  Enter=Yes  Esc=No";
            renderer.renderModalPrompt(prompt, modalFont, configManager.getRenderConfig().text.fontSize);
        }

        renderer.present();
    }
}
