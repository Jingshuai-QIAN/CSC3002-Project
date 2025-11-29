// TMJMap.cpp
#include "TMJMap.h"
#include "Utils/Logger.h"
#include "Utils/StringUtils.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>

// Alias for json library.
using json = nlohmann::json;

/*
 * File: TMJMap.cpp
 * Description: TMJ map parsing and texture management implementation.
 *
 * This file implements:
 *   - Loading TMJ JSON maps.
 *   - Building extruded textures for tilesets.
 *   - Parsing tile layers and object layers including NotWalkable polygons.
 *
 * Important functions/classes:
 *   - TMJMap::loadFromFile: main entry point to load a TMJ map.
 *   - TMJMap::parseObjectLayers: parse object layers for spawn/text/entrance/notwalkable.
 *
 * Notes:
 *   - Uses std::filesystem to resolve relative tileset image paths.
 *   - Textures are stored inside TilesetInfo.texture and must remain alive while sprites reference them.
 */


std::vector<std::string> splitDishesString(const std::string& str) {
    std::vector<std::string> dishes;
    std::string currentDish;
    
    for (char c : str) {
        if (c == ',') {
            // 清理空格并保存当前菜品
            size_t start = currentDish.find_first_not_of(" ");
            size_t end = currentDish.find_last_not_of(" ");
            if (start != std::string::npos && end != std::string::npos) {
                dishes.push_back(currentDish.substr(start, end - start + 1));
            }
            currentDish.clear();
        } else {
            currentDish += c;
        }
    }
    
    // 处理最后一个菜品
    size_t start = currentDish.find_first_not_of(" ");
    size_t end = currentDish.find_last_not_of(" ");
    if (start != std::string::npos && end != std::string::npos) {
        dishes.push_back(currentDish.substr(start, end - start + 1));
    }
    return dishes;
}

// Convert a string to lowercase in-place.
static std::string toLower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Returns true if a JSON object likely denotes the protagonist spawn (name/type/class contains "protagonist").
static bool nameIsProtagonist(const json& obj) {
    auto has = [&](const char* k){ return obj.contains(k) && obj[k].is_string(); };
    std::string key;
    if (has("name"))  key += " " + toLower(obj["name"].get<std::string>());
    if (has("type"))  key += " " + toLower(obj["type"].get<std::string>());
    if (has("class")) key += " " + toLower(obj["class"].get<std::string>());
    return key.find("protagonist") != std::string::npos;
}


// Point-in-polygon test using ray casting.
static bool pointInPolygon(const sf::Vector2f& p, const std::vector<sf::Vector2f>& poly) {
    bool inside = false;
    const size_t n = poly.size();
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const sf::Vector2f& a = poly[j];
        const sf::Vector2f& b = poly[i];
        const bool intersect = ((a.y > p.y) != (b.y > p.y)) &&
            (p.x < (b.x - a.x) * (p.y - a.y) / ((b.y - a.y) == 0.f ? 1e-6f : (b.y - a.y)) + a.x);
        if (intersect) inside = !inside;
    }
    return inside;
}


/**
 * @brief Load a TMJ map from a JSON file and initialize internal data.
 *
 * This function opens the TMJ JSON file, parses its contents and initializes
 * tilesets, tile layers and object layers. It also performs texture extrusion
 * when requested to avoid texture bleeding.
 *
 * @param filepath Path to the TMJ JSON file.
 * @param extrude Number of extruded border pixels to add for tileset textures.
 * @return true if the map was successfully loaded; false otherwise.
 */
