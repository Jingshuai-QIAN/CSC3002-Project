/**
 * @file load_map.cpp
 * @brief SFML-based TMJ (Tiled Map JSON) Viewer with texture extrusion and smooth navigation
 * 
 * @mainpage TMJ Map Viewer
 * 
 * @section overview Overview
 * 
 * This application loads and displays TMJ (Tiled Map JSON) format maps using SFML for rendering.
 * It provides interactive navigation with keyboard and mouse controls, automatic texture extrusion
 * to prevent visual gaps, and spawn point detection for character positioning.
 * 
 * Key features:
 * - Loads TMJ maps with multiple layers and tilesets
 * - Automatic 1-pixel texture extrusion for gap-free rendering at any scale
 * - Smooth keyboard navigation with time-based movement
 * - Mouse drag panning and zoom controls
 * - Automatic protagonist spawn point detection
 * - View constraint to map boundaries
 * 
 * @section dependencies Dependencies
 * 
 * - SFML 3.0+ (Graphics, Window, System modules)
 * - nlohmann/json (JSON parsing library)
 * - C++17 standard library with filesystem support
 * 
 * @section usage Usage
 * 
 * Basic controls:
 * - Arrow Keys: Smooth camera movement
 * - Z/X: Zoom in/out
 * - Mouse Drag: Pan the view
 * - Escape: Exit application
 * 
 * The viewer automatically:
 * - Positions at protagonist spawn point if found in object layers
 * - Shows 75×75 tile area initially
 * - Applies texture extrusion to prevent rendering artifacts
 * - Constrains view to map boundaries
 * 
 * @section structures Data Structures
 * 
 * - TMJMap: Complete map representation with tiles, tilesets, and spawn points
 * - TilesetInfo: Tileset metadata with original and extruded texture parameters
 * 
 * @section notes Important Notes
 * 
 * 1. Texture Extrusion:
 *    - Adds 1-pixel borders around each tile to prevent gaps when scaling
 *    - Original textures are modified in-memory (disk files unchanged)
 *    - Falls back to original textures if extrusion fails
 * 
 * 2. Spawn Point Detection:
 *    - Searches object layers for objects with "protagonist" in name/type/class fields
 *    - Case-insensitive matching
 *    - Uses object center for rectangles, or offset for points
 * 
 * 3. Coordinate Systems:
 *    - World coordinates: Pixels from top-left corner of map
 *    - Tile coordinates: Grid positions (0,0) to (width-1, height-1)
 *    - View coordinates: Current visible area in world coordinates
 * 
 * 4. Performance:
 *    - All tiles are pre-rendered as sprites during loading
 *    - 60 FPS frame limit by default
 *    - Texture smoothing disabled for pixel-art clarity
 * 
 * @section file_organization File Organization
 * 
 * - Data Structures: TMJMap, TilesetInfo
 * - Auxiliary Functions: Texture extrusion, GID lookup, string utilities
 * - Core Loading: TMJ parsing and sprite generation
 * - View Management: Camera controls and boundary constraints
 * - Main Loop: Event handling and rendering pipeline
 * 
 * @section troubleshooting Troubleshooting
 * 
 * Common issues:
 * - File not found: Check relative path from executable
 * - Missing textures: Verify image paths in TMJ file
 * - Black screen: Check layer visibility and opacity settings
 * - Performance: Reduce map size or enable culling for large maps
 */


#include <SFML/Graphics.hpp>    // Graphics, window, and input handling
#include <nlohmann/json.hpp>    // JSON parsing library
#include <fstream>              // File stream operations
#include <iostream>             // Input/output stream
#include <filesystem>
#include <vector>               // Dynamic array container
#include <string>               // String class
#include <optional>             // Optional value wrapper
#include <cstdint>              // Fixed-size integer types
#include <cmath>                // Math functions
#include <cctype>               // Character classification
#include <algorithm>            // For std::clamp function


using json = nlohmann::json;    // Alias for JSON library namespace


// -------------------- Data Structures --------------------

/**
 * TilesetInfo represents a tileset from a TMJ (Tiled Map JSON) file.
 * 
 * This structure stores both the original tileset parameters and the modified
 * parameters after texture extrusion processing. Extrusion adds a 1-pixel border
 * around each tile to prevent visual gaps when rendering at different scales.
 */
struct TilesetInfo {
    int firstGid      = 1;  ///< First global ID in this tileset (used for tile referencing)
    int tileCount     = 0;  ///< Total number of tiles in the tileset
    int columns       = 0;  ///< Number of columns in the tileset image grid

    // Current pixel dimensions after extrusion processing
    int tileWidth     = 0;  ///< Atlas tile width after extrusion (original + 2*extrude)
    int tileHeight    = 0;  ///< Atlas tile height after extrusion (original + 2*extrude)
    int spacing       = 0;  ///< Spacing between tiles in atlas (set to 0 after extrusion)
    int margin        = 0;  ///< Margin around tileset in atlas (set to 0 after extrusion)

    // Original tileset parameters from TMJ file
    int origTileW     = 0;  ///< Original tile width before any processing
    int origTileH     = 0;  ///< Original tile height before any processing
    int origSpacing   = 0;  ///< Original spacing between tiles in source image
    int origMargin    = 0;  ///< Original margin around the source image

