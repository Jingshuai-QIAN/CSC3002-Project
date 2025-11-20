// TMJ Viewer + building_names / entrance 扩展版
// - 渲染可见 tile 图层（支持 group 递归）+ 1px 外延防缝隙
// - 主角：方向键移动；相机自动跟随；停止保持最后朝向；对角速度归一化
// - 出生点优先级：命令行(@tile/@px) > spawns.json > TMJ protagonist > 地图中心
// - 新增：读取并显示 object layer: building_names（文字）与 entrance（入口矩形）
// - 视图固定显示 40×40 tiles（可按需修改）
//
// 编译（示例）：
// g++ your_new.cpp -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib \
//    -lsfml-graphics -lsfml-window -lsfml-system -o your_new
//
// 运行（示例）：
// ./your_new maps/lower_campus_map.tmj
// 或通过标准输入传入路径（无参时用默认）

#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <functional>

using json = nlohmann::json;

// -------------------- 自定义数据结构 --------------------

// tileset 信息（含 extrude 后图集纹理）
struct TilesetInfo {
    int firstGid      = 1;
    int tileCount     = 0;
    int columns       = 0;

    // extrude 后在图集中的像素尺寸
    int tileWidth     = 0;
    int tileHeight    = 0;
    int spacing       = 0;
    int margin        = 0;

    // 原始 tileset 参数（来自 TMJ）
    int origTileW     = 0;
    int origTileH     = 0;
    int origSpacing   = 0;
    int origMargin    = 0;

    std::string name;
    std::string imagePath;
    sf::Texture texture;    // extrude 后的纹理（或原纹理）
};

// 文本对象（来自 Tiled 的 objectgroup -> text）
struct TextObject {
    float x = 0.f, y = 0.f;
    float width = 0.f, height = 0.f;  // 文字框（用于对齐）
    std::string text;                  // 文本内容
    unsigned int fontSize = 16;        // 像素字体大小
    bool bold = false, italic = false; // 是否加粗/斜体
    sf::Color color = sf::Color::White;
    std::string halign = "left";       // left/center/right
    std::string valign = "top";        // top/center/bottom
};

// 入口区域（矩形，来自 objectgroup: entrance）
struct EntranceArea {
    float x = 0.f, y = 0.f;
    float width = 0.f, height = 0.f;
    std::string name; // 可选名字（用于跳转、标注等）
};

// 地图整体数据
struct TMJMap {
    int mapWidthTiles  = 0;
    int mapHeightTiles = 0;
    int tileWidth      = 0;
    int tileHeight     = 0;

    std::vector<TilesetInfo> tilesets;
    std::vector<sf::Sprite>  tiles;           // 已切好的 tile 精灵
    std::vector<TextObject>  textObjects;     // 楼名文字等
    std::vector<EntranceArea> entranceAreas;  // 入口区域

    std::optional<float> spawnX; // 出生点（像素坐标）
    std::optional<float> spawnY;

    int worldPixelWidth()  const { return mapWidthTiles * tileWidth; }
    int worldPixelHeight() const { return mapHeightTiles * tileHeight; }
};

// -------------------- 小工具函数 --------------------

static TilesetInfo* findTilesetForGid(std::vector<TilesetInfo>& v, int gid) {
    TilesetInfo* res = nullptr;
    for (auto& ts : v) {
        if (gid >= ts.firstGid && gid < ts.firstGid + ts.tileCount) {
            if (!res || ts.firstGid > res->firstGid) res = &ts;
        }
    }
    return res;
}

static std::string toLower(std::string s){
    for (char& c : s)
        c = static_cast<char>( std::tolower( static_cast<unsigned char>(c) ) );
    return s;
}

// 是否是主角对象（严格包含 "protagonist"）
static bool nameIsProtagonist(const nlohmann::json& obj){
    auto has = [&](const char* k){ return obj.contains(k) && obj[k].is_string(); };
    std::string key;
    if (has("name"))  key += " " + toLower(obj["name"].get<std::string>());
    if (has("type"))  key += " " + toLower(obj["type"].get<std::string>());
    if (has("class")) key += " " + toLower(obj["class"].get<std::string>());
    return key.find("protagonist") != std::string::npos;
}