bool TMJMap::loadFromFile(
    const std::string& filepath, 
    int extrude
) {
    cleanup();
    
    std::ifstream in(filepath);
    if (!in) {
        Logger::error("Failed to open TMJ file: " + filepath);
        return false;
    }

    json j;
    try { 
        in >> j; 
    } catch (...) {
        Logger::error("JSON parse failed for file: " + filepath);
        return false;
    }

    namespace fs = std::filesystem;
    fs::path tmjPath(filepath);
    fs::path tmjDir = tmjPath.parent_path();

    // Add debug test sprites to the map:
    // Create a red test sprite placed at the map center
    sf::Image redImage(sf::Vector2u(50, 50), sf::Color::Red);
    sf::Texture redTexture;
    if (redTexture.loadFromImage(redImage)) {
        sf::Sprite redSprite(redTexture);
        redSprite.setPosition(sf::Vector2f{3000, 600});  // map center
        tiles.push_back(redSprite);
        Logger::info("Added RED test tile at map center (3000, 600)");
    }

    // Create a green test sprite placed near the spawn point
    sf::Image greenImage(sf::Vector2u(50, 50), sf::Color::Green);
    sf::Texture greenTexture;
    if (greenTexture.loadFromImage(greenImage)) {
        sf::Sprite greenSprite(greenTexture);
        greenSprite.setPosition(sf::Vector2f{2370, 105});  // spawn point position
        tiles.push_back(greenSprite);
        Logger::info("Added GREEN test tile at spawn point (2370, 105)");
    }

    // Create a blue test sprite placed at the top-left of the map
    sf::Image blueImage(sf::Vector2u(50, 50), sf::Color::Blue);
    sf::Texture blueTexture;
    if (blueTexture.loadFromImage(blueImage)) {
        sf::Sprite blueSprite(blueTexture);
        blueSprite.setPosition(sf::Vector2f{100, 100});  // top-left corner
        tiles.push_back(blueSprite);
        Logger::info("Added BLUE test tile at top-left (100, 100)");
    }

    return parseMapData(j, tmjDir.string(), extrude);
}


/**
 * @brief Parse the top-level TMJ JSON structure into internal map data.
 *
 * This method extracts map dimensions, tilesets, tile layers and object
 * layers. It delegates tileset loading and object parsing to helper methods.
 *
 * @param j Parsed JSON object representing the TMJ file.
 * @param baseDir Base directory used to resolve relative tileset image paths.
 * @param extrude Extrusion amount passed to tileset texture creation.
 * @return true on success, false on parse or resource errors.
 */
