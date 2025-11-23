// Prevent multiple inclusions of this header file
#pragma once

// Standard library includes used throughout the header.
#include <string>        // std::string
#include <vector>        // std::vector
#include <memory>        // std::unique_ptr, std::shared_ptr
#include <unordered_map> // std::unordered_map

// Project headers for map components.
#include "TileLayer.h"      // Tile layer representation
#include "TileSetManager.h" // Tileset resource manager
#include "TMJMap.h"         // TMJMap definitions

// Forward declaration for rendering integration.
class Renderer;

/**
 * @file MapLoader.h
 * @brief High-level map loading and management interface.
 *
 * The MapLoader encapsulates two loading modes:
 *  - Legacy tile layer parsing (TileLayer / TileSetManager).
 *  - TMJ map loading (TMJMap) which includes object layers.
 *
 * Responsibilities:
 * - Parse map files and manage tileset resources.
 * - Provide a simple render(entry) to draw loaded maps via Renderer.
 *
 * Notes:
 * - When a TMJMap is loaded, TMJMap sprites/textures are preferred for rendering.
 * - The class stores a directory path used to resolve relative tileset images.
 */
class MapLoader {
public:
    /**
     * @brief Constructs a new MapLoader object.
     */
    MapLoader();
    
    /**
     * @brief Destroys the MapLoader object and cleans up resources.
     */
    ~MapLoader();
    
    /**
     * @brief Loads a map from the specified file path.
     * 
     * @param filepath The path to the map file to load.
     * @return true if the map was loaded successfully, false otherwise.
     */
    bool loadMap(const std::string& filepath);
    
    /**
     * @brief Renders all layers of the loaded map using the provided renderer.
     * 
     * @param renderer Pointer to the Renderer instance used for drawing.
     */
    void render(Renderer* renderer);
    
    /**
     * @brief Cleans up all resources associated with the loaded map.
     */
    void cleanup();
    
    /**
     * @brief Gets the width of the map in tiles.
     * 
     * @return int The map width in tiles.
     */
    int getMapWidth() const { return mapWidth; }
    
    /**
     * @brief Gets the height of the map in tiles.
     * 
     * @return int The map height in tiles.
     */
    int getMapHeight() const { return mapHeight; }
    
    /**
     * @brief Gets the width of an individual tile in pixels.
     * 
     * @return int The tile width in pixels.
     */
    int getTileWidth() const { return tileWidth; }
    
    /**
     * @brief Gets the height of an individual tile in pixels.
     * 
     * @return int The tile height in pixels.
     */
    int getTileHeight() const { return tileHeight; }

    /**
     * @brief Loads a complete TMJ map including object layers
     * @param filepath Path to the TMJ file
     * @param extrude Extrusion amount for texture bleeding prevention
     * @return Shared pointer to loaded TMJMap, or nullptr on failure
     */
    std::shared_ptr<TMJMap> loadTMJMap(
        const std::string& filepath, 
        int extrude = 1
    );
    
    /**
     * @brief Gets the last loaded TMJ map
     */
    std::shared_ptr<TMJMap> getCurrentTMJMap() const { return currentTMJMap; }
    
private:
    /**
     * @brief Parses the map file at the given file path.
     * 
     * @param filepath The path to the map file to parse.
     * @return true if parsing was successful, false otherwise.
     */
    bool parseMapFile(const std::string& filepath);

    /**
     * @brief Applies spawn point override from sidecar spawns.json file
     * 
     * @param tmjPath Path to the TMJ map file
     * @param map Reference to the TMJMap to modify
     */
    void applySpawnFromSidecar(const std::string& tmjPath, TMJMap& map);
    
    /**
     * @brief Loads all tilesets referenced by the map.
     * 
     * @return true if all tilesets were loaded successfully, false otherwise.
     */
    bool loadTileSets();
    
private:
    // Map dimensions in tiles
    int mapWidth = 0;   ///< Width of the map in tiles
    int mapHeight = 0;  ///< Height of the map in tiles
    
    // Tile dimensions in pixels
    int tileWidth = 0;  ///< Width of a single tile in pixels
    int tileHeight = 0; ///< Height of a single tile in pixels
    
    // Directory where the map file is located
    std::string mapDirectory; ///< Directory path of the loaded map file
    
    // Collection of tile layers that make up the map
    std::vector<std::unique_ptr<TileLayer>> layers; ///< Vector of tile layers
    
    // Manager for handling tileset resources
    std::unique_ptr<TileSetManager> tileSetManager; ///< Manages tileset loading and storage
    
    // Map properties stored as key-value pairs
    std::unordered_map<std::string, std::string> properties; ///< Custom map properties

    std::shared_ptr<TMJMap> currentTMJMap;
};