    std::string name;       ///< Name identifier for the tileset
    std::string imagePath;          ///< File path to the tileset image texture (relative to TMJ)
    std::string originalImagePath;  ///< Original image path from TMJ (for debugging)
    sf::Texture texture;    ///< SFML texture (extruded version or original if extrusion failed)
};


/**
 * TextObject represents a text object from TMJ object layers
 */
struct TextObject {
    std::string text;             ///< Text content

    float x = 0.f;                ///< X position in pixels
    float y = 0.f;                ///< Y position in pixels

    float width = 0.f;            ///< Bounding box width
    float height = 0.f;           ///< Bounding box height

    unsigned int fontSize = 12;   ///< Font size
    std::string fontFamily;       ///< Font family name
    sf::Color color = sf::Color::Black;  ///< Text color

    bool bold = true;             ///< Bold style
    bool italic = false;          ///< Italic style

    std::string halign = "left";  ///< Horizontal align: "left", "center", "right"
    std::string valign = "top";   ///< Vertical align: "top", "center", "bottom"
};


/**
 * EntranceArea represents a building entrance area from TMJ object layers
 */
struct EntranceArea {
    float x = 0.f;       ///< X position in pixels
    float y = 0.f;       ///< Y position in pixels
    float width = 0.f;   ///< Width in pixels
    float height = 0.f;  ///< Height in pixels
    std::string name;    ///< Optional name identifier
};


/**
 * TMJMap represents a complete map loaded from a TMJ (Tiled Map JSON) file.
 * 
 * This structure contains all the necessary data to render a tile-based map,
 * including map dimensions, tileset information, pre-rendered sprites, and
 * spawn point information for character placement.
 */
struct TMJMap {
    int mapWidthTiles  = 0;       ///< Map width measured in number of tiles
    int mapHeightTiles = 0;       ///< Map height measured in number of tiles
    int tileWidth      = 0;       ///< Single tile world grid width in pixels
    int tileHeight     = 0;       ///< Single tile world grid height in pixels

    std::vector<TilesetInfo> tilesets;       ///< Collection of tilesets used by this map
    std::vector<sf::Sprite>  tiles;          ///< All visible layer tiles converted to sprites in render order

    std::vector<TextObject>  textObjects;    ///< Text objects from object layers
    std::vector<EntranceArea> entranceAreas; ///< Entrance areas

    // Spawn point in pixel coordinates (origin at top-left corner)
    std::optional<float> spawnX;  ///< X-coordinate of player spawn point (pixels from left)
    std::optional<float> spawnY;  ///< Y-coordinate of player spawn point (pixels from top)

    /**
     * Calculates the total width of the map in pixels.
     * @return Map width in pixels
     */
    int worldPixelWidth()  const { return mapWidthTiles * tileWidth; }
    
    /**
     * Calculates the total height of the map in pixels.
     * @return Map height in pixels
     */
    int worldPixelHeight() const { return mapHeightTiles * tileHeight; }
};


// -------------------- Auxiliary Functions --------------------

/**
 * Finds the appropriate tileset for a given global tile ID (GID).
 * 
 * This function searches through all loaded tilesets to find which one contains
 * the specified GID. It implements the "most specific" matching by selecting
 * the tileset with the highest firstGid that still contains the target GID.
 * 
 * @param v Vector of tilesets to search through
 * @param gid Global ID of the tile to find
 * @return Pointer to the matching tileset, or nullptr if no tileset contains the GID
 */
static TilesetInfo* findTilesetForGid(std::vector<TilesetInfo>& v, int gid) {
    TilesetInfo* res = nullptr;  ///< Result pointer
    for (auto& ts : v) {
        // Check if GID is in this tileset's range
        int rangeStart = ts.firstGid;
        int rangeEnd = ts.firstGid + ts.tileCount;

        if (gid >= rangeStart && gid < rangeEnd) {
            // Select tileset with highest firstGid
            if (!res || ts.firstGid > res->firstGid) {
                res = &ts;
            }
        }
    }
    return res;  ///< Return result
}


/**
 * Converts a string to lowercase using locale-independent character conversion.
 * 
 * This function handles the safe conversion of characters to lowercase by first
 * casting to unsigned char to avoid undefined behavior with negative char values
 * that some std::tolower implementations may exhibit.
 * 
 * @param s Input string to convert
 * @return New string with all characters converted to lowercase
 */
static std::string toLower(std::string s) {
    for (char& c : s) {
        // Convert char to unsigned char for safe tolower operation,
        // then cast back to char for storage
        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))
        );
    }
    return s;
}


/**
 * Determines if a JSON object represents the protagonist character.
 * 
 * This function checks multiple potential identifier fields (name, type, class)
 * in a case-insensitive manner to identify protagonist objects. The search is
 * strict and requires the exact substring "protagonist" to be present.
 * 
 * @param obj JSON object representing a map object from TMJ file
 * @return true if the object contains "protagonist" in any identifier field,
 *         false otherwise
 */