bool TMJMap::parseMapData(
    const json& j, 
    const std::string& baseDir, 
    int extrude
) {
    // Parse basic map info
    if (!j.contains("width") || !j.contains("height") || 
        !j.contains("tilewidth") || !j.contains("tileheight")) {
        Logger::error("Map missing required dimensions");
        return false;
    }

    mapWidthTiles = j["width"];
    mapHeightTiles = j["height"];
    tileWidth = j["tilewidth"];
    tileHeight = j["tileheight"];

    // Load tilesets
    if (j.contains("tilesets") && j["tilesets"].is_array()) {
        if (!loadTilesets(j["tilesets"], baseDir, extrude)) {
            return false;
        }
    }

    // Parse layers and objects
    if (j.contains("layers") && j["layers"].is_array()) {
        parseObjectLayers(j["layers"]);
        
        // Recursive layer processing for tile layers
        std::function<void(const json&, float, float, float)> processLayer;
        processLayer = [&](
            const json& L, 
            float parentOffx, 
            float parentOffy, 
            float parentOpacity
        ) {
            if (!L.contains("type") || !L["type"].is_string()) return;
            std::string type = L["type"].get<std::string>();

            float offx = parentOffx;
            float offy = parentOffy;
            float opacity = parentOpacity;

            if (L.contains("offsetx") && L["offsetx"].is_number()) {
                offx += L["offsetx"].get<float>();
            }
            if (L.contains("offsety") && L["offsety"].is_number()) {
                offy += L["offsety"].get<float>();
            }
            if (L.contains("opacity") && L["opacity"].is_number()) {
                opacity *= L["opacity"].get<float>();
            }

            if (type == "group") {
                if (L.contains("layers") && L["layers"].is_array()) {
                    for (const auto& sub : L["layers"]) processLayer(sub, offx, offy, opacity);
                }
                return;
            }

            if (type != "tilelayer") return;

            bool visible = L.value("visible", true);
            if (!visible) return;

            int lw = L.value("width", mapWidthTiles);
            int lh = L.value("height", mapHeightTiles);

            if (!L.contains("data") || !L["data"].is_array()) return;

            std::vector<int> data;
            try { 
                data = L["data"].get<std::vector<int>>(); 
            } catch (...) {
                return;
            }

            for (int y = 0; y < lh; ++y) {
                for (int x = 0; x < lw; ++x) {
                    int gid = data[x + y * lw];
                    if (gid == 0) continue;

                    TilesetInfo* ts = findTilesetForGid(gid);
                    if (!ts || ts->texture.getSize().x == 0) continue;

                    int localId = gid - ts->firstGid;
                    if (localId < 0 || localId >= ts->tileCount) continue;

                    const int tu = localId % ts->columns;
                    const int tv = localId / ts->columns;
                    const int sx = ts->margin + tu * (ts->tileWidth + ts->spacing);
                    const int sy = ts->margin + tv * (ts->tileHeight + ts->spacing);

                    sf::IntRect rect({
                        {sx, sy}, 
                        {ts->tileWidth, ts->tileHeight}
                    });
                    sf::Sprite spr(ts->texture, rect);

                    Logger::debug(
                        "Sprite created - TextureRect: (" + 
                        std::to_string(rect.position.x) + "," + std::to_string(rect.position.y) + 
                        ") " + std::to_string(rect.size.x) + "x" + std::to_string(rect.size.y) +
                        " Position: (" + std::to_string(spr.getPosition().x) + "," + 
                        std::to_string(spr.getPosition().y) + ")"
                    );

                    spr.setPosition(sf::Vector2f{
                        offx + static_cast<float>(x * tileWidth),
                        offy + static_cast<float>(y * tileHeight)
                    });
                    
                    if (opacity < 1.f) {
                        spr.setColor(sf::Color(
                            255, 255, 255,
                            static_cast<uint8_t>(std::lround(255.f * opacity))
                        ));
                    }

                    tiles.push_back(spr);
                }
            }
        };

        for (const auto& L : j["layers"]) processLayer(L, 0.f, 0.f, 1.f);
    }

    Logger::info("TMJMap loaded: " + std::to_string(mapWidthTiles) + "x" + 
                 std::to_string(mapHeightTiles) + ", tiles: " + 
                 std::to_string(tiles.size()) + ", text objects: " + 
                 std::to_string(textObjects.size()));
    
    return true;
}


/**
 * @brief Parse object layers from TMJ JSON and populate runtime object lists.
 *
 * This function scans object groups for spawn points, text labels, entrance
 * areas and NotWalkable polygons, converting them into lightweight POD
 * structures used by the renderer and gameplay systems.
 *
 * @param layers JSON array containing TMJ layer objects.
 */