// 1px 外延（extrude）生成图集纹理，减少缩放缝隙
static bool makeExtrudedTextureFromImage(const sf::Image& src,
                                         int srcTileW, int srcTileH,
                                         int columns, int spacing, int margin,
                                         int extrude,
                                         sf::Texture& outTex)
{
    if (srcTileW <= 0 || srcTileH <= 0 || columns <= 0) return false;

    const sf::Vector2u isz = src.getSize();
    const int usableW = static_cast<int>(isz.x) - margin*2 + spacing;
    const int usableH = static_cast<int>(isz.y) - margin*2 + spacing;

    const int cols = columns;
    if (cols <= 0) return false;

    const int rows = (usableH + spacing) / (srcTileH + spacing);
    if (rows <= 0) return false;

    const int tileOutW = srcTileW + 2*extrude;
    const int tileOutH = srcTileH + 2*extrude;
    const int dstW = cols * tileOutW;
    const int dstH = rows * tileOutH;
    if (dstW <= 0 || dstH <= 0) return false;

    sf::Image dst(sf::Vector2u(static_cast<unsigned>(dstW), static_cast<unsigned>(dstH)),
                  sf::Color::Transparent);

    auto getPix = [&](int x, int y)->sf::Color{
        if (x < 0 || y < 0) return sf::Color::Transparent;
        if (x >= static_cast<int>(isz.x) || y >= static_cast<int>(isz.y)) return sf::Color::Transparent;
        return src.getPixel(sf::Vector2u(static_cast<unsigned>(x), static_cast<unsigned>(y)));
    };

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const int sx = margin + c * (srcTileW + spacing);
            const int sy = margin + r * (srcTileH + spacing);
            const int dx = c * tileOutW;
            const int dy = r * tileOutH;

            // 复制主区域
            for (int yy = 0; yy < srcTileH; ++yy) {
                for (int xx = 0; xx < srcTileW; ++xx) {
                    dst.setPixel(
                        sf::Vector2u(static_cast<unsigned>(dx + extrude + xx),
                                     static_cast<unsigned>(dy + extrude + yy)),
                        getPix(sx + xx, sy + yy)
                    );
                }
            }

            // 左右外延
            for (int yy = 0; yy < srcTileH; ++yy) {
                const sf::Color L = getPix(sx, sy + yy);
                const sf::Color R = getPix(sx + srcTileW - 1, sy + yy);
                for (int e = 0; e < extrude; ++e) {
                    dst.setPixel(sf::Vector2u(static_cast<unsigned>(dx + e),
                                              static_cast<unsigned>(dy + extrude + yy)), L);
                    dst.setPixel(sf::Vector2u(static_cast<unsigned>(dx + extrude + srcTileW + e),
                                              static_cast<unsigned>(dy + extrude + yy)), R);
                }
            }

            // 上下外延
            for (int xx = 0; xx < srcTileW; ++xx) {
                const sf::Color T = getPix(sx + xx, sy);
                const sf::Color B = getPix(sx + xx, sy + srcTileH - 1);
                for (int e = 0; e < extrude; ++e) {
                    dst.setPixel(sf::Vector2u(static_cast<unsigned>(dx + extrude + xx),
                                              static_cast<unsigned>(dy + e)), T);
                    dst.setPixel(sf::Vector2u(static_cast<unsigned>(dx + extrude + xx),
                                              static_cast<unsigned>(dy + extrude + srcTileH + e)), B);
                }
            }

            // 四角外延
            const sf::Color TL = getPix(sx, sy);
            const sf::Color TR = getPix(sx + srcTileW - 1, sy);
            const sf::Color BL = getPix(sx, sy + srcTileH - 1);
            const sf::Color BR = getPix(sx + srcTileW - 1, sy + srcTileH - 1);
            for (int ey = 0; ey < extrude; ++ey) {
                for (int ex = 0; ex < extrude; ++ex) {
                    dst.setPixel(sf::Vector2u(static_cast<unsigned>(dx + ex),
                                              static_cast<unsigned>(dy + ey)), TL);
                    dst.setPixel(sf::Vector2u(static_cast<unsigned>(dx + ex + extrude + srcTileW),
                                              static_cast<unsigned>(dy + ey)), TR);
                    dst.setPixel(sf::Vector2u(static_cast<unsigned>(dx + ex),
                                              static_cast<unsigned>(dy + ey + extrude + srcTileH)), BL);
                    dst.setPixel(sf::Vector2u(static_cast<unsigned>(dx + ex + extrude + srcTileW),
                                              static_cast<unsigned>(dy + ey + extrude + srcTileH)), BR);
                }
            }
        }
    }

    if (!outTex.loadFromImage(dst)) return false;
    outTex.setSmooth(false);
    return true;
}

