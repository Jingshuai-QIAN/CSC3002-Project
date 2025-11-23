// TileSetManager.cpp
#include "TileSetManager.h"    // TileSetManager declarations
#include "Utils/Logger.h"      // Logging helper
#include "Renderer/Renderer.h" // Optional renderer dependency
#include <fstream>             // std::ifstream
#include <filesystem>          // std::filesystem utilities

/**
 * @file TileSetManager.cpp
 * @brief Implementation of tileset parsing and texture loading.
 *
 * Responsibilities:
 * - Parse tileset metadata from JSON arrays provided by the map loader.
 * - Load and cache SFML textures for tilesets, exposing shared_ptr accessors.
 *
 * Notes:
 * - Textures are stored as shared_ptr in both the TileSet struct and a name-indexed cache.
 * - File system paths are resolved using std::filesystem::path.
 */
bool TileSetManager::parseTileSets(const nlohmann::json& tilesetsData) {
    try {
        // Check whether tilesets data is an array
        if (!tilesetsData.is_array()) {
            Logger::error("Tilesets data is not an array");
            return false;
        }
        
        // Iterate through each tileset in the JSON data
        for (const auto& tsData : tilesetsData) {
            TileSet tileSet;  // Create a new TileSet object
            
            // Check if necessary keys exist
            if (!tsData.contains("firstgid") || !tsData.contains("name") ||
                !tsData.contains("tilewidth") || !tsData.contains("tileheight") ||
                !tsData.contains("tilecount") || !tsData.contains("columns")) {
                Logger::error("Tileset missing required fields");
                continue;
            }
            
            // Parse basic tileset properties from JSON
            tileSet.firstGid = tsData["firstgid"];      // First global ID in this tileset
            tileSet.name = tsData["name"];              // Tileset name identifier
            tileSet.tileWidth = tsData["tilewidth"];    // Width of individual tiles
            tileSet.tileHeight = tsData["tileheight"];  // Height of individual tiles
            tileSet.tileCount = tsData["tilecount"];    // Total number of tiles in set
            tileSet.columns = tsData["columns"];        // Number of columns in tileset image
            
            // Parse image properties if present in JSON
            if (tsData.contains("image") && tsData["image"].is_string()) {
                tileSet.imagePath = tsData["image"];      // Path to tileset image file
                
                // Safely access image size
                if (tsData.contains("imagewidth")) {
                    tileSet.imageWidth = tsData["imagewidth"];
                }
                if (tsData.contains("imageheight")) {
                    tileSet.imageHeight = tsData["imageheight"];
                }
            } else {
                Logger::error("Tileset missing image path: " + tileSet.name);
                continue;  // Skip tilesets without image path
            }
            
            // Parse optional spacing and margin properties with default values
            tileSet.spacing = tsData.value("spacing", 0); // Space between tiles (pixels)
            tileSet.margin = tsData.value("margin", 0);   // Margin around tileset (pixels)
            
            // Add the parsed tileset to the collection
            tileSets.push_back(tileSet);
            
            // Log successful tileset parsing
            Logger::info("Parsed tileset: " + tileSet.name + 
                        " (GID: " + std::to_string(tileSet.firstGid) + 
                        ", Tiles: " + std::to_string(tileSet.tileCount) + ")");
        }
        
        // Check whether any tileset is successfully parsed
        if (tileSets.empty()) {
            Logger::error("No valid tilesets were parsed");
            return false;
        }
        
        return true;  // Return success if all tilesets parsed
    } catch (const std::exception& e) {
        // Log any exceptions during parsing
        Logger::error("Error parsing tilesets: " + std::string(e.what()));
        return false;  // Return failure on exception
    }
}


/**
 * @brief Loads texture resources for all parsed tilesets.
 * @param basePath Base directory path for tileset image files.
 * @return True if all textures loaded successfully, false otherwise.
 */
bool TileSetManager::loadAllTileSets(const std::string& basePath) {
    bool allLoaded = true;  // Track overall success status
    
    // Iterate through all tilesets and load their textures
    for (auto& tileSet : tileSets) {
        if (!loadTileSetTexture(tileSet, basePath)) {
            // Log failure for this tileset
            Logger::error("Failed to load texture for tileset: " + tileSet.name);
            allLoaded = false;  // Mark overall operation as failed
        } else {
            // Cache the loaded texture in the textures map
            textures[tileSet.name] = tileSet.texture;
        }
    }
    
    return allLoaded;  // Return overall success status
}


/**
 * @brief Loads texture from file for a specific tileset.
 * @param tileSet Reference to the tileset to load texture for.
 * @param basePath Base directory path for image files.
 * @return True if texture loaded successfully, false otherwise.
 */