static bool nameIsProtagonist(const nlohmann::json& obj) {
    // Lambda to check if JSON object has a string field with given key
    auto has = [&](const char* k) { 
        return obj.contains(k) && obj[k].is_string(); 
    };
    
    std::string key;  ///< Combined lowercase string of all identifier fields
    
    // Concatenate all available identifier fields with spaces
    if (has("name")) {
        key += " " + toLower(obj["name"].get<std::string>());
    }
    if (has("type")) {
        key += " " + toLower(obj["type"].get<std::string>());
    }
    if (has("class")) {
        key += " " + toLower(obj["class"].get<std::string>());
    }
    
    // Check if combined string contains "protagonist" substring
    return key.find("protagonist") != std::string::npos;
}


/**
 * Creates an extruded texture from source image to prevent visual gaps when scaling.
 * 
 * Extrusion adds a 1-pixel border around each tile by copying edge pixels,
 * which eliminates rendering artifacts during zooming and scaling operations.
 * The output texture has no margin or spacing, with each tile enlarged by 2*extrude.
 * 
 * @param src Source image containing the tileset
 * @param srcTileW Original source tile width in pixels
 * @param srcTileH Original source tile height in pixels
 * @param columns Number of tile columns in the source image
 * @param spacing Original spacing between tiles in source
 * @param margin Original margin around tileset in source
 * @param extrude Number of pixels to extrude (typically 1)
 * @param outTex Output texture with extruded tiles
 * @return true if extrusion successful, false otherwise
 */
static bool makeExtrudedTextureFromImage(
    const sf::Image& src,
    int srcTileW, 
    int srcTileH,
    int columns, 
    int spacing, 
    int margin,
    int extrude,
    sf::Texture& outTex
){
    // Validate input parameters
    if (srcTileW <= 0 || srcTileH <= 0 || columns <= 0) return false;

    const sf::Vector2u isz = src.getSize();     ///< Source image dimensions
    const int usableW = static_cast<int>(isz.x) - margin*2 + spacing;
    const int usableH = static_cast<int>(isz.y) - margin*2 + spacing;

    const int cols = columns;
    if (cols <= 0) return false;

    // Calculate number of rows based on available height
    const int rows = (usableH + spacing) / (srcTileH + spacing);
    if (rows <= 0) return false;

    // Calculate output dimensions for extruded texture
    const int tileOutW = srcTileW + 2*extrude;  ///< Output tile width
    const int tileOutH = srcTileH + 2*extrude;  ///< Output tile height
    const int dstW = cols * tileOutW;           ///< Total output width
    const int dstH = rows * tileOutH;           ///< Total output height
    if (dstW <= 0 || dstH <= 0) return false;

    // Create destination image with transparent background
    sf::Image dst(sf::Vector2u(static_cast<unsigned>(dstW), 
                               static_cast<unsigned>(dstH)),
                  sf::Color::Transparent);

    // Lambda for safe pixel retrieval with bounds checking
    auto getPix = [&](int x, int y)->sf::Color{
        if (x < 0 || y < 0) return sf::Color::Transparent;
        if (x >= static_cast<int>(isz.x) || y >= static_cast<int>(isz.y)) {
            return sf::Color::Transparent;
        }
        return src.getPixel(sf::Vector2u(static_cast<unsigned>(x), 
                                         static_cast<unsigned>(y)));
    };

    // Process each tile in the tileset
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const int sx = margin + c * (srcTileW + spacing);  ///< Source X
            const int sy = margin + r * (srcTileH + spacing);  ///< Source Y
            const int dx = c * tileOutW;                       ///< Dest X
            const int dy = r * tileOutH;                       ///< Dest Y

            // Copy main tile area to center of extruded region
            for (int yy = 0; yy < srcTileH; ++yy) {
                for (int xx = 0; xx < srcTileW; ++xx) {
                    dst.setPixel(
                        sf::Vector2u(static_cast<unsigned>(dx + extrude + xx),
                                     static_cast<unsigned>(dy + extrude + yy)),
                        getPix(sx + xx, sy + yy)
                    );
                }
            }

            // Extrude left and right edges
            for (int yy = 0; yy < srcTileH; ++yy) {
                const sf::Color L = getPix(sx, sy + yy);           ///< Left edge
                const sf::Color R = getPix(sx + srcTileW - 1, sy + yy); ///< Right edge
                
                for (int e = 0; e < extrude; ++e) {
                    // Left extrusion
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + e),
                            static_cast<unsigned>(dy + extrude + yy)
                        ), 
                        L
                    );
                    // Right extrusion
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + extrude + srcTileW + e),
                            static_cast<unsigned>(dy + extrude + yy)
                        ), 
                        R
                    );
                }
            }

            // Extrude top and bottom edges
            for (int xx = 0; xx < srcTileW; ++xx) {
                const sf::Color T = getPix(sx + xx, sy);  ///< Top edge
                const sf::Color B = getPix(
                    sx + xx, 
                    sy + srcTileH - 1
                ); ///< Bottom edge
                
                for (int e = 0; e < extrude; ++e) {
                    // Top extrusion
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + extrude + xx),
                            static_cast<unsigned>(dy + e)
                        ), 
                        T
                    );
                    // Bottom extrusion
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + extrude + xx),
                            static_cast<unsigned>(dy + extrude + srcTileH + e)
                        ),
                        B
                    );
                }
            }

            // Extrude four corners
            const sf::Color TL = getPix(sx, sy);                 ///< Top-left
            const sf::Color TR = getPix(sx + srcTileW - 1, sy);  ///< Top-right
            const sf::Color BL = getPix(sx, sy + srcTileH - 1);  ///< Bottom-left
            const sf::Color BR = getPix(
                sx + srcTileW - 1, 
                sy + srcTileH - 1
            ); ///< Bottom-right
            
            for (int ey = 0; ey < extrude; ++ey) {
                for (int ex = 0; ex < extrude; ++ex) {
                    // Top-left corner
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + ex),
                            static_cast<unsigned>(dy + ey)
                        ), 
                        TL
                    );
                    // Top-right corner
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + ex + extrude + srcTileW),
                            static_cast<unsigned>(dy + ey)
                        ), 
                        TR
                    );
                    // Bottom-left corner
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + ex),
                            static_cast<unsigned>(dy + ey + extrude + srcTileH)
                        ), 
                        BL
                    );
                    // Bottom-right corner
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + ex + extrude + srcTileW),
                            static_cast<unsigned>(dy + ey + extrude + srcTileH)
                        ), 
                        BR
                    );
                }
            }
        }
    }

    // Load texture from extruded image and disable smoothing
    if (!outTex.loadFromImage(dst)) return false;
    outTex.setSmooth(false);
    return true;
}