// -------------------- TMJ 加载 + 渲染（含 building_names / entrance 解析） --------------------
static bool loadTMJ(const std::string& path, TMJMap& map, int extrude = 1) {
    std::ifstream in(path);
    if (!in) { std::cerr << "open tmj failed: " << path << "\n"; return false; }

    json j;
    try { in >> j; } catch (...) { std::cerr << "json parse failed\n"; return false; }

    namespace fs = std::filesystem;
    fs::path tmjPath(path);
    fs::path tmjDir = tmjPath.parent_path();

    auto getInt = [&](const json& o, const char* k)->std::optional<int>{
        if (!o.contains(k) || !o[k].is_number_integer()) return std::nullopt;
        return o[k].get<int>();
    };

    // 基本信息
    auto w  = getInt(j,"width");
    auto h  = getInt(j,"height");
    auto tw = getInt(j,"tilewidth");
    auto th = getInt(j,"tileheight");
    if (!w || !h || !tw || !th) { std::cerr << "missing map dims\n"; return false; }
    map.mapWidthTiles  = *w;
    map.mapHeightTiles = *h;
    map.tileWidth      = *tw;
    map.tileHeight     = *th;

    // tilesets
    if (!j.contains("tilesets") || !j["tilesets"].is_array()) { std::cerr << "no tilesets\n"; return false; }
    map.tilesets.clear();

    for (const auto& tsj : j["tilesets"]) {
        TilesetInfo ts;
        auto fg = getInt(tsj,"firstgid");
        if (!fg) { std::cerr << "tileset no firstgid\n"; return false; }
        ts.firstGid = *fg;
        ts.name     = tsj.value("name", std::string("tileset"));

        std::string relImagePath = (tsj.contains("image") && tsj["image"].is_string())
                                 ? tsj["image"].get<std::string>() : "";

        ts.origTileW   = tsj.value("tilewidth",  map.tileWidth);
        ts.origTileH   = tsj.value("tileheight", map.tileHeight);
        ts.origSpacing = tsj.value("spacing", 0);
        ts.origMargin  = tsj.value("margin",  0);
        ts.columns     = tsj.value("columns", 0);
        ts.tileCount   = tsj.value("tilecount", 0);

        if (relImagePath.empty()) {
            std::cerr << "[loader] tileset '" << ts.name << "' has no embedded image (external .tsx?)\n";
            map.tilesets.push_back(std::move(ts));
            continue;
        }

        fs::path imgPath = tmjDir / relImagePath;
        ts.imagePath = imgPath.string();

        sf::Texture original;
        if (!original.loadFromFile(ts.imagePath)) {
            std::cerr << "[loader] load image failed: " << ts.imagePath << "\n";
            map.tilesets.push_back(std::move(ts));
            continue;
        } else {
            std::cout << "[loader] Loaded texture from: " << ts.imagePath << "\n";
        }

        if (ts.columns == 0) {
            auto s = original.getSize();
            ts.columns = static_cast<int>(s.x) / ts.origTileW;
        }
        if (ts.tileCount == 0) {
            auto s = original.getSize();
            int rows = static_cast<int>(s.y) / ts.origTileH;
            ts.tileCount = ts.columns * rows;
        }

        sf::Image src = original.copyToImage();
        sf::Texture extr;
        if (makeExtrudedTextureFromImage(src, ts.origTileW, ts.origTileH,
                                         ts.columns, ts.origSpacing, ts.origMargin,
                                         extrude, extr)) {
            ts.texture   = std::move(extr);
            ts.tileWidth  = ts.origTileW + 2*extrude;
            ts.tileHeight = ts.origTileH + 2*extrude;
            ts.spacing    = 0;
            ts.margin     = 0;
        } else {
            ts.texture   = std::move(original);
            ts.tileWidth  = ts.origTileW;
            ts.tileHeight = ts.origTileH;
            ts.spacing    = ts.origSpacing;
            ts.margin     = ts.origMargin;
        }

        std::cerr << "[tileset] '" << ts.name << "' GID=[" << ts.firstGid
                  << "," << (ts.firstGid + ts.tileCount - 1) << "], columns=" << ts.columns
                  << " tile=" << ts.origTileW << "x" << ts.origTileH
                  << " image=" << ts.imagePath << "\n";

        map.tilesets.push_back(std::move(ts));
    }

    // 渲染所有可见 tile 图层（支持 group 递归）
    if (!j.contains("layers") || !j["layers"].is_array()) {
        std::cerr << "JSON error: no 'layers' array in root\n";
        return false;
    }

    map.tiles.clear();
    int layerIdx = 0;

    std::function<void(const json&, float, float, float)> processLayer;
    processLayer = [&](const json& L, float parentOffx, float parentOffy, float parentOpacity) {
        if (!L.contains("type") || !L["type"].is_string()) return;
        std::string type = L["type"].get<std::string>();

        float offx = parentOffx;
        float offy = parentOffy;
        float opacity = parentOpacity;

        if (L.contains("offsetx") && L["offsetx"].is_number()) offx += L["offsetx"].get<float>();
        if (L.contains("offsety") && L["offsety"].is_number()) offy += L["offsety"].get<float>();
        if (L.contains("opacity") && L["opacity"].is_number()) opacity *= L["opacity"].get<float>();

        std::string lname = L.contains("name") && L["name"].is_string()
                          ? L["name"].get<std::string>() : "(noname)";

        if (type == "group") {
            std::cerr << "[group] '" << lname << "'\n";
            if (L.contains("layers") && L["layers"].is_array()) {
                for (const auto& sub : L["layers"]) processLayer(sub, offx, offy, opacity);
            }
            return;
        }

        if (type != "tilelayer") return; // objectgroup 由下方单独扫描

        ++layerIdx;

        bool visible = true;
        if (L.contains("visible") && L["visible"].is_boolean())
            visible = L["visible"].get<bool>();
        if (!visible) { std::cerr << "[layer] '"<<lname<<"' (invisible) skip\n"; return; }

        int lw = L.contains("width")  ? L["width"].get<int>()  : map.mapWidthTiles;
        int lh = L.contains("height") ? L["height"].get<int>() : map.mapHeightTiles;

        if (!L.contains("data") || !L["data"].is_array()) {
            std::cerr << "JSON error: tilelayer '" << lname << "' data missing / not array\n";
            return;
        }

        std::vector<int> data;
        try { data = L["data"].get<std::vector<int>>(); }
        catch (const std::exception& e) {
            std::cerr << "JSON error: tilelayer '" << lname << "' data to int[] failed: " << e.what() << "\n";
            return;
        }
        if (static_cast<int>(data.size()) != lw * lh) {
            std::cerr << "JSON error: layer '" << lname << "' data size mismatch: "
                      << data.size() << " vs " << (lw*lh) << "\n";
            return;
        }

        int painted = 0;
        for (int y = 0; y < lh; ++y) for (int x = 0; x < lw; ++x) {
            int gid = data[x + y * lw];
            if (gid == 0) continue;

            TilesetInfo* ts = findTilesetForGid(map.tilesets, gid);
            if (!ts || ts->texture.getSize().x == 0) continue;

            int localId = gid - ts->firstGid;
            if (localId < 0 || localId >= ts->tileCount) continue;

            const int tu = localId % ts->columns;
            const int tv = localId / ts->columns;

            const int sx = ts->margin + tu * (ts->tileWidth  + ts->spacing);
            const int sy = ts->margin + tv * (ts->tileHeight + ts->spacing);

            sf::IntRect rect(sf::Vector2i(sx, sy),
                             sf::Vector2i(ts->tileWidth, ts->tileHeight));

            sf::Sprite spr(ts->texture, rect);
            spr.setPosition(sf::Vector2f(
                offx + static_cast<float>(x * map.tileWidth),
                offy + static_cast<float>(y * map.tileHeight)
            ));
            if (opacity < 1.f) {
                spr.setColor(sf::Color(255,255,255,
                    static_cast<std::uint8_t>(std::lround(255.f * opacity))));
            }

            map.tiles.push_back(spr);
            ++painted;
        }

        std::cerr << "[layer] '" << lname << "' painted=" << painted
                  << " offset=(" << offx << "," << offy << ") opacity=" << opacity << "\n";
    };

    for (const auto& L : j["layers"]) processLayer(L, 0.f, 0.f, 1.f);

    // -------------------- 扫描 object layers：主角出生点 / 楼名文字 / 入口区域 --------------------
    map.textObjects.clear();
    map.entranceAreas.clear();

    if (j.contains("layers") && j["layers"].is_array()) {
        for (const auto& L : j["layers"]) {
            if (!L.contains("type") || !L["type"].is_string() || L["type"] != "objectgroup") continue;
            if (!L.contains("objects") || !L["objects"].is_array()) continue;

            // layer 名称
            std::string layerName = L.contains("name") && L["name"].is_string()
                                  ? L["name"].get<std::string>() : "objectgroup";
            std::string lnameLower = toLower(layerName);

            // ① 主角出生点（名字里包含 protagonist）
            for (const auto& obj : L["objects"]) {
                if (!obj.is_object()) continue;
                if (!nameIsProtagonist(obj)) continue;

                if (obj.contains("x") && obj.contains("y") && obj["x"].is_number() && obj["y"].is_number()) {
                    float ox = obj["x"].get<float>();
                    float oy = obj["y"].get<float>();
                    float ow = (obj.contains("width")  && obj["width"].is_number())  ? obj["width"].get<float>()  : 0.f;
                    float oh = (obj.contains("height") && obj["height"].is_number()) ? obj["height"].get<float>() : 0.f;

                    if (ow > 0.f || oh > 0.f) { ox += ow * 0.5f; oy += oh * 0.5f; }
                    else { ox += map.tileWidth * 0.5f; oy += map.tileHeight * 0.5f; }

                    map.spawnX = ox;
                    map.spawnY = oy;
                    std::cerr << "[spawn] protagonist at (" << ox << "," << oy << ") rect=" << ow << "x" << oh << "\n";
                    // 不跳出层循环，让其余对象也能被解析（文字/入口）
                }
            }

            // ② 楼名文字（layerName == "building_names"，或名字里包含 text / name）
            bool shouldDisplayText =
                (lnameLower == "building_names") ||
                (lnameLower.find("text") != std::string::npos) ||
                (lnameLower.find("name") != std::string::npos);

            if (shouldDisplayText) {
                for (const auto& obj : L["objects"]) {
                    if (!obj.is_object()) continue;

                    TextObject t;
                    t.x = obj.value("x", 0.f);
                    t.y = obj.value("y", 0.f);
                    t.width  = obj.value("width", 0.f);
                    t.height = obj.value("height", 0.f);

                    // Tiled 现代 text 对象格式（text 是一个对象）
                    if (obj.contains("text") && obj["text"].is_object()) {
                        const auto& textData = obj["text"];
                        t.text     = textData.value("text", "");
                        t.fontSize = textData.value("pixelsize", 16u);
                        t.bold     = textData.value("bold", false);
                        t.italic   = textData.value("italic", false);

                        if (textData.contains("halign") && textData["halign"].is_string())
                            t.halign = toLower(textData["halign"].get<std::string>());

                        if (textData.contains("valign") && textData["valign"].is_string())
                            t.valign = toLower(textData["valign"].get<std::string>());

                        // 颜色 "#RRGGBB"
                        if (textData.contains("color") && textData["color"].is_string()) {
                            std::string colorStr = textData["color"].get<std::string>();
                            if (colorStr.length() == 7 && colorStr[0] == '#') {
                                try {
                                    int r = std::stoi(colorStr.substr(1, 2), nullptr, 16);
                                    int g = std::stoi(colorStr.substr(3, 2), nullptr, 16);
                                    int b = std::stoi(colorStr.substr(5, 2), nullptr, 16);
                                    t.color = sf::Color(r, g, b);
                                } catch (...) {
                                    t.color = sf::Color::White;
                                }
                            }
                        }
                    }
                    // 兼容旧格式（text 是直接字符串）
                    else if (obj.contains("text") && obj["text"].is_string()) {
                        t.text     = obj["text"].get<std::string>();
                        t.fontSize = 16;
                        t.color    = sf::Color::White;
                    }

                    if (!t.text.empty()) {
                        map.textObjects.push_back(t);
                        std::cerr << "[text] '" << t.text << "' at (" << t.x << "," << t.y
                                  << "), size=" << t.fontSize << " align=" << t.halign << "/" << t.valign << "\n";
                    }
                }
            }

            // ③ 入口区域（要求 layerName == "entrance"）
            if (lnameLower == "entrance") {
                for (const auto& obj : L["objects"]) {
                    if (!obj.is_object()) continue;

                    EntranceArea a;
                    a.x = obj.value("x", 0.f);
                    a.y = obj.value("y", 0.f);
                    a.width  = obj.value("width", 0.f);
                    a.height = obj.value("height", 0.f);
                    if (obj.contains("name") && obj["name"].is_string())
                        a.name = obj["name"].get<std::string>();

                    map.entranceAreas.push_back(a);
                    std::cerr << "[entrance] '" << a.name << "' at (" << a.x << "," << a.y
                              << "), size=" << a.width << "x" << a.height << "\n";
                }
            }
        }
    }

    std::cerr << "[debug] total sprites=" << map.tiles.size()
              << " mapTiles=" << map.mapWidthTiles << "x" << map.mapHeightTiles
              << " tileSize=" << map.tileWidth << "x" << map.tileHeight << "\n";
    return true;
}

