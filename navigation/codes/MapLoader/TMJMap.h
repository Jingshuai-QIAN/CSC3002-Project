// TMJMap.h
#pragma once

// Map object lightweight types (TextObject, EntranceArea, BlockPoly).
#include "MapObjects.h"

// SFML types for sprites and images.
#include <SFML/Graphics.hpp>

// JSON is used by the implementation; included here for convenience in signatures.
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <optional>
#include <memory>

// Forward declaration for tileset manager used by TMJ loading logic.
class TileSetManager;

/**
 * @struct TilesetInfo
 * @brief Container for tileset metadata and the associated extruded texture.
 *
 * The texture field owns the SFML texture used to draw tiles from this set.
 */
struct TilesetInfo {
    int firstGid = 1;  
    int tileCount = 0; 
    int columns = 0;    

    int tileWidth = 0;
    int tileHeight = 0;
    int spacing = 0;
    int margin = 0;

    int origTileW = 0;
    int origTileH = 0;
    int origSpacing = 0;
    int origMargin = 0;

    std::string name;
    std::string imagePath;
    sf::Texture texture; 
};

/*
 * Class: TMJMap
 * Description: Represents a fully loaded TMJ map including tiles and object layers.
 *
 * Responsibilities:
 *   - Parse a TMJ JSON file and load tileset textures (with optional extrusion).
 *   - Provide accessors for tiles, text objects, entrance areas and spawn point.
 *   - Store NotWalkable regions (rectangles and polygons) and answer feet-block queries.
 *
 * Notes:
 *   - TMJMap stores SFML sprites referencing internal textures; ensure the
 *     TMJMap instance outlives any rendering usage that references its textures.
 */
class TMJMap : public sf::Drawable {
public:
    /**
     * @brief Load a TMJ map from a JSON file.
     * 
     * @param filepath Path to the TMJ JSON file.
     * @param extrude Number of extruded border pixels for tileset textures.
     * @return true if the map was successfully loaded, false otherwise.
     */
    bool loadFromFile(
        const std::string& filepath, 
        int extrude = 1
    );

    /**
     * @brief Clean up all resources associated with the loaded map.
     */
    void cleanup();
    
    // Getters
    int getMapWidthTiles() const { return mapWidthTiles; }
    int getMapHeightTiles() const { return mapHeightTiles; }
    int getTileWidth() const { return tileWidth; }
    int getTileHeight() const { return tileHeight; }
    int getWorldPixelWidth() const { return mapWidthTiles * tileWidth; }
    int getWorldPixelHeight() const { return mapHeightTiles * tileHeight; }
    const std::vector<InteractionObject>& getInteractionObjects() const { return interactionObjects; }
    const std::vector<sf::Sprite>& getTiles() const { return tiles; }
    const std::vector<TextObject>& getTextObjects() const { return textObjects; }
    const std::vector<EntranceArea>& getEntranceAreas() const { return entranceAreas; }
    const std::vector<GameTriggerArea>& getGameTriggers() const { return gameTriggers; }
    const std::optional<float>& getSpawnX() const { return spawnX; }
    const std::optional<float>& getSpawnY() const { return spawnY; }
    const std::vector<Chef>& getChefs() const { return m_chefs; }
    const std::vector<Professor>& getProfessors() const { return m_professors; }
    const std::vector<TableObject>& getTables() const { return m_tables; }
    const std::vector<FoodAnchor>& getFoodAnchors() const { return m_foodAnchors; }
    const std::vector<LawnArea>& getLawnAreas() const { return lawnAreas; }
    const std::vector<ShopTrigger>& getShopTriggers() const { return m_shopTriggers; }
    const RespawnPoint& getRespawnPoint() const { return respawnPoint; }

    /**
     * @brief Set the spawn point coordinates.
     * 
     * @param x X coordinate of spawn point.
     * @param y Y coordinate of spawn point.
     */
    void setSpawnPoint(float x, float y) { spawnX = x; spawnY = y; }
    
    /**
     * @brief Check whether a feet point is blocked by NotWalkable regions.
     * 
     * @param feet The point to check (player's feet position).
     * @return true if the point is inside any non-walkable area, false otherwise.
     */
    bool feetBlockedAt(const sf::Vector2f& feet) const;

