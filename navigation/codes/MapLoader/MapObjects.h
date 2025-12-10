// MapObjects.h
#pragma once

// Standard includes used by the object definitions.
#include <string>
#include <vector>

// SFML types for colors and geometry.
#include <SFML/Graphics.hpp>
#include <optional>

/*
 * File: MapObjects.h
 * Description: Lightweight POD types representing objects parsed from Tiled TMJ maps.
 *
 * This header defines small structures used by rendering and gameplay code to
 * represent text labels, entrance rectangles and non-walkable polygons parsed
 * from TMJ object layers.
 *
 * Structures:
 *   - TextObject: stores a single text label and alignment data.
 *   - EntranceArea: rectangular area representing an entrance/trigger region.
 *   - BlockPoly: polygon used for non-walkable region checks.
 *
 * Notes:
 *   - Uses SFML vector and color types.
 *   - Structures are POD-like to simplify JSON parsing and copying.
 */

/*
 * Struct: TextObject
 * Description: Represents a text label placed on the map by Tiled.
 *
 * Fields:
 *   x, y        - top-left position in pixels.
 *   width,height- optional bounding rectangle for alignment.
 *   text        - label string.
 *   fontSize    - font size to render the label.
 *   bold,italic - style flags.
 *   color       - text color.
 *   halign,valign - horizontal/vertical alignment ("left"/"center"/"right", "top"/"center"/"bottom").
 */
struct TextObject {
    float x = 0.f, y = 0.f;
    float width = 0.f, height = 0.f;

    std::string text;

    unsigned int fontSize = 16;
    bool bold = false, italic = false;

    // Default white color for text.
    sf::Color color = sf::Color::White;

    // Horizontal and vertical alignment presets.
    std::string halign = "left";
    std::string valign = "top";
};

/*
 * Struct: InteractionObject
 * Description: Represents an interactive object on the map.
 *
 * Fields:
 *   type    - Interaction type (e.g., "counter").
 *   name    - Object name.
 *   rect    - Interaction area rectangle.
 *   options - List of interaction options (e.g., menu items).
 */
struct InteractionObject {
    std::string type;       
    std::string name;       
    sf::FloatRect rect;     
    std::vector<std::string> options; 
};

/*
 * Struct: EntranceArea
 * Description: Rectangle representing an entrance/trigger region placed in Tiled.
 *
 * The rectangle is expressed in pixel coordinates using x/y as top-left.
 * Fields include optional target path and optional explicit targetX/targetY
 * coordinates in the target map (pixels).
 */
struct EntranceArea {
    float x = 0.f, y = 0.f;
    float width = 0.f, height = 0.f;
    std::string name;
    std::string target;
    std::optional<float> targetX;
    std::optional<float> targetY;
};

/*
 * Struct: GameTriggerArea
 * Description: Represents a game trigger area on the map.
 *
 * Fields:
 *   x, y          - Top-left corner coordinates of the area.
 *   width, height - Size of the area.
 *   name          - Area name (e.g., "bookstore_game").
 *   gameType      - Mini-game type (used to distinguish different games).
 *   questionSet   - Optional: question bank ID (e.g., "classroom_basic").
 *   rect          - Rectangle bounding box for collision detection.
 */
struct GameTriggerArea {
    float x = 0.f, y = 0.f;       
    float width = 0.f, height = 0.f; 
    std::string name;             
    std::string gameType;        
    std::string questionSet;     
    sf::FloatRect rect;           
};

/*
 * Struct: Chef
 * Description: Represents a chef object on the map.
 *
 * Fields:
 *   name - Object name (can be used to distinguish different chefs).
 *   rect - Rectangle object position and size (exported from Tiled).
 */
struct Chef {
    std::string name;       
    sf::FloatRect rect;    
};

/*
 * Struct: Professor
 * Description: Represents a professor NPC on the map.
 *
 * Fields:
 *   name        - Professor name.
 *   rect        - Position and collision bounding box.
 *   course      - Course taught by the professor.
 *   dialogType  - Dialogue type.
 *   available   - Whether the professor is available for interaction.
 */
struct Professor {
    std::string name;
    sf::FloatRect rect;        
    std::string course;        
    std::string dialogType;    
    bool available;           
    
    // Constructor
    Professor() : available(true) {}
};

/**
 * @struct BlockPoly
 * @brief Polygonal blocking region parsed from a "NotWalkable" object layer.
 *
 * The points vector stores absolute pixel coordinates of polygon vertices.
 * The bounds field is the axis-aligned bounding box (AABB) used for fast rejection
 * before performing point-in-polygon tests.
 */
struct BlockPoly {
    std::vector<sf::Vector2f> points; // polygon vertices in world pixels
    sf::FloatRect bounds;             // AABB (SFML 3: position + size)

    // Default constructor with forced bounds initialization
    BlockPoly()
        : points(),
          bounds(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f)) {}

    // Convenience constructor for parsing TMJ data
    explicit BlockPoly(std::vector<sf::Vector2f> p)
        : points(std::move(p)),
          bounds(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f)) {}

    // Explicit safe copy operations
    BlockPoly(const BlockPoly&) = default;
    BlockPoly& operator=(const BlockPoly&) = default;

    // Explicit safe move operations
    BlockPoly(BlockPoly&&) noexcept = default;
    BlockPoly& operator=(BlockPoly&&) noexcept = default;
};

/*
 * Struct: TableObject
 * Description: Represents a dining table object on the map.
 *
 * Fields:
 *   name          - Must be in format left_table1/right_table1.
 *   rect          - Table area (left/top/width/height).
 *   seatPosition  - Chair insertion point coordinates (must be configured, non-zero).
 */
struct TableObject {
    std::string name;          
    sf::FloatRect rect;      
    sf::Vector2f seatPosition; 
};

/*
 * Struct: FoodAnchor
 * Description: Represents a food display point on the map.
 *
 * Fields:
 *   id          - Unique identifier (e.g., food1, food2...).
 *   position    - Display position coordinates.
 *   tableName   - Associated table name (e.g., left_table1, right_table2).
 */
struct FoodAnchor {
    std::string id;            
    sf::Vector2f position;     
    std::string tableName;    
};

/*
 * Struct: LawnArea
 * Description: Represents a lawn area on the map.
 *
 * Fields:
 *   name - Area name.
 *   rect - Area bounds (position: x,y; size: width,height).
 */
struct LawnArea {
    std::string name;
    sf::FloatRect rect; 

    LawnArea() = default;
    LawnArea(std::string n, float x, float y, float w, float h)
        : name(std::move(n)), rect({x, y}, {w, h}) {}
};

/*
 * Struct: ShopTrigger
 * Description: Represents a convenience store door trigger area.
 *
 * Fields:
 *   name - Trigger name (e.g., familymart_door_1, familymart_door_2).
 *   type - Shop type (e.g., convenience).
 *   rect - Interaction area (world coordinates).
 */
struct ShopTrigger {
    std::string name;   
    std::string type;      
    sf::FloatRect rect;   
};

/*
 * Struct: RespawnPoint
 * Description: Represents a respawn point on the map.
 *
 * Fields:
 *   name     - Point name (e.g., rebirth_point).
 *   position - Respawn position coordinates.
 *   maxCount - Maximum respawn count (read from count property).
 */
struct RespawnPoint {
    std::string name;     
    sf::Vector2f position; 
    int maxCount;         
    
    RespawnPoint() : maxCount(3) {} // Default 3 times
};