// -------------------- TMJ Loading and Rendering --------------------
/**
 * Loads and processes a TMJ (Tiled Map JSON) file into a TMJMap structure.
 * 
 * This function handles the complete loading pipeline including:
 * - Parsing map dimensions and properties
 * - Loading and extruding tileset textures
 * - Processing visible tile layers into sprites
 * - Locating protagonist spawn points
 * 
 * @param path File path to the TMJ file
 * @param map Output TMJMap structure to populate
 * @param extrude Number of pixels for texture extrusion (default: 1)
 * @return true if loading successful, false otherwise
 */
static bool loadTMJ(const std::string& path, TMJMap& map, int extrude = 1) {
    // Open and validate TMJ file
    std::ifstream in(path);
    if (!in) { 
        std::cerr << "open tmj failed: " << path << "\n"; 
        return false; 
    }

    // Get the directory of the TMJ file
    std::filesystem::path tmjPath(path);
    std::filesystem::path tmjDir = tmjPath.parent_path();
    std::cout << "[loader] TMJ directory: " << tmjDir.string() << std::endl;

    json j;
    try { 
        in >> j; 
    } catch (...) { 
        std::cerr << "json parse failed\n"; 
        return false; 
    }

    // Lambda helper for safe integer extraction from JSON
    auto getInt = [&](const json& o, const char* k)->std::optional<int>{
        if (!o.contains(k) || !o[k].is_number_integer()) return std::nullopt;
        return o[k].get<int>();
    };

    // Extract and validate basic map dimensions
    auto w  = getInt(j,"width");
    auto h  = getInt(j,"height");
    auto tw = getInt(j,"tilewidth");
    auto th = getInt(j,"tileheight");

    if (!w || !h || !tw || !th) { 
        std::cerr << "missing map dims\n"; 
        return false; 
    }

    map.mapWidthTiles  = *w;
    map.mapHeightTiles = *h;
    map.tileWidth      = *tw;
    map.tileHeight     = *th;

    // Process tilesets array
    if (!j.contains("tilesets") || !j["tilesets"].is_array()) { 
        std::cerr << "no tilesets\n"; 
        return false; 
    }
    map.tilesets.clear();

    for (const auto& tsj : j["tilesets"]) {
        TilesetInfo ts;
        
        // Extract basic tileset properties
        auto fg = getInt(tsj,"firstgid");
        if (!fg) { 
            std::cerr << "tileset no firstgid\n"; 
            return false; 
        }

        ts.firstGid  = *fg;
        ts.name      = tsj.value("name", std::string("tileset"));
        ts.originalImagePath = (tsj.contains("image") && tsj["image"].is_string()) 
                    ? tsj["image"].get<std::string>() : "";
        
        // Construct the full path of the image
        if (!ts.originalImagePath.empty()) {
            std::filesystem::path imageRelativePath(ts.originalImagePath);
            std::filesystem::path fullImagePath = tmjDir / imageRelativePath;
            ts.imagePath = fullImagePath.string();
            
            std::cout << "[loader] Image path - Original: " << ts.originalImagePath 
                      << ", Full: " << ts.imagePath << std::endl;
        } else {
            ts.imagePath = "";
        }

        // Store original tileset parameters
        ts.origTileW   = tsj.value("tilewidth",  map.tileWidth);
        ts.origTileH   = tsj.value("tileheight", map.tileHeight);
        ts.origSpacing = tsj.value("spacing", 0);
        ts.origMargin  = tsj.value("margin",  0);
        ts.columns     = tsj.value("columns", 0);
        ts.tileCount   = tsj.value("tilecount", 0);

        // Skip tilesets without embedded images (external .tsx files)
        if (ts.imagePath.empty()) {
            std::cerr << "[loader] tileset '" << ts.name 
                      << "' has no embedded image (external .tsx?)\n";
            map.tilesets.push_back(std::move(ts));
            continue;
        }

        // Load original tileset texture
        sf::Texture original;

        if (!original.loadFromFile(ts.imagePath)) {
            std::cerr << "[loader] load image failed: " << ts.imagePath << "\n";
            map.tilesets.push_back(std::move(ts));
            continue;
        } else {
            std::cout << "[loader] Loaded texture from: " << ts.imagePath << "\n";
        }

        // Calculate columns and tile count if not provided
        if (ts.columns == 0) {
            auto s = original.getSize();
            ts.columns = static_cast<int>(s.x) / ts.origTileW;
        }
        if (ts.tileCount == 0) {
            auto s = original.getSize();
            int rows = static_cast<int>(s.y) / ts.origTileH;
            ts.tileCount = ts.columns * rows;
        }

        // Create extruded texture atlas
        sf::Image src = original.copyToImage();
        sf::Texture extr;
        if (makeExtrudedTextureFromImage(
                src, 
                ts.origTileW, 
                ts.origTileH,
                ts.columns, 
                ts.origSpacing, 
                ts.origMargin,
                extrude, 
                extr
            )
        ) {
            // Use extruded texture with updated parameters
            ts.texture    = std::move(extr);
            ts.tileWidth  = ts.origTileW + 2*extrude;
            ts.tileHeight = ts.origTileH + 2*extrude;
            ts.spacing    = 0;
            ts.margin     = 0;
        } else {
            // Fall back to original texture
            ts.texture    = std::move(original);
            ts.tileWidth  = ts.origTileW;
            ts.tileHeight = ts.origTileH;
            ts.spacing    = ts.origSpacing;
            ts.margin     = ts.origMargin;
        }

        map.tilesets.push_back(std::move(ts));
    }

    // Process layers array for tile data
    if (!j.contains("layers") || !j["layers"].is_array()) {
        std::cerr << "JSON error: no 'layers' array in root\n";
        return false;
    }

    map.tiles.clear();
    int layerIdx = 0;

    for (const auto& L : j["layers"]) {
        ++layerIdx;

        // Skip non-tile layers
        if (!L.contains("type") || !L["type"].is_string() || L["type"] != "tilelayer")
            continue;

        // Check layer visibility
        bool visible = true;
        if (L.contains("visible") && L["visible"].is_boolean())
            visible = L["visible"].get<bool>();
        if (!visible) continue;

        // Extract layer metadata
        std::string lname = L.contains("name") && L["name"].is_string()
                          ? L["name"].get<std::string>()
                          : ("layer_" + std::to_string(layerIdx));

        // Get layer dimensions (default to map dimensions)
        int lw = L.contains("width")  ? L["width"].get<int>()  : map.mapWidthTiles;
        int lh = L.contains("height") ? L["height"].get<int>() : map.mapHeightTiles;

        // Extract layer rendering properties
        float offx    = (L.contains("offsetx") && L["offsetx"].is_number()) 
                        ? L["offsetx"].get<float>() : 0.f;
        float offy    = (L.contains("offsety") && L["offsety"].is_number()) 
                        ? L["offsety"].get<float>() : 0.f;
        float opacity = (L.contains("opacity") && L["opacity"].is_number()) 
                        ? L["opacity"].get<float>() : 1.f;
        
        if (opacity < 0.f) opacity = 0.f;
        if (opacity > 1.f) opacity = 1.f;

        // Validate and extract tile data array
        if (!L.contains("data") || !L["data"].is_array()) {
            std::cerr << "JSON error: tilelayer '" << lname 
                      << "' data missing / not array\n";
            continue;
        }
        
        std::vector<int> data;
        try { 
            data = L["data"].get<std::vector<int>>(); 
        } catch (const std::exception& e) {
            std::cerr << "JSON error: tilelayer '" << lname 
                      << "' data to int[] failed: " << e.what() << "\n";
            continue;
        }
        
        if (static_cast<int>(data.size()) != lw*lh) {
            std::cerr << "JSON error: layer '" << lname << "' data size mismatch: "
                      << data.size() << " vs " << (lw*lh) << "\n";
            continue;
        }

        // Process each tile in the layer
        int painted = 0;
        for (int y = 0; y < lh; ++y) {
            for (int x = 0; x < lw; ++x) {
                int gid = data[x + y * lw];
                if (gid == 0) continue;  // Skip empty tiles

                // Find appropriate tileset for this GID
                TilesetInfo* ts = findTilesetForGid(map.tilesets, gid);
                if (!ts) continue;
                if (ts->texture.getSize().x == 0) continue;

                // Calculate local tile ID and texture coordinates
                int localId = gid - ts->firstGid;
                if (localId < 0 || localId >= ts->tileCount) continue;

                const int tu = localId % ts->columns;  ///< Texture X coordinate
                const int tv = localId / ts->columns;  ///< Texture Y coordinate

                // Calculate source rectangle in tileset texture
                const int sx = ts->margin + tu * (ts->tileWidth  + ts->spacing);
                const int sy = ts->margin + tv * (ts->tileHeight + ts->spacing);

                sf::IntRect rect(sf::Vector2i(sx, sy),
                                 sf::Vector2i(ts->tileWidth, ts->tileHeight));

                // Create and configure sprite
                sf::Sprite spr(ts->texture, rect);
                spr.setPosition(sf::Vector2f(
                    offx + static_cast<float>(x * map.tileWidth),
                    offy + static_cast<float>(y * map.tileHeight)
                ));
                
                // Apply layer opacity
                if (opacity < 1.f) {
                    spr.setColor(sf::Color(
                        255, 255, 255,
                        static_cast<std::uint8_t>(std::lround(255.f * opacity))
                    ));
                }

                map.tiles.push_back(spr);
                ++painted;
            }
        }

        std::cerr << "[layer] '" << lname << "' painted=" << painted
                  << " offset=(" << offx << "," << offy << ") opacity=" << opacity << "\n";
    }

    // Process text objects
    map.textObjects.clear();

    // Search text objects in object layers
    for (const auto& L : j["layers"]) {
        if (!L.contains("type") || !L["type"].is_string() || L["type"] != "objectgroup") 
            continue;
            
        std::string layerName = L.contains("name") && L["name"].is_string()
                              ? L["name"].get<std::string>() : "objectgroup";

        // Display text objects across all object layers
        // or specify a particular layer (such as building_names).
        bool shouldDisplayText = (layerName == "building_names") || 
                               (layerName.find("text") != std::string::npos) ||
                               (layerName.find("name") != std::string::npos);

        if (L.contains("objects") && L["objects"].is_array() && shouldDisplayText) {
            for (const auto& obj : L["objects"]) {
                if (!obj.is_object()) continue;

                TextObject textObj;
                
                textObj.x = obj.value("x", 0.f);
                textObj.y = obj.value("y", 0.f);
                textObj.width = obj.value("width", 0.f);
                textObj.height = obj.value("height", 0.f);

                if (obj.contains("text") && obj["text"].is_object()) {
                    const auto& textData = obj["text"];

                    // Basic properties
                    textObj.text = textData.value("text", "");
                    textObj.fontSize = textData.value("pixelsize", 16u);
                    textObj.bold = textData.value("bold", false);
                    textObj.italic = textData.value("italic", false);
                    
                    // Align type
                    if (textData.contains("halign") && textData["halign"].is_string()) {
                        textObj.halign = textData["halign"].get<std::string>();
                    }
                    if (textData.contains("valign") && textData["valign"].is_string()) {
                        textObj.valign = textData["valign"].get<std::string>();
                    }

                    // Process colors
                    if (textData.contains("color") && textData["color"].is_string()) {
                        std::string colorStr = textData["color"].get<std::string>();
                        if (colorStr.length() == 7 && colorStr[0] == '#') {
                            try {
                                int r = std::stoi(colorStr.substr(1, 2), nullptr, 16);
                                int g = std::stoi(colorStr.substr(3, 2), nullptr, 16);
                                int b = std::stoi(colorStr.substr(5, 2), nullptr, 16);
                                textObj.color = sf::Color(r, g, b);
                            } catch (...) {
                                textObj.color = sf::Color::White;
                            }
                        }
                    }
                }

                // Compatible with older formats: Text is directly used as a string attribute
                else if (obj.contains("text") && obj["text"].is_string()) {
                    textObj.text = obj["text"].get<std::string>();
                    textObj.fontSize = 16; // 默认大小
                    textObj.color = sf::Color::White;
                }
                
                // If text contents exit, record them.
                if (!textObj.text.empty()) {
                    map.textObjects.push_back(textObj);
                    std::cerr << "[text] Loaded text: '" << textObj.text 
                              << "' at (" << textObj.x << "," << textObj.y << ")\n";
                }
            }
        }
    }

    // Process entrance areas from object layers
    map.entranceAreas.clear();

    for (const auto& L : j["layers"]) {
        if (!L.contains("type") || !L["type"].is_string() || L["type"] != "objectgroup") 
            continue;
            
        std::string layerName = L.contains("name") && L["name"].is_string()
                            ? L["name"].get<std::string>() : "objectgroup";
        
        // Here, we only process the object layer "entrance"
        if (layerName != "entrance") continue;

        if (L.contains("objects") && L["objects"].is_array()) {
                for (const auto& obj : L["objects"]) {
                    if (!obj.is_object()) continue;

                    EntranceArea area;
                    
                    area.x = obj.value("x", 0.f);
                    area.y = obj.value("y", 0.f);
                    area.width = obj.value("width", 0.f);
                    area.height = obj.value("height", 0.f);
                    
                    // Record the read areas
                    if (obj.contains("name") && obj["name"].is_string()) {
                        area.name = obj["name"].get<std::string>();
                    }

                    map.entranceAreas.push_back(area);
                    std::cerr << "[entrance] Loaded area: " << area.name 
                            << " at (" << area.x << "," << area.y << ")"
                            << " size=" << area.width << "x" << area.height << "\n";
                }
            }
        }

    // Search object layers for protagonist spawn point
    if (j.contains("layers") && j["layers"].is_array()) {
        for (const auto& L : j["layers"]) {
            if (!L.contains("type") || !L["type"].is_string() || L["type"] != "objectgroup") 
                continue;
            if (!L.contains("objects") || !L["objects"].is_array()) 
                continue;

            for (const auto& obj : L["objects"]) {
                if (!obj.is_object()) continue;
                if (!nameIsProtagonist(obj)) continue;

                // Extract and calculate spawn position
                if (obj.contains("x") && obj.contains("y") && 
                    obj["x"].is_number() && obj["y"].is_number()) {
                    
                    float ox = obj["x"].get<float>();
                    float oy = obj["y"].get<float>();
                    float ow = (obj.contains("width") && obj["width"].is_number()) 
                             ? obj["width"].get<float>() : 0.f;
                    float oh = (obj.contains("height") && obj["height"].is_number()) 
                             ? obj["height"].get<float>() : 0.f;

                    // Calculate center point for rectangles, or offset for points
                    if (ow > 0.f || oh > 0.f) {
                        ox += ow * 0.5f;
                        oy += oh * 0.5f;
                    } else {
                        ox += map.tileWidth  * 0.5f;
                        oy += map.tileHeight * 0.5f;
                    }

                    map.spawnX = ox;
                    map.spawnY = oy;
                    std::cerr << "[spawn] protagonist at (" << ox << "," << oy 
                              << ") rect=" << ow << "x" << oh << "\n";
                    goto SPAWN_FOUND;  // Exit search after finding first protagonist
                }
            }
        }
    }
SPAWN_FOUND:

    // Output loading statistics
    std::cerr << "[debug] total sprites=" << map.tiles.size()
              << " mapTiles=" << map.mapWidthTiles << "x" << map.mapHeightTiles
              << " tileSize=" << map.tileWidth << "x" << map.tileHeight << "\n";

    return true;
}