// 将 view 的中心限制在地图内（避免相机移出边界）
static void clampViewToMap(sf::View& view, int mapW, int mapH) {
    sf::Vector2f size   = view.getSize();
    sf::Vector2f half   = { size.x * 0.5f, size.y * 0.5f };
    sf::Vector2f center = view.getCenter();

    if (size.x >= mapW) center.x = std::max(0.f, float(mapW) * 0.5f);
    else center.x = std::clamp(center.x, half.x, float(mapW) - half.x);

    if (size.y >= mapH) center.y = std::max(0.f, float(mapH) * 0.5f);
    else center.y = std::clamp(center.y, half.y, float(mapH) - half.y);

    view.setCenter(center);
}

// -------------------- 辅助：地图名与出生点覆盖（命令行/标准输入） --------------------
static std::string trim(const std::string& s){
    const char* ws = " \t\r\n";
    auto l = s.find_first_not_of(ws);
    auto r = s.find_last_not_of(ws);
    if (l == std::string::npos) return "";
    return s.substr(l, r - l + 1);
}

// 若命令行未提供地图名，则尝试从标准输入读一行；否则返回 fallback
static std::string readMapPathFromStdinIfEmpty(int argc, char** argv, const std::string& fallback) {
    if (argc >= 2) return argv[1];
    std::string line;
    if (std::getline(std::cin, line)) {
        line = trim(line);
        if (!line.empty()) return line;
    }
    return fallback;
}