void TMJMap::parseObjectLayers(const json& layers) {
    for (const auto& L : layers) {
        if (!L.contains("type") || !L["type"].is_string() || L["type"] != "objectgroup") continue;
        if (!L.contains("objects") || !L["objects"].is_array()) continue;

        std::string layerName = L.value("name", "objectgroup");
        std::string lnameLower = toLower(layerName);

        // 1) protagonist spawn points
        for (const auto& obj : L["objects"]) {
            if (!obj.is_object()) continue;
            if (nameIsProtagonist(obj)) {
                float ox = obj.value("x", 0.f);
                float oy = obj.value("y", 0.f);
                float ow = obj.value("width", 0.f);
                float oh = obj.value("height", 0.f);
                if (ow > 0.f || oh > 0.f) { ox += ow * 0.5f; oy += oh * 0.5f; }
                else { ox += tileWidth * 0.5f; oy += tileHeight * 0.5f; }
                spawnX = ox; spawnY = oy;
                Logger::info("Found protagonist spawn at: " + std::to_string(ox) + ", " + std::to_string(oy));
            }
        }

        // 2) text objects
        bool shouldDisplayText = (lnameLower == "building_names") ||
                                 (lnameLower.find("text") != std::string::npos) ||
                                 (lnameLower.find("name") != std::string::npos);
        if (shouldDisplayText) {
            for (const auto& obj : L["objects"]) {
                if (!obj.is_object()) continue;
                TextObject t;
                t.x = obj.value("x", 0.f);
                t.y = obj.value("y", 0.f);
                t.width = obj.value("width", 0.f);
                t.height = obj.value("height", 0.f);
                if (obj.contains("text") && obj["text"].is_object()) {
                    const auto& textData = obj["text"];
                    t.text = textData.value("text", "");
                    t.fontSize = textData.value("pixelsize", 16u);
                    t.bold = textData.value("bold", false);
                    t.italic = textData.value("italic", false);
                    t.halign = toLower(textData.value("halign", "left"));
                    t.valign = toLower(textData.value("valign", "top"));
                    if (textData.contains("color") && textData["color"].is_string()) {
                        std::string colorStr = textData["color"].get<std::string>();
                        if (colorStr.length() == 7 && colorStr[0] == '#') {
                            try {
                                int r = std::stoi(colorStr.substr(1,2), nullptr, 16);
                                int g = std::stoi(colorStr.substr(3,2), nullptr, 16);
                                int b = std::stoi(colorStr.substr(5,2), nullptr, 16);
                                t.color = sf::Color(r,g,b);
                            } catch (...) { t.color = sf::Color::White; }
                        }
                    }
                } else if (obj.contains("text") && obj["text"].is_string()) {
                    t.text = obj["text"].get<std::string>();
                    t.fontSize = 16;
                    t.color = sf::Color::White;
                }
                if (!t.text.empty()) textObjects.push_back(std::move(t));
            }
        }

        // 3) entrance areas
        for (const auto& obj : L["objects"]) {
            if (!obj.is_object()) continue;

            bool layerIsEntrance = (lnameLower == "entrance");
            bool objectIsEntrance = false;
            if (obj.contains("type") && obj["type"].is_string()) {
                std::string t = toLower(obj["type"].get<std::string>());
                if (t == "entrance") objectIsEntrance = true;
            }
            if (!objectIsEntrance && obj.contains("class") && obj["class"].is_string()) {
                std::string c = toLower(obj["class"].get<std::string>());
                if (c == "entrance") objectIsEntrance = true;
            }

            if (!layerIsEntrance && !objectIsEntrance) continue;

            EntranceArea a;
            a.x = obj.value("x", 0.f);
            a.y = obj.value("y", 0.f);
            a.width = obj.value("width", 0.f);
            a.height = obj.value("height", 0.f);
            a.name = obj.value("name", "");
            a.target = "";
            if (obj.contains("properties") && obj["properties"].is_array()) {
                for (const auto& p : obj["properties"]) {
                    if (!p.is_object()) continue;
                    std::string pname = p.value("name", "");
                    if (pname == "target" && p.contains("value") && p["value"].is_string()) {
                        a.target = p["value"].get<std::string>();
                    } else if (pname == "targetX" && p.contains("value") && p["value"].is_number()) {
                        a.targetX = p["value"].get<float>();
                    } else if (pname == "targetY" && p.contains("value") && p["value"].is_number()) {
                        a.targetY = p["value"].get<float>();
                    }
                }
            }
            if (a.target.empty() && obj.contains("target") && obj["target"].is_string()) {
                a.target = obj["target"].get<std::string>();
            }
            if (!a.targetX && obj.contains("targetX") && obj["targetX"].is_number()) a.targetX = obj["targetX"].get<float>();
            if (!a.targetY && obj.contains("targetY") && obj["targetY"].is_number()) a.targetY = obj["targetY"].get<float>();

            Logger::info("Parsed entrance '" + a.name + "' target='" + a.target + "'");
            if (a.targetX && a.targetY) {
                Logger::info("  targetX/Y = " + std::to_string(*a.targetX) + ", " + std::to_string(*a.targetY));
            } else {
                Logger::info("  no explicit targetX/targetY");
            }

            entranceAreas.push_back(std::move(a));
        }

        // 4) Parse NotWalkable layers
        if (lnameLower.find("notwalkable") != std::string::npos) {
            for (const auto& obj : L["objects"]) {
                if (!obj.is_object()) continue;

                if (obj.contains("polygon") && obj["polygon"].is_array()) {
                    float ox = obj.value("x", 0.f);
                    float oy = obj.value("y", 0.f);
                    BlockPoly poly;
                    poly.points.reserve(obj["polygon"].size());

                    float minx = std::numeric_limits<float>::max();
                    float miny = std::numeric_limits<float>::max();
                    float maxx = std::numeric_limits<float>::lowest();
                    float maxy = std::numeric_limits<float>::lowest();

                    for (const auto& pt : obj["polygon"]) {
                        if (!pt.is_object()) continue;
                        float px = ox + pt.value("x", 0.f);
                        float py = oy + pt.value("y", 0.f);
                        poly.points.emplace_back(px, py);
                        minx = std::min(minx, px); miny = std::min(miny, py);
                        maxx = std::max(maxx, px); maxy = std::max(maxy, py);
                    }

                    if (poly.points.size() >= 3) {
                        poly.bounds = sf::FloatRect(sf::Vector2f{minx, miny},
                                                   sf::Vector2f{maxx - minx, maxy - miny});
                        notWalkPolys.push_back(std::move(poly));
                        Logger::info("NotWalkable polygon parsed, pts=" + std::to_string(notWalkPolys.back().points.size()));
                    } else {
                        Logger::warn("NotWalkable polygon ignored: less than 3 points");
                    }
                } else if (obj.contains("width") && obj.contains("height")) {
                    float rx = obj.value("x", 0.f);
                    float ry = obj.value("y", 0.f);
                    float rw = obj.value("width", 0.f);
                    float rh = obj.value("height", 0.f);
                    sf::FloatRect r(sf::Vector2f{rx, ry}, sf::Vector2f{rw, rh});
                    notWalkRects.push_back(r);
                    Logger::info("NotWalkable rect parsed at (" + std::to_string(rx) + "," + std::to_string(ry) + 
                                 ") size " + std::to_string(rw) + "x" + std::to_string(rh));
                }
            }

        }

        // 5) Parse Chef objects (修复作用域错误)
        for (const auto& obj : L["objects"]) {
            if (!obj.is_object()) continue;

            bool isChef = false;
            if (obj.contains("type") && obj["type"].is_string()) {
                std::string t = toLower(obj["type"].get<std::string>());
                if (t == "chef") isChef = true;
            }
            if (!isChef && obj.contains("class") && obj["class"].is_string()) {
                std::string c = toLower(obj["class"].get<std::string>());
                if (c == "chef") isChef = true;
            }

            if (!isChef) continue;

            Chef chef;
            chef.name = obj.value("name", "chef");
            chef.rect = sf::FloatRect(
                sf::Vector2f(obj.value("x", 0.f), obj.value("y", 0.f)),
                sf::Vector2f(obj.value("width", 16.f), obj.value("height", 17.f))
            );

            m_chefs.push_back(std::move(chef));
            Logger::info("Parsed chef object: " + chef.name + " at (" + 
                         std::to_string(chef.rect.position.x) + ", " + 
                         std::to_string(chef.rect.position.y) + ")");
        }

        // 6) 解析GameTriggerArea (新增)
        layerName = L.value("name", "");
        if (layerName == "game_triggers") { // 确保Tiled中图层名为"game_triggers"
            for (const auto& obj : L["objects"]) {
                GameTriggerArea trigger;
                trigger.x = obj["x"];
                trigger.y = obj["y"];
                trigger.width = obj["width"];
                trigger.height = obj["height"];
                trigger.name = obj["name"];
                
                // 解析自定义属性gameType
                for (const auto& prop : obj["properties"]) {
                    if (prop["name"] == "gameType") {
                        trigger.gameType = prop["value"];
                    }
                }
                
                gameTriggers.push_back(trigger);
            }
        }

        // 7) Parse Interaction objects (Counter)
        if (lnameLower == "interaction") {
            for (const auto& obj : L["objects"]) {
                if (!obj.is_object()) continue;

                bool isCounter = false;
                if (obj.contains("type") && obj["type"].is_string()) {
                    std::string t = toLower(obj["type"].get<std::string>());
                    if (t == "counter") isCounter = true;
                }
                if (!isCounter && obj.contains("class") && obj["class"].is_string()) {
                    std::string cls = toLower(obj["class"].get<std::string>());
                    if (cls == "counter") isCounter = true;
                }

                if (!isCounter) continue;

                InteractionObject io;
                io.type = "counter";
                io.name = obj.value("name", "counter");
                io.rect = sf::FloatRect(
                    sf::Vector2f(obj.value("x", 0.f), obj.value("y", 0.f)),
                    sf::Vector2f(obj.value("width", 0.f), obj.value("height", 0.f))
                );

                if (obj.contains("properties") && obj["properties"].is_array()) {
                    for (const auto& p : obj["properties"]) {
                        if (!p.is_object()) continue;
                        std::string pname = p.value("name", "");
                        if (pname == "dishes" && p.contains("value") && p["value"].is_string()) {
                            std::string dishesStr = p["value"].get<std::string>();
                            io.options = splitDishesString(dishesStr);
                            break;
                        }
                    }
                }

                interactionObjects.push_back(io);
                Logger::info("Successfully parsed Counter: " + io.name + 
                             " | Rect: (" + std::to_string(io.rect.position.x) + "," + std::to_string(io.rect.position.y) + 
                             ") " + std::to_string(io.rect.size.x) + "x" + std::to_string(io.rect.size.y) +
                             " | Dishes: " + std::to_string(io.options.size()));
            }
        }
    }
}