/**
 * Constrains the view's center to stay within map boundaries.
 * 
 * This function ensures the camera view doesn't display areas outside the map.
 * If the view is larger than the map in either dimension, it centers the view
 * in that dimension. Otherwise, it clamps the view to prevent showing black borders.
 * 
 * @param view SFML view to constrain
 * @param mapW Total map width in pixels
 * @param mapH Total map height in pixels
 */
static void clampViewToMap(sf::View& view, int mapW, int mapH) {
    sf::Vector2f size   = view.getSize();  ///< View size in world coordinates (pixels)
    sf::Vector2f half   = { size.x * 0.5f, size.y * 0.5f };
    sf::Vector2f center = view.getCenter();

    // Handle horizontal clamping
    if (size.x >= mapW) {
        // View wider than map: center horizontally
        center.x = std::max(0.f, float(mapW) * 0.5f);
    } else {
        // View narrower than map: clamp to map edges
        float minX = half.x;
        float maxX = float(mapW) - half.x;
        center.x = std::clamp(center.x, minX, maxX);
    }

    // Handle vertical clamping
    if (size.y >= mapH) {
        // View taller than map: center vertically
        center.y = std::max(0.f, float(mapH) * 0.5f);
    } else {
        // View shorter than map: clamp to map edges
        float minY = half.y;
        float maxY = float(mapH) - half.y;
        center.y = std::clamp(center.y, minY, maxY);
    }

    view.setCenter(center);
}


