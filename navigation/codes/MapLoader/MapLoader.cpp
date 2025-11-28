// MapLoader.cpp
#include "MapLoader.h"               // Implementation of MapLoader class
#include "Renderer/Renderer.h"       // Renderer interface used for drawing
#include "Utils/Logger.h"            // Logging utilities
#include "Utils/FileUtils.h"         // File utility helpers

// Standard library includes for file IO and JSON parsing.
#include <fstream>                   // std::ifstream
#include <nlohmann/json.hpp>         // nlohmann::json

// Alias used throughout this file for convenience.
using json = nlohmann::json;

/**
 * @file MapLoader.cpp
 * @brief Implementation of map loading, tileset management and simple rendering.
 *
 * This file contains the concrete behavior for parsing legacy map files and
 * for orchestrating TMJ map loading via TMJMap. It also exposes a render entry
 * point that delegates drawing to either the TMJMap (when available) or the
 * legacy TileLayer system.
 *
 * Notes:
 * - Errors are logged via Utils::Logger.
 * - JSON parsing uses nlohmann::json and throws exceptions on malformed input;
 *   exceptions are caught and converted to error logs.
 */
/*
 * The MapLoader is a high-level component responsible for:
 * - Parsing JSON-based map files (legacy format and TMJ).
 * - Loading tilesets via TileSetManager.
 * - Holding a collection of TileLayer objects for legacy maps.
 * - Delegating TMJ rendering and exposing helper APIs used by the app.
 */


/**
 * @brief Constructs a new MapLoader object and initializes the tileset manager.
 */
MapLoader::MapLoader() {
    tileSetManager = std::make_unique<TileSetManager>();
}


/**
 * @brief Destroys the MapLoader object and cleans up resources.
 */
MapLoader::~MapLoader() {
    cleanup();
}


/**
 * @brief Loads a map from the specified file path.
 * 
 * @param filepath The path to the map file to load.
 * @return true if the map was loaded successfully, false otherwise.
 */
bool MapLoader::loadMap(const std::string& filepath) {
    cleanup();
    
    // Extract map directory from file path
    size_t lastSlash = filepath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        mapDirectory = filepath.substr(0, lastSlash + 1);
    }
    
    if (!parseMapFile(filepath)) {
        Logger::error("Failed to parse map file: " + filepath);
        return false;
    }
    
    if (!loadTileSets()) {
        Logger::error("Failed to load tilesets");
        return false;
    }
    
    Logger::info(
        "Map loaded successfully: " + 
        std::to_string(mapWidth) + "x" + 
        std::to_string(mapHeight)
    );
    return true;
}


/**
 * @brief Parses the map file at the given file path.
 * 
 * @param filepath The path to the map file to parse.
 * @return true if parsing was successful, false otherwise.
 */
bool MapLoader::parseMapFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::error("Cannot open map file: " + filepath);
            return false;
        }
        
        json mapData = json::parse(file);
        
        // Check whether the necessary keys exist
        if (!mapData.contains("width") || !mapData.contains("height") ||
            !mapData.contains("tilewidth") || !mapData.contains("tileheight") ||
            !mapData.contains("layers") || !mapData.contains("tilesets")) {
            Logger::error("Map file missing required fields");
            return false;
        }
        
        // Parse basic map information
        mapWidth = mapData["width"];
        mapHeight = mapData["height"];
        tileWidth = mapData["tilewidth"];
        tileHeight = mapData["tileheight"];
        
        // Parse tile layers with safety check
        if (mapData.contains("layers") && mapData["layers"].is_array()) {
            for (const auto& layerData : mapData["layers"]) {
                if (layerData.contains("type") && layerData["type"] == "tilelayer") {
                    auto layer = std::make_unique<TileLayer>();
                    if (layer->parseLayerData(layerData)) {
                        layers.push_back(std::move(layer));
                    }
                }
            }
        }
        
        // Parse tilesets with safety check
        if (mapData.contains("tilesets") && mapData["tilesets"].is_array()) {
            if (!tileSetManager->parseTileSets(mapData["tilesets"])) {
                return false;
            }
        } else {
            Logger::error("Map file missing tilesets array");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Error parsing map file: " + std::string(e.what()));
        return false;
    }
}


/**
 * @brief Loads all tilesets referenced by the map.
 * 
 * @return true if all tilesets were loaded successfully, false otherwise.
 */
bool MapLoader::loadTileSets() {
    return tileSetManager->loadAllTileSets(mapDirectory);
}


/**
 * @brief Renders all layers of the loaded map using the provided renderer.
 * 
 * @param renderer Pointer to the Renderer instance used for drawing.
 */
void MapLoader::render(Renderer* renderer) {
    if (!renderer) return;

    // If a TMJMap is loaded, render it
    if (currentTMJMap) {
        // Debug information
        Logger::debug(
            "Rendering TMJ map - Tiles count: " + 
            std::to_string(currentTMJMap->getTiles().size())
        );

        // Render all tiles from TMJMap
        for (const auto& tile : currentTMJMap->getTiles()) {
            renderer->drawSprite(tile);
        }

        Logger::debug(
            "Finished rendering " + 
            std::to_string(currentTMJMap->getTiles().size()) + 
            " tiles from TMJMap"
        );
    } 
    // Otherwise fall back to the legacy tile layer system
    else {
        Logger::debug("Rendering legacy tile layers");
        for (auto& layer : layers) {
            layer->render(
                renderer, 
                tileSetManager.get(), 
                tileWidth, 
                tileHeight
            );
        }
    }
}


