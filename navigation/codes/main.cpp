// main.cpp
#include <SFML/Graphics.hpp>
#include <memory>
#include "Config/ConfigManager.h"
#include "MapLoader/MapLoader.h"
#include "Renderer/Renderer.h"
#include "Character/Character.h"
#include "Input/InputManager.h"
#include "Utils/Logger.h"
#include "Renderer/TextRenderer.h"
#include <filesystem>
#include "App.h"
#include "Login/LoginScreen.h"

/**
 * @file main.cpp
 * @brief Application entry point for the navigation demo.
 *
 * This file wires together the configuration manager, renderer, map loader,
 * character and input subsystems and runs the main game loop.
 *
 * Notes:
 * - The main loop performs: input sampling, event handling, character update,
 *   camera follow, and rendering.
 */

/**
 * @brief Clamp the view center so it stays within the map bounds.
 * @param view The view to clamp.
 * @param mapWidth Map width in pixels.
 * @param mapHeight Map height in pixels.
 */
void clampViewToBounds(sf::View& view, int mapWidth, int mapHeight) {
    sf::Vector2f center = view.getCenter();
    sf::Vector2f size   = view.getSize();
    sf::Vector2f half   = { size.x * 0.5f, size.y * 0.5f };

    if (size.x >= static_cast<float>(mapWidth)) {
        center.x = std::max(0.f, float(mapWidth) * 0.5f);
    } else {
        center.x = std::clamp(center.x, half.x, float(mapWidth) - half.x);
    }

    if (size.y >= static_cast<float>(mapHeight)) {
        center.y = std::max(0.f, float(mapHeight) * 0.5f);
    } else {
        center.y = std::clamp(center.y, half.y, float(mapHeight) - half.y);
    }

    view.setCenter(center);
}


// Run the main application loop (extracted from main for modularity)
static void runMainLoop(
    Renderer& renderer,
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    Character& character,
    sf::View& view,
    ConfigManager& configManager
) {
    // Run the main loop (extracted)
    runMainLoop(renderer, mapLoader, tmjMap, character, view, configManager);
}


int main() {
    // Initialize configuration
    auto& configManager = ConfigManager::getInstance();
    if (!configManager.loadAllConfigs()) {
        Logger::error("Failed to load configurations");
        return -1;
    }

    // Initialize character configuration
    auto& characterConfigManager = CharacterConfigManager::getInstance();
    characterConfigManager.loadConfig();

    // Initialize renderer
    Renderer renderer;
    if (!renderer.initialize(
        configManager.getAppConfig(), 
        configManager.getRenderConfig()
    )) {
        Logger::error("Failed to initialize renderer");
        return -1;
    }
    // NOTE: schedule button should NOT be enabled during the login screens.
    // It will be configured later (before entering the main game loop).

    // 3. Main loop: 只运行一次，直到7天结束
    bool shouldRun = true;
    while (shouldRun) {
        if (!runLoginScreen(renderer)) {
            renderer.cleanup();
            return 0;
        }

        // Initialize map loader
        MapLoader mapLoader;
        std::string mapPath = configManager.getFullMapPath();
        auto tmjMap = mapLoader.loadTMJMap(mapPath);

        if (!tmjMap) {
            Logger::error("Failed to load map: " + mapPath);
            return -1;
        }

        Logger::info(
            "Map pixel dimensions: " + 
            std::to_string(tmjMap->getWorldPixelWidth()) + "x" + 
            std::to_string(tmjMap->getWorldPixelHeight())
        );
        Logger::info(
            "Tile dimensions: " + 
            std::to_string(tmjMap->getTileWidth()) + "x" + 
            std::to_string(tmjMap->getTileHeight())
        );
        

        // Initialize character
        Character character;
        if (!character.initialize()) {
            Logger::error("Failed to initialize character");
            return -1;
        }

        // Set initial character position
        sf::Vector2f spawnPosition;
        if (tmjMap->getSpawnX() && tmjMap->getSpawnY()) {
            spawnPosition = sf::Vector2f(*tmjMap->getSpawnX(), *tmjMap->getSpawnY());
        } else {
            // Use map center as fallback spawn point
            spawnPosition = sf::Vector2f(
                tmjMap->getWorldPixelWidth() * 0.5f,
                tmjMap->getWorldPixelHeight() * 0.5f
            );
        }
        character.setPosition(spawnPosition);

        // Retrieve configuration
        const auto& appConfig = configManager.getAppConfig();

        // Compute view size (based on configured tile counts)
        const float viewW = appConfig.mapDisplay.tilesWidth * tmjMap->getTileWidth();
        const float viewH = appConfig.mapDisplay.tilesHeight * tmjMap->getTileHeight();

        // Create view
        sf::View view(sf::Vector2f(0, 0), sf::Vector2f(viewW, viewH));
        view.setCenter(spawnPosition);

        // Set renderer view
        renderer.setView(view);
        // configure map button from app config
        renderer.setMapButtonConfig(configManager.getAppConfig().mapButton);
        // configure schedule button from app config
        {
            auto sb = configManager.getAppConfig().scheduleButton;
            const auto& mb = configManager.getAppConfig().mapButton;
            // If both anchored to right, ensure schedule sits to the left of map without overlap
            if (sb.anchorRight && mb.anchorRight) {
                // place schedule left of map with 10px gap
                sb.x = mb.x - (sb.width + 10);
            }
            renderer.setScheduleButtonConfig(sb);
        }

        Logger::info("Map pixel dimensions: " + 
                std::to_string(tmjMap->getWorldPixelWidth()) + "x" + 
                std::to_string(tmjMap->getWorldPixelHeight()));
        Logger::info("Tile dimensions: " + 
                    std::to_string(tmjMap->getTileWidth()) + "x" + 
                    std::to_string(tmjMap->getTileHeight()));
        Logger::info("Spawn position: (" + 
                    std::to_string(spawnPosition.x) + ", " + 
                    std::to_string(spawnPosition.y) + ")");
        Logger::info("Calculated view size: " + 
                    std::to_string(viewW) + "x" + 
                    std::to_string(viewH) + 
                    " (based on " + std::to_string(appConfig.mapDisplay.tilesWidth) + 
                    "x" + std::to_string(appConfig.mapDisplay.tilesHeight) + " tiles)");
        Logger::info("View center: (" + 
                    std::to_string(view.getCenter().x) + ", " + 
                    std::to_string(view.getCenter().y) + ")");
        
        // 3.5 Run main game loop
        AppResult appResult = runApp(renderer, mapLoader, tmjMap, character, view, configManager);

        // Clean up per-run resources
        character.cleanup();
        mapLoader.cleanup();

        // 无论结果如何都退出（因为7天已结束）
        shouldRun = false;
    }

    // 4. Final renderer cleanup
    renderer.cleanup();
    return 0;
}