// -------------------- Main Function --------------------
/**
 * Main application entry point for TMJ Map Viewer.
 * 
 * Handles program initialization, map loading, window creation, and the main game loop.
 * Provides interactive map navigation with keyboard and mouse controls.
 * 
 * @return Application exit code (0 = success, 1 = error)
 */
int main() {
    std::cout << "=== Program START ===" << std::endl;

    // Try to get the current directory 
    try {
        std::cout << "Current working directory: " << 
            std::filesystem::current_path().string() << std::endl;
    } catch (...) {
        std::cout << "Cannot get current working directory" << std::endl;
    }

    // Determine TMJ file path from command line or use default
    const std::string tmj = "Map/map_codes/lower_campus_map/lower_campus_map.tmj";
    
    std::cout << "Looking for file: " << tmj << std::endl;
    
    if (std::filesystem::exists(tmj)) {
        std::cout << "File exists! Proceeding to load..." << std::endl;
    } else {
        std::cout << "File does NOT exist! Check the path." << std::endl;
        system("pause");
        return 1;
    }
    std::cout.flush();
    
    // Check whether the file exists
    std::ifstream test(tmj);
    if (!test) {
        std::cerr << "File does not exist or cannot be opened: " << tmj << std::endl;
        system("pause");
        return 1;
    }
    test.close();

    // Initialize a map instance
    TMJMap map;

    // Load and process the TMJ map file with 1-pixel extrusion
    if (!loadTMJ(tmj, map, /*extrude*/1)) {
        std::cerr << "loadTMJ failed!" << std::endl;
        system("pause");
        return 1;
    }

    // Calculate map and window dimensions
    const int mapW = map.worldPixelWidth();
    const int mapH = map.worldPixelHeight();
    const int winW = std::min(mapW, 1280);    ///< Window width (cap at 1280)
    const int winH = std::min(mapH, 720);     ///< Window height (cap at 720)

    // Create application window
    sf::VideoMode vm{ sf::Vector2u(static_cast<unsigned>(winW), 
                                   static_cast<unsigned>(winH)) };
    sf::RenderWindow window(vm, sf::String("TMJ Viewer"), sf::State::Windowed);
    window.setFramerateLimit(60);             ///< Limit to 60 FPS

    // Configure initial view to show 75×75 tiles area
    const float tilesW = 75.f;                     ///< View width in tiles
    const float tilesH = 75.f;                     ///< View height in tiles
    const float viewW  = tilesW * map.tileWidth;   ///< View width in pixels
    const float viewH  = tilesH * map.tileHeight;  ///< View height in pixels

    sf::View view(sf::FloatRect(sf::Vector2f(0.f, 0.f), 
                                sf::Vector2f(viewW, viewH)));
    
    // Position view at spawn point or map center
    if (map.spawnX && map.spawnY) {
        view.setCenter(sf::Vector2f(*map.spawnX, *map.spawnY));
    } else {
        view.setCenter(sf::Vector2f(static_cast<float>(mapW) * 0.5f, 
                                    static_cast<float>(mapH) * 0.5f));
    }

    // Mouse drag state variables
    bool dragging = false;                    ///< Currently dragging view
    sf::Vector2i prevMouse{0,0};              ///< Previous mouse position

    // Frame timing for smooth movement
    sf::Clock clock;

    // Main application loop
    while (window.isOpen()) {
        // Process all pending events
        while (auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) window.close();

            // Optional: Uncomment for mouse wheel zoom support
            // if (ev->is<sf::Event::MouseWheelScrolled>()) {
            //     auto mw = ev->get<sf::Event::MouseWheelScrollEvent>();
            //     if (mw.delta > 0) view.zoom(1.f / 1.1f);  // Scroll up: zoom in
            //     else              view.zoom(1.1f);        // Scroll down: zoom out
            // }
        }

        // Keyboard controls
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) window.close();
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z)) view.zoom(1.f / 1.05f);  // Zoom in
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::X)) view.zoom(1.05f);        // Zoom out

        // Arrow key movement with time-based smoothing
        float dt = clock.restart().asSeconds();   ///< Delta time since last frame
        const float speed = 150.f;                ///< Movement speed in pixels/second
        sf::Vector2f move(0.f, 0.f);              ///< Movement vector
        
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  move.x -= speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) move.x += speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))    move.y -= speed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))  move.y += speed * dt;
        
        view.move(move);

        // Mouse drag panning
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            const auto cur = sf::Mouse::getPosition(window);
            if (!dragging) { 
                dragging = true; 
                prevMouse = cur; 
            } else {
                const sf::Vector2i d = cur - prevMouse;
                view.move(sf::Vector2f(-static_cast<float>(d.x), 
                                       -static_cast<float>(d.y)));
                prevMouse = cur;
            }
        } else {
            dragging = false;
        }

        // Prevent excessive zoom-out (view larger than map)
        {
            sf::Vector2f sz = view.getSize();
            bool adjust = false;
            
            if (sz.x > static_cast<float>(mapW)) { 
                sz.x = static_cast<float>(mapW); 
                adjust = true; 
            }
            if (sz.y > static_cast<float>(mapH)) { 
                sz.y = static_cast<float>(mapH); 
                adjust = true; 
            }
            
            if (adjust) view.setSize(sz);
        }

        // Ensure view stays within map boundaries
        clampViewToMap(view, mapW, mapH);

        // Rendering
        window.clear(sf::Color::Black);    ///< Clear to black background
        window.setView(view);              ///< Apply current view

        for (const auto& s : map.tiles) {  ///< Draw all map tiles
            window.draw(s);
        }

        // Draw text objects
        for (const auto& textObj : map.textObjects) {
            const sf::Font font("font/calibri.ttf");
            
            sf::Text text(font, textObj.text, textObj.fontSize);

            text.setFillColor(textObj.color);
            
            if (textObj.bold) text.setStyle(sf::Text::Bold);
            if (textObj.italic) text.setStyle(sf::Text::Italic);

            // Compute the boundaries for aligning
            sf::FloatRect textBounds = text.getLocalBounds();
            float textWidth = textBounds.size.x;
            float textHeight = textBounds.size.y;

            // Compute the position considering aligning method
            float finalX = textObj.x;
            float finalY = textObj.y;

            // Horizontal align
            if (textObj.halign == "center") {
                finalX += textObj.width * 0.5f;
                text.setOrigin(sf::Vector2f{textWidth * 0.5f, 0});
            } else if (textObj.halign == "right") {
                finalX += textObj.width;
                text.setOrigin(sf::Vector2f{textWidth, 0});
            } else {
                text.setOrigin(sf::Vector2f{0, 0});
            }

            // Vertical align
            if (textObj.valign == "center") {
                finalY += textObj.height * 0.5f;
                text.setOrigin(sf::Vector2f{text.getOrigin().x, textHeight * 0.5f});
            } else if (textObj.valign == "bottom") {
                finalY += textObj.height;
                text.setOrigin(sf::Vector2f{text.getOrigin().x, textHeight});
            } else {
                text.setOrigin(sf::Vector2f{text.getOrigin().x, 0});
            }
            
            text.setPosition(sf::Vector2f(finalX, finalY));
            
            window.draw(text);
        }

        // Draw entrance areas
        for (const auto& area : map.entranceAreas) {
            sf::RectangleShape rect(sf::Vector2f(area.width, area.height));
            rect.setPosition(sf::Vector2f(area.x, area.y));
            
            rect.setFillColor(sf::Color(0, 100, 255, 100)); // RGBA: Translucent blue
            rect.setOutlineThickness(0); // No outline
            
            window.draw(rect);
        }

        // Update window
        window.display();
    }

    return 0;
}