/**
 * @brief Cleans up all resources associated with the loaded map.
 */
void MapLoader::cleanup() {
    layers.clear();
    tileSetManager->cleanup();
    properties.clear();
}


std::shared_ptr<TMJMap> MapLoader::loadTMJMap(
    const std::string& filepath, 
    int extrude
) {
    currentTMJMap = std::make_shared<TMJMap>();
    
    if (!currentTMJMap->loadFromFile(filepath, extrude)) {
        Logger::error("Failed to load TMJ map: " + filepath);
        currentTMJMap.reset();
        return nullptr;
    }
    
    applySpawnFromSidecar(filepath, *currentTMJMap);

    // 新增：打印交互对象解析结果（关键调试日志）
    auto interactionObjs = currentTMJMap->getInteractionObjects();
    Logger::info("Loaded " + std::to_string(interactionObjs.size()) + " interaction objects from TMJ map");
    for (const auto& io : interactionObjs) {
        if (io.type == "counter") {
            Logger::info("  Counter found: " + io.name + " | Rect: (" + 
                         std::to_string(io.rect.position.x) + "," + std::to_string(io.rect.position.y) + 
                         ") " + std::to_string(io.rect.size.x) + "x" + std::to_string(io.rect.size.y));
        }
    }

    // Extract map directory for future reference
    size_t lastSlash = filepath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        mapDirectory = filepath.substr(0, lastSlash + 1);
    }
    // Record current map path (use generic string for consistent keying)
    currentMapPath = filepath;

    Logger::info("TMJMap loaded successfully: " + filepath);
    return currentTMJMap;
}


// MapLoader.cpp - additional helper method
void MapLoader::applySpawnFromSidecar(const std::string& tmjPath, TMJMap& map) {
    namespace fs = std::filesystem;

    fs::path tmjFilePath(tmjPath);
    fs::path sidecarPath = tmjFilePath.parent_path() / "spawns.json";

    if (!fs::exists(sidecarPath)) {
        return;
    }

    std::ifstream file(sidecarPath);
    if (!file.is_open()) {
        return;
    }

    try {
        nlohmann::json j;
        file >> j;

        // Try several keys in order:
        // 1) the exact tmjPath passed in (e.g. "maps/lower_campus_map.tmj")
        // 2) the filename only (e.g. "lower_campus_map.tmj")
        // 3) the filesystem generic string (useful if JSON keys use forward slashes)
        std::string key_full = tmjPath;
        std::string key_file = tmjFilePath.filename().string();
        std::string key_generic = tmjFilePath.generic_string();

        const nlohmann::json* cfg = nullptr;
        if (j.contains(key_full)) cfg = &j[key_full];
        else if (j.contains(key_file)) cfg = &j[key_file];
        else if (j.contains(key_generic)) cfg = &j[key_generic];
        else {
            // Not found
            return;
        }

        const nlohmann::json& spawnData = *cfg;
        std::string mode = spawnData.value("mode", "tile");

        if (mode == "tile") {
            int tx = spawnData.value("x", 0);
            int ty = spawnData.value("y", 0);
            float px = (tx + 0.5f) * map.getTileWidth();
            float py = (ty + 0.5f) * map.getTileHeight();
            map.setSpawnPoint(px, py);
        } else if (mode == "pixel") {
            float px = spawnData.value("x", 0.0f);
            float py = spawnData.value("y", 0.0f);
            map.setSpawnPoint(px, py);
        }
    } catch (...) {
        Logger::warn("Failed to parse spawns.json for: " + tmjPath);
    }
}

// Spawn override helpers
void MapLoader::setSpawnOverride(const std::string& mapKey, float x, float y) {
    spawnOverrides[mapKey] = sf::Vector2f{x, y};
}

/**
 * @brief Retrieve a previously stored spawn override for a map key.
 *
 * @param mapKey Map identifier to query.
 * @return optional containing the stored world position or std::nullopt if none.
 */
std::optional<sf::Vector2f> MapLoader::getSpawnOverride(const std::string& mapKey) const {
    auto it = spawnOverrides.find(mapKey);
    if (it != spawnOverrides.end()) return it->second;
    return std::nullopt;
}

/**
 * @brief Resolve the spawn position for a map with optional consume behavior.
 *
 * Resolution order:
 * 1) stored spawn override
 * 2) TMJ embedded spawn
 * 3) map center fallback
 *
 * If consume==true and an override exists, it will be removed after resolving.
 */
sf::Vector2f MapLoader::resolveSpawnForMap(const std::string& mapKey, const TMJMap& map, bool consume) const {
    if (!mapKey.empty()) {
        auto it = spawnOverrides.find(mapKey);
        if (it != spawnOverrides.end()) {
            sf::Vector2f v = it->second;
            if (consume) {
                const_cast<MapLoader*>(this)->spawnOverrides.erase(it);
            }
            return v;
        }
    }
    if (map.getSpawnX() && map.getSpawnY()) {
        return sf::Vector2f(*map.getSpawnX(), *map.getSpawnY());
    }
    return sf::Vector2f(map.getWorldPixelWidth() * 0.5f, map.getWorldPixelHeight() * 0.5f);
}


void MapLoader::clearSpawnOverride(const std::string& mapKey) {
    spawnOverrides.erase(mapKey);
}

void MapLoader::clearAllSpawnOverrides() {
    spawnOverrides.clear();
}