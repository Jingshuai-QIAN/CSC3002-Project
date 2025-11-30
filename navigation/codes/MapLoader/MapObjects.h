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

struct InteractionObject {
    std::string type;       // 交互类型（如"counter"）
    std::string name;       // 对象名称
    sf::FloatRect rect;     // 交互区域
    std::vector<std::string> options; // 交互选项（菜品列表）
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

struct GameTriggerArea {
    float x = 0.f, y = 0.f;       // 区域左上角坐标
    float width = 0.f, height = 0.f; // 区域大小
    std::string name;             // 区域名称（如"bookstore_game"）
    std::string gameType;         // 小游戏类型（用于区分不同游戏）
    std::string questionSet;      // 可选：题库 id（e.g. "classroom_basic"）
};

// 添加Chef对象结构
struct Chef {
    std::string name;       // 对象名称（可用于区分不同厨师）
    sf::FloatRect rect;     // 矩形对象的位置和大小（从Tiled导出）
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

    // ✅ 1️⃣ 默认构造时强制初始化 bounds，防止野值
    BlockPoly()
        : points(),
          bounds(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f)) {}

    // ✅ 2️⃣ 便捷构造（常用于解析 TMJ）
    explicit BlockPoly(std::vector<sf::Vector2f> p)
        : points(std::move(p)),
          bounds(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f)) {}

    // ✅ 3️⃣ 显式允许安全拷贝
    BlockPoly(const BlockPoly&) = default;
    BlockPoly& operator=(const BlockPoly&) = default;

    // ✅ 4️⃣ 显式允许安全移动（关键！防止 vector 扩容时崩溃）
    BlockPoly(BlockPoly&&) noexcept = default;
    BlockPoly& operator=(BlockPoly&&) noexcept = default;
};
