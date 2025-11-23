// MapObjects.h
#pragma once

// Standard includes used by the object definitions.
#include <string>
#include <vector>

// SFML types for colors and geometry.
#include <SFML/Graphics.hpp>

/**
 * @file MapObjects.h
 * @brief Small POD types representing objects parsed from Tiled TMJ maps.
 *
 * This header declares lightweight structures used to expose object layers
 * (text labels, entrance rectangles and NotWalkable polygons) to rendering
 * and gameplay code.
 *
 * Structures:
 * - TextObject: stores a single text label and alignment data.
 * - EntranceArea: rectangular area representing a map entrance.
 * - BlockPoly: polygon used for non-walkable region checks.
 *
 * Dependencies:
 * - SFML for color and vector types.
 *
 * Notes:
 * - All structures are POD-like to keep JSON parsing and copying simple.
 */

/**
 * @struct TextObject
 * @brief Represents a text label placed on the map by Tiled.
 *
 * Fields:
 * - x,y: top-left position in pixels.
 * - width,height: optional bounding rectangle for alignment.
 * - text/fontSize/bold/italic/color: rendering parameters.
 * - halign/valign: alignment strings ("left"/"center"/"right", "top"/"center"/"bottom").
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

/**
 * @struct EntranceArea
 * @brief Rectangle representing an entrance/trigger region placed in Tiled.
 *
 * The rectangle is expressed in pixel coordinates using x/y as top-left.
 */
struct EntranceArea {
    float x = 0.f, y = 0.f;
    float width = 0.f, height = 0.f;
    std::string name;
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
    sf::FloatRect bounds;             // AABB as position + size vectors (SFML 3)
};