// 从同目录 spawns.json 读取出生点（tile 或 pixel）
void applySpawnFromSidecar(const std::string& tmjPath,
                           const TMJMap& map,
                           std::optional<float>& spawnX,
                           std::optional<float>& spawnY)
{
    namespace fs = std::filesystem;

    // 关键：永远在 tmj 所在目录找 spawns.json
    fs::path tmj(tmjPath);
    fs::path sidecarPath = tmj.parent_path() / "spawns.json";

    std::cerr << "[spawn] cwd = " << fs::current_path() << "\n";
    std::cerr << "[spawn] looking for sidecar at " << sidecarPath << "\n";

    std::ifstream ifs(sidecarPath);
    if (!ifs) {
        std::cerr << "[spawn] cannot open " << sidecarPath << "\n";
        return;
    }

    json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        std::cerr << "[spawn] parse spawns.json failed: " << e.what() << "\n";
        return;
    }

    // 先用 tmjPath 原样找，比如 "maps/bus.tmj"
    auto it = j.find(tmjPath);

    // 找不到就退一步，用文件名 "bus.tmj" 再试
    if (it == j.end()) {
        fs::path p(tmjPath);
        std::string fname = p.filename().string();   // "bus.tmj"
        it = j.find(fname);

        if (it == j.end()) {
            std::cerr << "[spawn] no entry for '" << tmjPath
                      << "' or '" << fname << "' in spawns.json\n";
            return;
        }
    }

    const json& cfg = *it;
    std::string mode = cfg.value("mode", "tile");

    if (mode == "tile") {
        int tx = cfg.value("x", 0);
        int ty = cfg.value("y", 0);

        float px = (tx + 0.5f) * map.tileWidth;
        float py = (ty + 0.5f) * map.tileHeight;

        spawnX = px;
        spawnY = py;

        std::cerr << "[spawn] tile(" << tx << "," << ty << ") -> pixel("
                  << px << "," << py << ") for " << tmjPath << "\n";
    } else if (mode == "pixel") {
        float px = cfg.value("x", 0.0f);
        float py = cfg.value("y", 0.0f);

        spawnX = px;
        spawnY = py;

        std::cerr << "[spawn] pixel spawn (" << px << "," << py
                  << ") for " << tmjPath << "\n";
    } else {
        std::cerr << "[spawn] unknown mode '" << mode
                  << "' for " << tmjPath << "\n";
    }
}