bool TileSetManager::loadTileSetTexture(TileSet& tileSet, const std::string& basePath) {
    namespace fs = std::filesystem;  // Filesystem namespace alias
    
    // Construct full path to tileset image file
    fs::path fullPath = fs::path(basePath) / tileSet.imagePath;
    
    // Check if image file exists
    if (!fs::exists(fullPath)) {
        Logger::error("Tileset image not found: " + fullPath.string());
        return false;  // Return failure if file doesn't exist
    }
    
    // Load texture using SFML 3.0.2
    sf::Texture texture;
    if (!texture.loadFromFile(fullPath.string())) {
        Logger::error("Failed to load texture from: " + fullPath.string());
        return false;  // Return failure if texture loading fails
    }
    
    // Configure texture parameters for pixel art (no smoothing)
    texture.setSmooth(false);
    
    // Transfer texture to tileset as shared pointer
    tileSet.texture = std::make_shared<sf::Texture>(std::move(texture));
    
    // Log successful texture loading
    Logger::info("Loaded tileset texture: " + fullPath.string() + 
                " (" + std::to_string(tileSet.imageWidth) + "x" + 
                std::to_string(tileSet.imageHeight) + ")");
    
    return true;  // Return success
}


/**
 * @brief Finds the tileset that contains a specific global ID (GID).
 * @param gid The global ID to search for.
 * @return Pointer to the containing tileset, or nullptr if not found.
 */
TileSet* TileSetManager::getTileSetForGid(int gid) const {
    TileSet* result = nullptr;  // Initialize result pointer
    
    // Iterate through all tilesets to find the one containing the GID
    for (const auto& tileSet : tileSets) {
        // Check if GID falls within this tileset's range
        if (gid >= tileSet.firstGid && gid < tileSet.firstGid + tileSet.tileCount) {
            // Select the tileset with the highest firstGid (most specific match)
            if (!result || tileSet.firstGid > result->firstGid) {
                result = const_cast<TileSet*>(&tileSet);  // Remove const for return
            }
        }
    }
    
    return result;  // Return found tileset or nullptr
}


/**
 * @brief Retrieves raw texture pointer for a tileset by name.
 * @param name Name identifier of the tileset.
 * @return Raw pointer to the texture, or nullptr if not found.
 */
void* TileSetManager::getTextureForTileSet(const std::string& name) const {
    // First check texture cache
    auto it = textures.find(name);

    if (it != textures.end()) {
        return it->second.get();  // Return raw pointer from shared_ptr
    }
    
    // If not in cache, search through tilesets directly
    for (const auto& tileSet : tileSets) {
        if (tileSet.name == name && tileSet.texture) {
            return tileSet.texture.get();
        }
    }
    
    return nullptr;
}


/**
 * @brief Retrieves shared texture pointer for a tileset by name.
 * @param name Name identifier of the tileset.
 * @return Shared pointer to the texture, or nullptr if not found.
 */
std::shared_ptr<sf::Texture> TileSetManager::getSharedTextureForTileSet(const std::string& name) const {
    // First check texture cache
    auto it = textures.find(name);
    if (it != textures.end()) {
        return it->second;  // Return shared_ptr from cache
    }
    
    // If not in cache, search through tilesets directly
    for (const auto& tileSet : tileSets) {
        if (tileSet.name == name) {
            return tileSet.texture;  // Return shared_ptr
        }
    }
    
    return nullptr;  // Return null if tileset not found
}


/**
 * @brief Cleans up all managed resources and resets the manager state.
 */
void TileSetManager::cleanup() {
    // Release texture resources from all tilesets
    for (auto& tileSet : tileSets) {
        tileSet.texture.reset();  // Release shared_ptr reference
    }
    
    // Clear all collections
    tileSets.clear();   // Clear tileset vector
    textures.clear();   // Clear texture cache
    
    Logger::info("TileSetManager cleanup completed");  // Log cleanup
}


/**
 * @brief Gets read-only access to all managed tilesets.
 * @return Const reference to vector of all tilesets.
 */
const std::vector<TileSet>& TileSetManager::getAllTileSets() const {
    return tileSets;  // Return const reference to tilesets
}


/**
 * @brief Checks if a tileset with the given name exists.
 * @param name Name identifier to check for.
 * @return True if tileset exists, false otherwise.
 */
bool TileSetManager::hasTileSet(const std::string& name) const {
    // Search through tilesets for matching name
    for (const auto& tileSet : tileSets) {
        if (tileSet.name == name) {
            return true;  // Return true if found
        }
    }
    return false;  // Return false if not found
}


/**
 * @brief Gets the total number of managed tilesets.
 * @return Count of tilesets in the manager.
 */
int TileSetManager::getTotalTileSets() const {
    return static_cast<int>(tileSets.size());  // Return vector size as int
}