/**
 * @brief Load tileset definitions and build textures (optionally extruded).
 *
 * For each tileset entry in the TMJ data, this loads the source image,
 * computes columns/rows, and optionally creates an extruded texture to
 * avoid sampling artifacts at tile borders.
 *
 * @param tilesetsData JSON array of tileset descriptors.
 * @param baseDir Base directory for resolving tileset image paths.
 * @param extrude Extrusion pixel count to add around tiles when building textures.
 * @return true if tilesets loaded successfully; false if critical resources are missing.
 */
bool TMJMap::loadTilesets(
    const json& tilesetsData, 
    const std::string& baseDir, 
    int extrude
) {
    for (const auto& tsj : tilesetsData) {
        TilesetInfo ts;

        ts.firstGid = tsj.value("firstgid", 1);
        ts.name = tsj.value("name", "tileset");

        std::string relImagePath = tsj.value("image", "");

        if (relImagePath.empty()) {
            Logger::warn("Tileset '" + ts.name + "' has no embedded image");
            tilesets.push_back(ts);
            continue;
        }

        std::string imagePath = baseDir + "/" + relImagePath;
        ts.imagePath = imagePath;

        ts.origTileW = tsj.value("tilewidth", tileWidth);
        ts.origTileH = tsj.value("tileheight", tileHeight);
        ts.origSpacing = tsj.value("spacing", 0);
        ts.origMargin = tsj.value("margin", 0);
        ts.columns = tsj.value("columns", 0);
        ts.tileCount = tsj.value("tilecount", 0);

        sf::Texture original;

        if (!original.loadFromFile(imagePath)) {
            Logger::error("Failed to load tileset image: " + imagePath);
            tilesets.push_back(ts);
            continue;
        }

        if (ts.columns == 0) {
            auto size = original.getSize();
            ts.columns = static_cast<int>(size.x) / ts.origTileW;
        }
        if (ts.tileCount == 0) {
            auto size = original.getSize();
            int rows = static_cast<int>(size.y) / ts.origTileH;
            ts.tileCount = ts.columns * rows;
        }

        sf::Image src = original.copyToImage();
        sf::Texture extruded;

        if (makeExtrudedTexture(
            src, 
            ts.origTileW, 
            ts.origTileH, 
            ts.columns, 
            ts.origSpacing, 
            ts.origMargin, 
            extrude, 
            extruded
        )) {
            ts.texture = std::move(extruded);
            ts.tileWidth = ts.origTileW + 2 * extrude;
            ts.tileHeight = ts.origTileH + 2 * extrude;
            ts.spacing = 0;
            ts.margin = 0;
        } else {
            ts.texture = std::move(original);
            ts.tileWidth = ts.origTileW;
            ts.tileHeight = ts.origTileH;
            ts.spacing = ts.origSpacing;
            ts.margin = ts.origMargin;
        }

        tilesets.push_back(ts);
        Logger::info(
            "Loaded tileset: " + ts.name + 
            " (" + std::to_string(ts.tileCount) + " tiles)"
        );
    }
    
    return true;
}