// 命令行覆盖：@tile tx ty / @px px py
static void applySpawnOverrideFromArgs(int argc, char** argv,
                                       const TMJMap& map,
                                       std::optional<float>& outX,
                                       std::optional<float>& outY)
{
    if (argc >= 5) {
        std::string mode = argv[2];
        if (mode == "@tile") {
            int tx = std::atoi(argv[3]);
            int ty = std::atoi(argv[4]);
            float px = tx * map.tileWidth  + map.tileWidth  * 0.5f;
            float py = ty * map.tileHeight + map.tileHeight * 0.5f;
            outX = px; outY = py;
            std::cerr << "[spawn] override by args (tile): tx=" << tx << " ty=" << ty
                      << " -> px=(" << px << "," << py << ")\n";
        } else if (mode == "@px") {
            float px = std::stof(argv[3]);
            float py = std::stof(argv[4]);
            outX = px; outY = py;
            std::cerr << "[spawn] override by args (px): (" << px << "," << py << ")\n";
        }
    }
}

// -------------------- 程序入口 --------------------
int main(int argc, char** argv) {
    // 地图名优先级：命令行 > 标准输入 > 默认主地图
    const std::string tmj = readMapPathFromStdinIfEmpty(argc, argv, "maps/lower_campus_map.tmj");
    std::cerr << "[main] tmj = " << tmj << "\n";   // 打印实际加载的路径

    TMJMap map;
    if (!loadTMJ(tmj, map, /*extrude*/1)) return 1;

    // 侧车文件 / 命令行 覆盖出生点
    applySpawnFromSidecar(tmj, map, map.spawnX, map.spawnY);
    applySpawnOverrideFromArgs(argc, argv, map, map.spawnX, map.spawnY);

    if (map.spawnX && map.spawnY) {
        std::cerr << "[main] spawn = (" << *map.spawnX << ", " << *map.spawnY << ")\n";
    } else {
        std::cerr << "[main] NO spawn (will use center)\n";
    }

    const int mapW = map.worldPixelWidth();
    const int mapH = map.worldPixelHeight();
    const int winW = std::min(mapW, 1200);
    const int winH = std::min(mapH, 800);

    sf::VideoMode vm{ sf::Vector2u(static_cast<unsigned>(winW), static_cast<unsigned>(winH)) };
    sf::RenderWindow window(vm, sf::String("TMJ Viewer"), sf::State::Windowed);
    window.setFramerateLimit(60);

    // 视图：固定显示 40×40 tiles
    const float tilesW = 60.f, tilesH = 40.f;
    const float viewW  = tilesW * map.tileWidth;
    const float viewH  = tilesH * map.tileHeight;
    sf::View view(sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(viewW, viewH)));

    // 相机/主角初始位置
    sf::Vector2f spawnPos = (map.spawnX && map.spawnY)
        ? sf::Vector2f(*map.spawnX, *map.spawnY)
        : sf::Vector2f(float(mapW) * 0.5f, float(mapH) * 0.5f);
    view.setCenter(spawnPos);

    // ================== 主角 sprite & 动画设置 ==================
    constexpr const char* HERO_TEXTURE_PATH = "tiles/F_01.png";
    sf::Texture heroTexture;
    // ✅ 修正：Texture 仍然是 loadFromFile, 不是 openFromFile
    if (!heroTexture.loadFromFile(HERO_TEXTURE_PATH)) {
        std::cerr << "[hero] failed to load hero texture: " << HERO_TEXTURE_PATH << "\n";
    }
    heroTexture.setSmooth(false);


    constexpr int HERO_FRAME_W    = 16; // 帧宽
    constexpr int HERO_FRAME_H    = 16; // 帧高
    constexpr int HERO_DIR_COLS   = 4;  // 列=方向（素材列顺序：下、右、上、左）
    constexpr int HERO_FRAME_ROWS = 3;  // 行=动画帧：0,1,2
    constexpr int ROW_SPACING     = 1;  // 行间距 1px

    enum Dir { Down=0, Left=1, Right=2, Up=3 };

    // 修正左右映射：你的素材列顺序为 下(0) 右(1) 上(2) 左(3)
    static const int DIR2COL[4] = {
        0, // Down  -> col 0
        3, // Left  -> col 3
        1, // Right -> col 1
        2  // Up    -> col 2
    };

    auto setHeroRect = [&](sf::Sprite& spr, int frameRow, int dirCol){
        frameRow = std::clamp(frameRow, 0, HERO_FRAME_ROWS-1);
        dirCol   = std::clamp(dirCol,   0, HERO_DIR_COLS-1);
        const int sx = dirCol * HERO_FRAME_W;                       // 横向无间距
        const int sy = frameRow * (HERO_FRAME_H + ROW_SPACING);     // 纵向含 1px 行间距
        spr.setTextureRect(sf::IntRect(sf::Vector2i(sx, sy),
                                       sf::Vector2i(HERO_FRAME_W, HERO_FRAME_H)));
    };

    sf::Sprite hero(heroTexture);
    int heroFrameRow = 1;         // 动画帧（0/1/2），静止用 1
    Dir lastDir      = Dir::Down; // 停止时保持
    int heroDirCol   = DIR2COL[lastDir];

    float heroScale = std::max(1.0f, static_cast<float>(map.tileWidth) / HERO_FRAME_W);
    hero.setScale(sf::Vector2f(heroScale, heroScale));
    hero.setOrigin(sf::Vector2f(HERO_FRAME_W * 0.5f, HERO_FRAME_H * 0.5f));
    setHeroRect(hero, heroFrameRow, heroDirCol);

    sf::Vector2f heroPos = spawnPos;
    hero.setPosition(heroPos);

    float animTimer = 0.f;
    const float ANIM_INTERVAL = 0.15f;
    const float HERO_SPEED    = 75.f;

    // 英文字体优先尝试项目内 fonts/DejaVuSans.ttf，找不到再尝试系统常见路径（macOS）
    std::vector<std::string> FONT_CANDIDATES = {
        "fonts/DejaVuSans.ttf",                        // 项目内自带（推荐把这个文件放进来）
        "/Library/Fonts/Arial.ttf",                    // macOS 常见
        "/System/Library/Fonts/Supplemental/Arial.ttf" // macOS 另一常见路径
    };

    sf::Font fontEN;
    bool fontLoaded = false;
    for (const auto& path : FONT_CANDIDATES) {
        if (fontEN.openFromFile(path)) {               // ★ SFML 3：openFromFile
            std::cerr << "[font] using: " << path << "\n";
            fontLoaded = true;
            break;
        }
    }
    if (!fontLoaded) {
        std::cerr << "[font] no English font found; building_names will be skipped.\n";
    }

    sf::Clock clock;

    // ====================== 主循环 ======================
    while (window.isOpen()) {
        while (auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) window.close();
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) window.close();

        float dt = clock.restart().asSeconds();

        bool left  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
        bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
        bool up    = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);
        bool down  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);

        sf::Vector2f move(0.f, 0.f);
        if (left)  move.x -= 1.f;
        if (right) move.x += 1.f;
        if (up)    move.y -= 1.f;
        if (down)  move.y += 1.f;

        bool moving = (move.x != 0.f || move.y != 0.f);

        // 仅单一方向键按下时更新朝向（避免斜向抖动）
        int horizontal = (left?1:0) + (right?1:0);
        int vertical   = (up?1:0)   + (down?1:0);
        if (horizontal + vertical == 1) {
            if (left)  lastDir = Dir::Left;
            if (right) lastDir = Dir::Right;
            if (up)    lastDir = Dir::Up;
            if (down)  lastDir = Dir::Down;
        }

        if (moving && move.x != 0.f && move.y != 0.f) {
            const float invLen = 1.0f / std::sqrt(move.x*move.x + move.y*move.y);
            move.x *= invLen; move.y *= invLen;
        }
        move *= HERO_SPEED * dt;

        sf::Vector2f heroPosNew = heroPos + move;
        float halfW = HERO_FRAME_W * 0.5f * heroScale;
        float halfH = HERO_FRAME_H * 0.5f * heroScale;
        heroPosNew.x = std::clamp(heroPosNew.x, halfW, static_cast<float>(mapW) - halfW);
        heroPosNew.y = std::clamp(heroPosNew.y, halfH, static_cast<float>(mapH) - halfH);

        heroPos = heroPosNew;
        hero.setPosition(heroPos);

        heroDirCol = DIR2COL[lastDir];

        if (moving) {
            animTimer += dt;
            if (animTimer >= ANIM_INTERVAL) {
                animTimer -= ANIM_INTERVAL;
                heroFrameRow = (heroFrameRow + 1) % HERO_FRAME_ROWS; // 0→1→2 循环
            }
        } else {
            heroFrameRow = 1;
            animTimer = 0.f;
        }
        setHeroRect(hero, heroFrameRow, heroDirCol);

        // 相机跟随 + 夹紧
        view.setCenter(heroPos);
        {
            sf::Vector2f sz = view.getSize();
            bool adjust = false;
            if (sz.x > static_cast<float>(mapW)) { sz.x = static_cast<float>(mapW); adjust = true; }
            if (sz.y > static_cast<float>(mapH)) { sz.y = static_cast<float>(mapH); adjust = true; }
            if (adjust) view.setSize(sz);
        }
        clampViewToMap(view, mapW, mapH);

        // =================== 绘制阶段 ===================
        window.clear(sf::Color::Black);
        window.setView(view);

        // 1) 地图 tiles
        for (const auto& s : map.tiles) window.draw(s);

        // 2) 主角
        window.draw(hero);

        // 3) entrance 入口区域（半透明蓝色矩形）
        for (const auto& area : map.entranceAreas) {
            sf::RectangleShape rect(sf::Vector2f(area.width, area.height));
            rect.setPosition(sf::Vector2f(area.x, area.y));
            rect.setFillColor(sf::Color(0, 100, 255, 100)); // 半透明蓝
            rect.setOutlineThickness(0);
            window.draw(rect);
        }

        // 4) building_names 文字（英文字体）
        if (fontLoaded) {
            for (const auto& t : map.textObjects) {
                // ★ SFML 3：必须带 font 构造
                sf::Text text(fontEN, t.text, t.fontSize);
                text.setFillColor(t.color);

                unsigned style = sf::Text::Regular;
                if (t.bold)   style |= sf::Text::Bold;
                if (t.italic) style |= sf::Text::Italic;
                text.setStyle(style);

                // （可选）给文字加描边，地图上更清晰
                text.setOutlineColor(sf::Color(0, 0, 0, 160));
                text.setOutlineThickness(1.f);

                // 计算对齐
                sf::FloatRect local = text.getLocalBounds();
                float textW = local.size.x;
                float textH = local.size.y;

                float px = t.x;
                float py = t.y;
                sf::Vector2f origin(0.f, 0.f);

                if (t.halign == "center") {
                    px += t.width * 0.5f;
                    origin.x = textW * 0.5f;
                } else if (t.halign == "right") {
                    px += t.width;
                    origin.x = textW;
                } // left -> origin.x = 0

                if (t.valign == "center") {
                    py += t.height * 0.5f;
                    origin.y = textH * 0.5f;
                } else if (t.valign == "bottom") {
                    py += t.height;
                    origin.y = textH;
                } // top -> origin.y = 0

                text.setOrigin(origin);
                text.setPosition(sf::Vector2f(px, py));

                window.draw(text);
            }
        }

        window.display();
    }

    return 0;
}