    /**
     * @brief Manual rendering method (fallback if draw override fails).
     * 
     * @param target Render target to draw to.
     */
    void render(sf::RenderTarget& target) const {
        for (const auto& tile : tiles) {
            target.draw(tile);
        }
    }

    /**
     * @brief Add a shop trigger area to the map.
     * 
     * @param shopTrigger Shop trigger area to add.
     */
    void addShopTrigger(const ShopTrigger& shopTrigger) {
        m_shopTriggers.push_back(shopTrigger);
    }

    /**
     * @brief Add a game trigger area to the map.
     * 
     * @param gameTrigger Game trigger area to add.
     */
    void addGameTrigger(const GameTriggerArea& gameTrigger) {
        gameTriggers.push_back(gameTrigger); 
    }

private:
    /**
     * @brief Override sf::Drawable's draw method (automatically called by window.draw(map)).
     * 
     * @param target Render target to draw to.
     * @param states Render states to apply.
     */
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        for (const auto& tile : tiles) {
            target.draw(tile, states);
        }
    }

    /**
     * @brief Parse the top-level TMJ JSON structure into internal map data.
     * 
     * @param j Parsed JSON object representing the TMJ file.
     * @param baseDir Base directory used to resolve relative tileset image paths.
     * @param extrude Extrusion amount passed to tileset texture creation.
     * @return true on success, false on parse or resource errors.
     */
    bool parseMapData(
        const nlohmann::json& j, 
        const std::string& baseDir, 
        int extrude
    );

    /**
     * @brief Load tileset definitions and build textures (optionally extruded).
     *
     * @param tilesetsData JSON array of tileset descriptors.
     * @param baseDir Base directory for resolving tileset image paths.
     * @param extrude Extrusion pixel count to add around tiles when building textures.
     * @return true if tilesets loaded successfully; false if critical resources are missing.
     */
    bool loadTilesets(
        const nlohmann::json& tilesetsData, 
        const std::string& baseDir, 
        int extrude
    );

    /**
     * @brief Create an extruded texture image from a tileset source image.
     *
     * @param src Source image containing the tileset.
     * @param srcTileW Original tile width in pixels.
     * @param srcTileH Original tile height in pixels.
     * @param columns Number of tile columns in the source image.
     * @param spacing Pixel spacing between tiles in the source.
     * @param margin Pixel margin around tiles in the source.
     * @param extrude Number of pixels to extrude around each tile.
     * @param outTex Output texture to populate from the generated image.
     * @return true if the extruded texture was created successfully.
     */
    bool makeExtrudedTexture(
        const sf::Image& src, 
        int srcTileW, 
        int srcTileH,
        int columns, 
        int spacing, 
        int margin, 
        int extrude, 
        sf::Texture& outTex
    );

    /**
     * @brief Parse object layers from TMJ JSON and populate runtime object lists.
     *
     * @param layers JSON array containing TMJ layer objects.
     */
    void parseObjectLayers(const nlohmann::json& layers);

    /**
     * @brief Find the tileset information that contains a given global tile ID (gid).
     *
     * @param gid Global tile ID to lookup.
     * @return Pointer to matching TilesetInfo or nullptr if not found.
     */
    TilesetInfo* findTilesetForGid(int gid);
    
private:
    int mapWidthTiles = 0;
    int mapHeightTiles = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    
    std::vector<TilesetInfo> tilesets;
    std::vector<sf::Sprite> tiles;
    std::vector<TextObject> textObjects;
    std::vector<EntranceArea> entranceAreas;
    std::vector<GameTriggerArea> gameTriggers;
    std::vector<Chef> m_chefs;
    std::vector<Professor> m_professors;
    std::vector<InteractionObject> interactionObjects;
    std::vector<sf::FloatRect> notWalkRects; 
    std::vector<BlockPoly>     notWalkPolys; 
    
    std::optional<float> spawnX;
    std::optional<float> spawnY;

    std::vector<TableObject> m_tables;
    std::vector<FoodAnchor> m_foodAnchors;

    std::vector<LawnArea> lawnAreas;
    std::vector<ShopTrigger> m_shopTriggers;
    RespawnPoint respawnPoint;  
};
