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
    int firstGid = 1;   // first global tile id in this tileset
    int tileCount = 0;  // number of tiles in the source tileset
    int columns = 0;    // number of columns in the tileset image

    // Effective tile dimensions in the texture after extrusion.
    int tileWidth = 0;
    int tileHeight = 0;
    int spacing = 0;
    int margin = 0;

    // Original (source) tileset parameters from TMJ.
    int origTileW = 0;
    int origTileH = 0;
    int origSpacing = 0;
    int origMargin = 0;

    std::string name;
    std::string imagePath;
    sf::Texture texture; // owned texture (extruded or original)
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
    bool loadFromFile(
        const std::string& filepath, 
        int extrude = 1
    );
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
    
    void setSpawnPoint(float x, float y) { spawnX = x; spawnY = y; }
    
    // Collision query: check whether a feet point is blocked by NotWalkable regions
    bool feetBlockedAt(const sf::Vector2f& feet) const;

    // 手动渲染方法（备用，若 draw 重载失效时调用）
    void render(sf::RenderTarget& target) const {
        for (const auto& tile : tiles) {
            target.draw(tile);
        }
    }


private:

    // 重载 sf::Drawable 的 draw 方法（被 window.draw(map) 自动调用）
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        // 遍历所有瓦片并绘制
        for (const auto& tile : tiles) {
            target.draw(tile, states);
        }
    }

    bool parseMapData(
        const nlohmann::json& j, 
        const std::string& baseDir, 
        int extrude
    );

    bool loadTilesets(
        const nlohmann::json& tilesetsData, 
        const std::string& baseDir, 
        int extrude
    );

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

    void parseObjectLayers(const nlohmann::json& layers);
    
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
    
    // Not-walkable regions parsed from object layers
    std::vector<sf::FloatRect> notWalkRects; // rectangle blocking regions
    std::vector<BlockPoly>     notWalkPolys; // polygonal blocking regions
    
    std::optional<float> spawnX;
    std::optional<float> spawnY;

    // 新增：桌子和食物锚点存储（对应 TMJMap.cpp 中的解析逻辑）
    std::vector<TableObject> m_tables;
    std::vector<FoodAnchor> m_foodAnchors;

    std::vector<LawnArea> lawnAreas;
    std::vector<ShopTrigger> m_shopTriggers;
};
