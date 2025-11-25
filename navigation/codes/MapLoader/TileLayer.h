// Prevent multiple inclusions of this header file
#pragma once

// Standard headers for container and string types.
#include <vector>
#include <string>

// JSON parsing library used by the parseLayerData API.
#include <nlohmann/json.hpp>

// Forward declarations to keep compilation units small.
class Renderer;
class TileSetManager;

/*
 * File: TileLayer.h
 * Description: TileLayer represents a single tile layer read from a map file.
 *
 * The TileLayer class provides parsing of JSON layer data and a render
 * function that delegates drawing to the Renderer while using the
 * TileSetManager to resolve tile textures.
 *
 * It stores layer metadata and the flattened tile ID array used for rendering.
 */
class TileLayer {
public:
    /**
     * @brief Parses layer data from JSON format.
     * 
     * @param layerData JSON object containing layer configuration and tile data.
     * @return true if parsing was successful, false otherwise.
     */
    bool parseLayerData(const nlohmann::json& layerData);
    
    /**
     * @brief Renders the tile layer to the screen.
     * 
     * @param renderer Pointer to the Renderer instance used for drawing.
     * @param tileSetManager Pointer to the TileSetManager for accessing tile textures.
     * @param tileWidth Width of individual tiles in pixels.
     * @param tileHeight Height of individual tiles in pixels.
     */
    void render(
        Renderer* renderer, 
        TileSetManager* tileSetManager, 
        int tileWidth, 
        int tileHeight
    );
    
    /**
     * @brief Gets the name of this tile layer.
     * 
     * @return const std::string& Reference to the layer's name.
     */
    const std::string& getName() const { return name; }
    
    /**
     * @brief Gets the width of the layer in tiles.
     * 
     * @return int The layer width in tiles.
     */
    int getWidth() const { return width; }
    
    /**
     * @brief Gets the height of the layer in tiles.
     * 
     * @return int The layer height in tiles.
     */
    int getHeight() const { return height; }
    
private:
    std::string name;           ///< Name identifier for the layer
    int width = 0;              ///< Width of the layer in tiles
    int height = 0;             ///< Height of the layer in tiles
    std::vector<int> data;      ///< 1D array of tile IDs representing the layer layout
    float opacity = 1.0f;       ///< Opacity level for rendering (0.0f to 1.0f)
    bool visible = true;        ///< Visibility flag for the layer
};