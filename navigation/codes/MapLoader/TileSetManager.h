// Prevent multiple inclusions of this header file
#pragma once

// Standard includes used by the tileset manager.
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

// SFML for texture representation.
#include <SFML/Graphics.hpp>

// JSON library used by the implementation to parse tileset metadata.
#include <nlohmann/json.hpp>

/**
 * @file TileSetManager.h
 * @brief Tileset parsing and texture management interface.
 *
 * The TileSetManager is responsible for parsing tileset metadata from JSON
 * and loading textures referenced by tileset definitions. Loaded textures are
 * cached and owned via shared_ptr to allow sprites to reference them safely.
 */
/**
 * @struct TileSet
 * @brief Represents a tileset containing tile images and metadata.
 *
 * The tileset holds image dimensions, tile size and a shared texture pointer.
 */
struct TileSet {
    int firstGid;           ///< First global ID of the tileset
    std::string name;       ///< Name identifier of the tileset

    std::string imagePath;  ///< File path to the tileset image
    int imageWidth;         ///< Width of the tileset image in pixels
    int imageHeight;        ///< Height of the tileset image in pixels

    int tileWidth;          ///< Width of individual tiles in pixels
    int tileHeight;         ///< Height of individual tiles in pixels
    int tileCount;          ///< Total number of tiles in the tileset

    int columns;            ///< Number of tile columns in the tileset image
    
    int spacing = 0;        ///< Spacing between tiles in pixels
    int margin = 0;         ///< Margin around the tileset image in pixels

    std::shared_ptr<sf::Texture> texture = nullptr;  ///< SFML texture object
};

/**
 * @class TileSetManager
 * @brief Manages loading, storage, and retrieval of tilesets and their textures.
 * 
 * This class handles parsing tileset definitions from JSON, loading texture images,
 * and providing access to tilesets based on global tile IDs.
 */
class TileSetManager {
public:
    /**
     * @brief Parses tileset data from JSON format.
     * 
     * @param tilesetsData JSON array containing tileset definitions.
     * @return true if parsing was successful, false otherwise.
     */
    bool parseTileSets(const nlohmann::json& tilesetsData);
    
    /**
     * @brief Loads all tileset textures from disk.
     * 
     * @param basePath Base directory path for resolving relative image paths.
     * @return true if all textures were loaded successfully, false otherwise.
     */
    bool loadAllTileSets(const std::string& basePath);
    
    /**
     * @brief Releases all loaded textures and clears tileset data.
     */
    void cleanup();
    
    /**
     * @brief Finds the appropriate tileset for a given global tile ID.
     * 
     * @param gid Global tile ID to search for.
     * @return TileSet* Pointer to the tileset containing the tile, or nullptr if not found.
     */
    TileSet* getTileSetForGid(int gid) const;
    
    /**
     * @brief Retrieves the texture for a specific tileset by name.
     * 
     * @param name Name of the tileset to find.
     * @return void* Platform-specific texture pointer, or nullptr if not found.
     */
    void* getTextureForTileSet(const std::string& name) const;

    /**
     * @brief Gets a shared pointer to the texture for a specific tileset by name.
     * 
     * @param name Name of the tileset to find.
     * @return std::shared_ptr<sf::Texture> Shared pointer to the texture, or nullptr if not found.
     */
    std::shared_ptr<sf::Texture> getSharedTextureForTileSet(const std::string& name) const;

    /**
     * @brief Gets all loaded tilesets.
     * 
     * @return const std::vector<TileSet>& Reference to the vector of all tilesets.
     */
    const std::vector<TileSet>& getAllTileSets() const;

    /**
     * @brief Checks if a tileset with the given name exists.
     * 
     * @param name Name of the tileset to check.
     * @return true if the tileset exists, false otherwise.
     */
    bool hasTileSet(const std::string& name) const;

    /**
     * @brief Gets the total number of loaded tilesets.
     * 
     * @return int Number of tilesets.
     */
    int getTotalTileSets() const;
    
private:
    /**
     * @brief Loads the texture image for a single tileset.
     * 
     * @param tileSet Reference to the tileset to load texture for.
     * @param basePath Base directory path for resolving relative image paths.
     * @return true if texture loading was successful, false otherwise.
     */
    bool loadTileSetTexture(
        TileSet& tileSet, 
        const std::string& basePath
    );
    
private:
    std::vector<TileSet> tileSets;  ///< Collection of all loaded tilesets

    std::unordered_map<
        std::string, 
        std::shared_ptr<sf::Texture>
    > textures;  ///< Texture cache mapped by tileset name
};