/**
 * @brief Create an extruded texture image from a tileset source image.
 *
 * The function copies tile pixels into a destination image and duplicates
 * edge pixels into the extrusion border so textured quads avoid bleeding.
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
bool TMJMap::makeExtrudedTexture(
    const sf::Image& src, 
    int srcTileW, 
    int srcTileH,
    int columns, 
    int spacing, 
    int margin, 
    int extrude, 
    sf::Texture& outTex
) {
    if (srcTileW <= 0 || srcTileH <= 0 || columns <= 0) {
        Logger::error("Invalid tile dimensions or columns");
        return false;
    }

    const sf::Vector2u isz = src.getSize();
    
    // Fix: usableW/usableH represent usable pixel extents and should not include spacing
    const int usableW = static_cast<int>(isz.x) - margin * 2;
    const int usableH = static_cast<int>(isz.y) - margin * 2;

    // Fix: compute rows taking spacing into account
    const int cols = columns;
    const int rows = (usableH + spacing) / (srcTileH + spacing);
    
    if (rows <= 0) {
        Logger::error("Invalid rows calculation: " + std::to_string(rows));
        Logger::error(
            "Input: srcSize=" + std::to_string(isz.x) + "x" + std::to_string(isz.y) +
            " tile=" + std::to_string(srcTileW) + "x" + std::to_string(srcTileH) +
            " margin=" + std::to_string(margin) + " spacing=" + std::to_string(spacing)
        );
        return false;
    }

    const int tileOutW = srcTileW + 2 * extrude;
    const int tileOutH = srcTileH + 2 * extrude;
    const int dstW = cols * tileOutW;
    const int dstH = rows * tileOutH;

    // Add debug information
    Logger::debug("Creating extruded texture: " + 
                 std::to_string(dstW) + "x" + std::to_string(dstH) +
                 " from " + std::to_string(isz.x) + "x" + std::to_string(isz.y) +
                 " tiles: " + std::to_string(cols) + "x" + std::to_string(rows));

    sf::Image dst(
        sf::Vector2u(
            static_cast<unsigned>(dstW), 
            static_cast<unsigned>(dstH)
        ), 
        sf::Color::Transparent
    );

    auto getPix = [&](int x, int y) -> sf::Color {
        if (x < 0 || y < 0 || x >= static_cast<int>(isz.x) || y >= static_cast<int>(isz.y)) {
            return sf::Color::Transparent;
        }
        return src.getPixel(sf::Vector2u(static_cast<unsigned>(x), static_cast<unsigned>(y)));
    };

    int tilesProcessed = 0;
    
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            // Fix: calculate source coordinates considering spacing
            const int sx = margin + c * (srcTileW + spacing);
            const int sy = margin + r * (srcTileH + spacing);
            
            // Check whether source coordinates are inside the image bounds
            if (sx + srcTileW > static_cast<int>(isz.x) || sy + srcTileH > static_cast<int>(isz.y)) {
                Logger::warn("Tile coordinates out of bounds: (" + 
                            std::to_string(sx) + "," + std::to_string(sy) + ")");
                continue;
            }
            
            const int dx = c * tileOutW;
            const int dy = r * tileOutH;

            // Copy main region
            for (int yy = 0; yy < srcTileH; ++yy) {
                for (int xx = 0; xx < srcTileW; ++xx) {
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + extrude + xx),
                            static_cast<unsigned>(dy + extrude + yy)
                        ),
                        getPix(sx + xx, sy + yy)
                    );
                }
            }

            // Extrude edges (unchanged)
            for (int yy = 0; yy < srcTileH; ++yy) {
                const sf::Color L = getPix(sx, sy + yy);
                const sf::Color R = getPix(sx + srcTileW - 1, sy + yy);

                for (int e = 0; e < extrude; ++e) {
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + e), 
                            static_cast<unsigned>(dy + extrude + yy)
                        ), 
                        L
                    );
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + extrude + srcTileW + e), 
                            static_cast<unsigned>(dy + extrude + yy)
                        ), 
                        R
                    );
                }
            }

            for (int xx = 0; xx < srcTileW; ++xx) {
                const sf::Color T = getPix(sx + xx, sy);
                const sf::Color B = getPix(sx + xx, sy + srcTileH - 1);

                for (int e = 0; e < extrude; ++e) {
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + extrude + xx), 
                            static_cast<unsigned>(dy + e)
                        ), 
                        T
                    );
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + extrude + xx), 
                            static_cast<unsigned>(dy + extrude + srcTileH + e)
                        ), 
                        B
                    );
                }
            }

            // Extrude corners (unchanged)
            const sf::Color TL = getPix(sx, sy);
            const sf::Color TR = getPix(sx + srcTileW - 1, sy);
            const sf::Color BL = getPix(sx, sy + srcTileH - 1);
            const sf::Color BR = getPix(sx + srcTileW - 1, sy + srcTileH - 1);

            for (int ey = 0; ey < extrude; ++ey) {
                for (int ex = 0; ex < extrude; ++ex) {
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + ex), 
                            static_cast<unsigned>(dy + ey)
                        ), 
                        TL
                    );
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + ex + extrude + srcTileW), 
                            static_cast<unsigned>(dy + ey)
                        ), 
                        TR
                    );
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + ex), 
                            static_cast<unsigned>(dy + ey + extrude + srcTileH)
                        ), 
                        BL
                    );
                    dst.setPixel(
                        sf::Vector2u(
                            static_cast<unsigned>(dx + ex + extrude + srcTileW), 
                            static_cast<unsigned>(dy + ey + extrude + srcTileH)
                        ), 
                        BR
                    );
                }
            }
            
            tilesProcessed++;
        }
    }

    Logger::debug("Processed " + std::to_string(tilesProcessed) + " tiles for extrusion");

    if (!outTex.loadFromImage(dst)) {
        Logger::error("Failed to load extruded texture from image");
        return false;
    }
    
    outTex.setSmooth(false);

    Logger::debug(
        "Texture created - Size: " + 
        std::to_string(outTex.getSize().x) + "x" + 
        std::to_string(outTex.getSize().y)
    );
    
    return true;
}


/**
 * @brief Find the tileset information that contains a given global tile ID (gid).
 *
 * Tileset entries encode a firstGid value; this function returns the TilesetInfo
 * pointer for the tileset whose ID range covers the requested gid.
 *
 * @param gid Global tile ID to lookup.
 * @return Pointer to matching TilesetInfo or nullptr if not found.
 */
TilesetInfo* TMJMap::findTilesetForGid(int gid) {
    TilesetInfo* result = nullptr;

    for (auto& ts : tilesets) {
        if (gid >= ts.firstGid && gid < ts.firstGid + ts.tileCount) {
            if (!result || ts.firstGid > result->firstGid) 
                result = &ts;
        }
    }

    return result;
}


void TMJMap::cleanup() {
    tilesets.clear();
    tiles.clear();
    textObjects.clear();
    entranceAreas.clear();
    spawnX.reset();
    spawnY.reset();
}


// Check whether a feet point lies within any NotWalkable region (rectangles or polygons).
bool TMJMap::feetBlockedAt(const sf::Vector2f& feet) const {
    // Check rectangles first for quick rejection.
    for (const auto& r : notWalkRects) {
        if (r.contains(feet)) return true;
    }
    // Then check polygons using AABB rejection followed by point-in-polygon test.
    for (const auto& poly : notWalkPolys) {
        if (poly.bounds.contains(feet)) {
            if (pointInPolygon(feet, poly.points)) return true;
        }
    }
    return false;

}
