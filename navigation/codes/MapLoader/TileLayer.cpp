// TileLayer.cpp
// Implementation file for the TileLayer class responsible for handling tile-based layers

// Public header for the class
#include "TileLayer.h"

// Supporting components
#include "TileSetManager.h"
#include "Renderer/Renderer.h"
#include "Utils/Logger.h"
#include <cmath>

/*
 * File: TileLayer.cpp
 * Description: JSON parsing and rendering implementation for a single tile layer.
 *
 * The TileLayer implementation:
 *   - Parses layer metadata and tile ID arrays from a JSON object.
 *   - Renders the layer by resolving tile textures via TileSetManager.
 *
 * Notes:
 *   - Rendering uses Renderer::drawSprite which ultimately draws via SFML.
 */


/**
 * Parses JSON data to initialize the tile layer
 * @param layerData JSON object containing layer configuration
 * @return true if parsing succeeded, false otherwise
 */
bool TileLayer::parseLayerData(const nlohmann::json& layerData) {
    try {
        // Parse layer name
        if (layerData.contains("name") && layerData["name"].is_string()) {
            name = layerData["name"].get<std::string>();
        } else {
            // Default name if not specified
            name = "unnamed";
        }

        // Parse layer width
        if (layerData.contains("width") && layerData["width"].is_number_integer()) {
            width = layerData["width"].get<int>();
        } else {
            // Log error and return if width is missing or invalid
            Logger::error("TileLayer: Missing or invalid width");
            return false;
        }

        // Parse layer height
        if (layerData.contains("height") && layerData["height"].is_number_integer()) {
            height = layerData["height"].get<int>();
        } else {
            // Log error and return if height is missing or invalid
            Logger::error("TileLayer: Missing or invalid height");
            return false;
        }

        // Parse visibility flag
        if (layerData.contains("visible") && layerData["visible"].is_boolean()) {
            visible = layerData["visible"].get<bool>();
        }

        // Parse opacity value
        if (layerData.contains("opacity") && layerData["opacity"].is_number()) {
            opacity = layerData["opacity"].get<float>();
            // Clamp opacity to valid range [0.0, 1.0]
            opacity = std::clamp(opacity, 0.0f, 1.0f);
        }

        // Parse tile data array
        if (layerData.contains("data") && layerData["data"].is_array()) {
            data = layerData["data"].get<std::vector<int>>();
            
            // Validate data size matches layer dimensions
            if (static_cast<int>(data.size()) != width * height) {
                Logger::error(
                    "TileLayer: Data size mismatch. Expected " + 
                    std::to_string(width * height) + 
                    " tiles, got " + 
                    std::to_string(data.size())
                );
                return false;
            }
        } else {
            // Log error and return if data array is missing or invalid
            Logger::error("TileLayer: Missing or invalid data array");
            return false;
        }

        // Log successful parsing with layer details
        Logger::debug(
            "TileLayer '" + name + "' parsed: " + 
            std::to_string(width) + "x" + std::to_string(height) + 
            ", visible: " + std::to_string(visible) + 
            ", opacity: " + std::to_string(opacity)
        );

        return true;
    }
    catch (const std::exception& e) {
        // Log any exceptions during parsing
        Logger::error("TileLayer: Exception during parsing: " + std::string(e.what()));
        return false;
    }
}


/**
 * Renders the tile layer to the screen
 * @param renderer Pointer to the renderer for drawing operations
 * @param tileSetManager Pointer to the tile set manager for texture retrieval
 * @param tileWidth Width of individual tiles in pixels
 * @param tileHeight Height of individual tiles in pixels
 */
void TileLayer::render(
    Renderer* renderer, 
    TileSetManager* tileSetManager, 
    int tileWidth, 
    int tileHeight
) {
    // Early return if preconditions are not met
    if (!renderer || !tileSetManager || !visible) {
        return;
    }

    // Iterate through all tiles in the layer
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Get global tile ID at current position
            int gid = data[x + y * width];
            // Skip empty tiles (GID 0)
            if (gid == 0) {
                continue;
            }

            // Find the appropriate tile set for this GID
            TileSet* tileSet = tileSetManager->getTileSetForGid(gid);
            // Skip if tile set not found or has no texture
            if (!tileSet || !tileSet->texture) {
                continue;
            }

            // Calculate local tile ID within the tile set
            int localId = gid - tileSet->firstGid;
            // Skip if local ID is out of valid range
            if (localId < 0 || localId >= tileSet->tileCount) {
                continue;
            }

            // Calculate texture coordinates within the tile set
            int tileX = localId % tileSet->columns;
            int tileY = localId / tileSet->columns;

            // Calculate texture rectangle coordinates
            int textureX = tileSet->margin + tileX * (tileSet->tileWidth + tileSet->spacing);
            int textureY = tileSet->margin + tileY * (tileSet->tileHeight + tileSet->spacing);

            // Create sprite and set its texture
            sf::Sprite tileSprite(*tileSet->texture);

            // Set the texture rectangle to select the correct tile
            tileSprite.setTextureRect({
                {textureX, textureY}, 
                {tileSet->tileWidth, tileSet->tileHeight}
            });

            // Calculate and set world position
            float posX = static_cast<float>(x * tileWidth);
            float posY = static_cast<float>(y * tileHeight);
            tileSprite.setPosition(sf::Vector2f(posX, posY));

            // Apply opacity if less than fully opaque
            if (opacity < 1.0f) {
                sf::Color color = tileSprite.getColor();
                // Calculate alpha value based on opacity
                color.a = static_cast<uint8_t>(std::lround(255.0f * opacity));
                tileSprite.setColor(color);
            }

            // Draw the tile sprite to the renderer
            renderer->drawSprite(tileSprite);
        }
    }
}