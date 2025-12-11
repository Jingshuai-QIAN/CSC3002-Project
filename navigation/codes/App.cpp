#include "App.h"

#include "Renderer/Renderer.h"
#include "MapLoader/MapLoader.h"
#include "MapLoader/TMJMap.h"
#include "Renderer/TextRenderer.h"
#include "Input/InputManager.h"
#include "Utils/Logger.h"
#include <filesystem>
#include "Character/Character.h"
#include "DialogSystem.h"
#include "QuizGame/QuizGame.h"
#include <algorithm> 
#include <unordered_map>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Window.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/Event.hpp>
#include <optional>
#include <cmath>
#include "Manager/TimeManager.h"
#include "Manager/TaskManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "QuizGame/LessonTrigger.h"

// Global variables for Achievement System 
static std::string g_achievementText = "";
static float g_achievementTimer = 0.0f;

bool showFinalResultScreen(Renderer& renderer, char grade, int starCount, const std::string& resultText);

// Helper to trigger task completion and show popup
static void handleTaskCompletion(TaskManager& taskManager, const std::string& taskId) {
    std::string achievement = taskManager.completeTask(taskId);
    if (!achievement.empty()) {
        g_achievementText = "Achievement Unlocked: " + achievement;
        g_achievementTimer = 3.0f; // Show for 3 seconds
        Logger::info("Achievement Unlocked: " + achievement);
    }
}

// Show hint for where to go
namespace {
    sf::Font g_modalFont;
    bool     g_modalFontReady = false;

    std::string g_hintText;
    float       g_hintTimer = 0.f;   // number of seconds
}

// make sure the typeface only loaded once
static bool ensureModalFont() {
    if (g_modalFontReady) return true;
    if (g_modalFont.openFromFile("fonts/arial.ttf")) {
        g_modalFontReady = true;
    } else {
        std::cerr << "[ModalHint] Failed to load fonts/arial.ttf\n";
    }
    return g_modalFontReady;
}

// enqueue a prompt displaying for several seconds
static void queueHint(const std::string& text, float seconds = 2.8f) {
    g_hintText  = text;
    g_hintTimer = seconds;
    ensureModalFont();
}




// Helper: detect whether character is inside an entrance and facing it.
static bool detectEntranceTrigger(const Character& character, const TMJMap* map, EntranceArea& outArea) {
    if (!map) return false;
    sf::Vector2f feet = character.getFeetPoint();
    for (const auto& a : map->getEntranceAreas()) {
        sf::FloatRect rect(sf::Vector2f(a.x, a.y), sf::Vector2f(a.width, a.height));

        if (rect.contains(feet)) {
            outArea = a;
            return true;
        }
    }
    return false;
}

static bool detectGameTrigger(const Character& character, const TMJMap* map, GameTriggerArea& outArea) {
    if (!map) return false;

    sf::Vector2f feet = character.getFeetPoint();
    for (const auto& gta : map->getGameTriggers()) {
        sf::FloatRect rect(sf::Vector2f(gta.x, gta.y), sf::Vector2f(gta.width, gta.height)); // 修正构造方式
        if (rect.contains(feet)) {
            outArea = gta;
            return true;
        }
    }

    return false;
}

// Detect the interaction with professors
static bool detectProfessorInteraction(const Character& character, const TMJMap* map, Professor& outProf) {
    if (!map) return false;

    sf::Vector2f center = character.getPosition();  // character center
    const auto& professors = map->getProfessors();

    for (const auto& prof : professors) {
        if (!prof.available) continue;

        if (prof.rect.contains(center)) {
            Logger::info("SUCCESS: Player touched Professor: " + prof.name);
            outProf = prof;
            return true;
        }
    }

    return false;
}

// Detect the area of store interaction
static bool detectShopTrigger(const Character& character, const TMJMap* map, ShopTrigger& outShop) {
    if (!map) {
        Logger::debug("detectShopTrigger: map is null");
        return false;
    }

    sf::Vector2f feet = character.getFeetPoint();
    const auto& shopTriggers = map->getShopTriggers();

    for (const auto& shop : shopTriggers) {
        // detect all areas of store interaction
        if (shop.rect.contains(feet)) {
            outShop = shop;
            Logger::info("Detected shop trigger area: " + shop.name);
            return true;
        }
    }

    return false;
}

// show the full-map modal (blocking) 
static void showFullMapModal(Renderer& renderer, const std::shared_ptr<TMJMap>& tmjMap, const ConfigManager& configManager) {
    auto dm = sf::VideoMode::getDesktopMode();
    sf::RenderWindow mapWin(dm, sf::String("Full Map"), sf::State::Windowed); 
    mapWin.setFramerateLimit(60);

    int mapW = tmjMap->getWorldPixelWidth();
    int mapH = tmjMap->getWorldPixelHeight();

    float winW = static_cast<float>(dm.size.x);  
    float winH = static_cast<float>(dm.size.y); 

    float mapWf = static_cast<float>(mapW);
    float mapHf = static_cast<float>(mapH);

    float scale = 1.f;
    if (mapW > 0 && mapH > 0) scale = std::min(winW / mapWf, winH / mapHf);

    float displayW = mapWf * scale;
    float displayH = mapHf * scale;

    float left = (winW - displayW) * 0.5f / winW;
    float top  = (winH - displayH) * 0.5f / winH;
    float vw   = (displayW / winW);
    float vh   = (displayH / winH);

    sf::View fullView(sf::FloatRect({0.f, 0.f}, {mapWf, mapHf}));
    fullView.setViewport(sf::FloatRect({left, top}, {vw, vh}));
    mapWin.setView(fullView);

    TextRenderer tr;
    tr.initialize(configManager.getRenderConfig().text.fontPath);

    float zoomFactor = 1.f;
    const float ZOOM_MIN = 0.25f;
    const float ZOOM_MAX = 8.0f;

    bool dragging = false;
    sf::Vector2i prevDragPixel{0,0};

    sf::View view = fullView;

    while (mapWin.isOpen()) {
        // event polling
        std::optional<sf::Event> evOpt = mapWin.pollEvent();
        while (evOpt.has_value()) {
            sf::Event& ev = evOpt.value();
            
            // window closing event 
            if (auto closed = ev.getIf<sf::Event::Closed>()) {
                mapWin.close(); 
                break; 
            }
            
            // keyboard pressing events
            if (auto keyPressed = ev.getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Escape) {
                    mapWin.close(); 
                    break; 
                }
            }

            // mouse wheel events
            if (auto mouseWheel = ev.getIf<sf::Event::MouseWheelScrolled>()) {
                float delta = mouseWheel->delta;
                if (delta > 0) zoomFactor *= 1.1f;
                else if (delta < 0) zoomFactor /= 1.1f;
                zoomFactor = std::clamp(zoomFactor, ZOOM_MIN, ZOOM_MAX);
                view.setSize(sf::Vector2f{
                    mapWf / zoomFactor,
                    mapHf / zoomFactor
                });
                mapWin.setView(view);
            }

            // mouse pressing events
            if (auto mousePressed = ev.getIf<sf::Event::MouseButtonPressed>()) {
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    dragging = true;
                    prevDragPixel = mousePressed->position;
                }
            }
            
            // mouse release events
            if (auto mouseReleased = ev.getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseReleased->button == sf::Mouse::Button::Left) {
                    dragging = false;
                }
            }
            
            // mouse movement events
            if (auto mouseMoved = ev.getIf<sf::Event::MouseMoved>()) {
                if (dragging) {
                    sf::Vector2i curPixel = mouseMoved->position;
                    sf::Vector2f prevWorld = mapWin.mapPixelToCoords(prevDragPixel);
                    sf::Vector2f curWorld  = mapWin.mapPixelToCoords(curPixel);
                    sf::Vector2f diff = prevWorld - curWorld;
                    view.setCenter(view.getCenter() + diff);
                    mapWin.setView(view);
                    prevDragPixel = curPixel;
                }
            }

            evOpt = mapWin.pollEvent();
        }

        if (!mapWin.isOpen()) break;

        mapWin.clear(sf::Color::Black);
        for (const auto& s : tmjMap->getTiles()) mapWin.draw(s);
        for (const auto& a : tmjMap->getEntranceAreas()) {
            sf::RectangleShape rect(sf::Vector2f(a.width, a.height));
            rect.setPosition(sf::Vector2f(a.x, a.y));
            rect.setFillColor(sf::Color(0, 100, 255, 120));
            rect.setOutlineThickness(0);
            mapWin.draw(rect);
        }
        if (tr.isFontLoaded()) {
            tr.renderTextObjects(tmjMap->getTextObjects(), mapWin);
        }

        mapWin.display();
    }
}

// show the schedule image in a blocking modal window
static void showScheduleModal(Renderer& renderer, const ConfigManager& configManager) {
    auto dm = sf::VideoMode::getDesktopMode();
    sf::RenderWindow schedWin(dm, sf::String("Schedule"), sf::State::Windowed);
    schedWin.setFramerateLimit(60);

    sf::Texture schedTex;
    if (!schedTex.loadFromFile(std::string("config/quiz/course_schedule.png"))) {
        Logger::error("Failed to load config/quiz/course_schedule.png");
        return;
    }
    sf::Sprite schedSprite(schedTex);

    // Scale to fit desktop while preserving aspect ratio
    float winW = static_cast<float>(dm.size.x);
    float winH = static_cast<float>(dm.size.y);
    float texW = static_cast<float>(schedTex.getSize().x);
    float texH = static_cast<float>(schedTex.getSize().y);
    float scale = 1.f;
    if (texW > 0 && texH > 0) scale = std::min(winW / texW, winH / texH);
    schedSprite.setScale(sf::Vector2f(scale, scale));
    // Center sprite
    float displayW = texW * scale;
    float displayH = texH * scale;
    schedSprite.setPosition(sf::Vector2f((winW - displayW) * 0.5f, (winH - displayH) * 0.5f));

    while (schedWin.isOpen()) {
        std::optional<sf::Event> evOpt = schedWin.pollEvent();
        while (evOpt.has_value()) {
            sf::Event& ev = evOpt.value();
            if (auto closed = ev.getIf<sf::Event::Closed>()) {
                schedWin.close();
                break;
            }
            if (auto key = ev.getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    schedWin.close();
                    break;
                }
            }
            if (auto mouse = ev.getIf<sf::Event::MouseButtonPressed>()) {
                schedWin.close();
                break;
            }
            evOpt = schedWin.pollEvent();
        }
        if (!schedWin.isOpen()) break;
        schedWin.clear(sf::Color::Black);
        schedWin.draw(schedSprite);
        schedWin.display();
    }
}

// Attempt to load target map from entrance; returns true on success and updates tmjMap & character & renderer view.
static bool tryEnterTarget(
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    const EntranceArea& entrance,
    Character& character,
    Renderer& renderer,
    ConfigManager& configManager
) {
    namespace fs = std::filesystem;
    fs::path targetPath(entrance.target);
    fs::path resolved = targetPath.is_absolute() ? targetPath : fs::path(mapLoader.getMapDirectory()) / targetPath;
    std::string resolvedStr = resolved.generic_string();
    auto newMap = mapLoader.loadTMJMap(resolvedStr);
    if (!newMap) {
        Logger::error("Failed to load target map: " + resolvedStr);
        return false;
    }
    tmjMap = newMap;
    Logger::info("Entering target map: " + resolvedStr + " via entrance target='" + entrance.target + "'");
    if (entrance.targetX && entrance.targetY) {
        Logger::info("  entrance provides targetX/Y = " + std::to_string(*entrance.targetX) + ", " + std::to_string(*entrance.targetY));
    } else {
        Logger::info("  entrance has no explicit targetX/Y");
    }
    sf::Vector2f spawnPos;
    if (entrance.targetX && entrance.targetY) {
        spawnPos = sf::Vector2f(*entrance.targetX, *entrance.targetY);
    } else {
        auto ov = mapLoader.getSpawnOverride(resolvedStr);
        if (ov) {
            Logger::info("  using spawn override for map: " + resolvedStr + " -> " + std::to_string(ov->x) + ", " + std::to_string(ov->y));
        } else if (newMap->getSpawnX() && newMap->getSpawnY()) {
            Logger::info("  using map's default spawn: " + std::to_string(*newMap->getSpawnX()) + ", " + std::to_string(*newMap->getSpawnY()));
        } else {
            Logger::info("  no spawn found, will use map center");
        }
        spawnPos = mapLoader.resolveSpawnForMap(resolvedStr, *newMap);
    }
    character.setPosition(spawnPos);
    renderer.updateCamera(spawnPos, tmjMap->getWorldPixelWidth(), tmjMap->getWorldPixelHeight());
    return true;
}

static void cancelEntranceMove(Character& character, const TMJMap& map) {
    int tileW = map.getTileWidth();
    int tileH = map.getTileHeight();
    sf::Vector2f pos = character.getPosition();
    switch (character.getCurrentDirection()) {
        case Character::Direction::Up:    pos.y += static_cast<float>(tileH); break;
        case Character::Direction::Down:  pos.y -= static_cast<float>(tileH); break;
        case Character::Direction::Left:  pos.x += static_cast<float>(tileW); break;
        case Character::Direction::Right: pos.x -= static_cast<float>(tileW); break;
    }
    character.setPosition(pos);
}

// detect interactions
static bool detectInteraction(const Character& character, const TMJMap* map, InteractionObject& outObj) {
    if (!map) {
        Logger::debug("detectInteraction: map is null");
        return false;
    }
    sf::Vector2f feet = character.getFeetPoint();
    Logger::debug("detectInteraction: character feet at (" + std::to_string(feet.x) + "," + std::to_string(feet.y) + ")");
    
    const auto& interactionObjs = map->getInteractionObjects(); 
    Logger::debug("detectInteraction: " + std::to_string(interactionObjs.size()) + " interaction objects total");
    
    for (const auto& obj : interactionObjs) {
        if (obj.type != "counter") continue; // detect counter
        Logger::debug("detectInteraction: checking Counter '" + obj.name + "' rect (" + 
                     std::to_string(obj.rect.position.x) + "," + std::to_string(obj.rect.position.y) + 
                     ") size (" + std::to_string(obj.rect.size.x) + "," + std::to_string(obj.rect.size.y) + ")");
        
        if (obj.rect.contains(feet)) {
            sf::Vector2f center(
                obj.rect.position.x + obj.rect.size.x / 2.0f,
                obj.rect.position.y + obj.rect.size.y / 2.0f
            );
            sf::Vector2f dir = center - feet;
            Character::Direction desired = (std::abs(dir.x) > std::abs(dir.y)) 
                ? (dir.x > 0 ? Character::Direction::Right : Character::Direction::Left)
                : (dir.y > 0 ? Character::Direction::Down : Character::Direction::Up);
            
            Logger::debug("detectInteraction: Counter contains feet, desired direction: " + std::to_string(static_cast<int>(desired)) + 
                         ", character direction: " + std::to_string(static_cast<int>(character.getCurrentDirection())));
            
            if (desired == character.getCurrentDirection()) {
                outObj = obj;
                Logger::debug("detectInteraction: success - matched Counter '" + obj.name + "'");
                return true;
            }
        }
    }
    Logger::debug("detectInteraction: no matching Counter found");
    return false;
}

// detect interactions with tables
static bool detectTableInteraction(const Character& character, const TMJMap* map, TableObject& outTable) {
    if (!map) {
        Logger::error("detectTableInteraction: map is null");
        return false;
    }
    sf::Vector2f feet = character.getFeetPoint();
    Logger::info("detectTableInteraction: feet coordinates = (" + std::to_string(feet.x) + "," + std::to_string(feet.y) + ")");
    
    const auto& tables = map->getTables();
    Logger::info("detectTableInteraction: Total tables in map: " + std::to_string(tables.size()));
    if (tables.empty()) {
        Logger::warn("detectTableInteraction: No tables found in map");
        return false;
    }

    for (const auto& table : tables) {
        // 5 pixels tolerance
        sf::FloatRect tolerantRect = table.rect;
        tolerantRect.position.x -= 5;  // 替代 left
        tolerantRect.position.y -= 5;  // 替代 top
        tolerantRect.size.x += 10;     // 替代 width
        tolerantRect.size.y += 10;     // 替代 height

        Logger::debug("detectTableInteraction: detact table → name: " + table.name + 
                     " | original rect: (" + std::to_string(table.rect.position.x) + "," + std::to_string(table.rect.position.y) + 
                     ") | tolerant rect: (" + std::to_string(tolerantRect.position.x) + "," + std::to_string(tolerantRect.position.y) + ")");

        //Check whether the foot is within the tolerance range
        if (tolerantRect.contains(feet)) {
            outTable = table;
            Logger::info("detectTableInteraction: matched → table name: " + table.name + 
                         " | seatPosition: (" + std::to_string(table.seatPosition.x) + "," + std::to_string(table.seatPosition.y) + ")");
            return true;
        }
    }

    Logger::warn("detectTableInteraction: the character is not on any table");
    return false;
}

// load food textures
static std::unordered_map<std::string, sf::Texture> loadFoodTextures() {
    std::unordered_map<std::string, sf::Texture> textures;
    sf::Texture tex;
    
    if (tex.loadFromFile("textures/chicken_steak.png")) {
        textures["Chicken Steak"] = tex;
        Logger::info("Loaded food texture: Chicken Steak");
    } else {
        Logger::warn("Failed to load texture: textures/chicken_steak.png");
    }
    
    if (tex.loadFromFile("textures/pasta.png")) {
        textures["Pasta"] = tex;
        Logger::info("Loaded food texture: Pasta");
    } else {
        Logger::warn("Failed to load texture: textures/pasta.png");
    }
    
    if (tex.loadFromFile("textures/beef_noodles.png")) {
        textures["Beef Noodles"] = tex;
        Logger::info("Loaded food texture: Beef Noodles");
    } else {
        Logger::warn("Failed to load texture: textures/beef_noodles.png");
    }
    
    return textures;
}

// Scan → Key trasformation
static sf::Keyboard::Key scanToKey(sf::Keyboard::Scan scanCode) {
    switch (scanCode) {
        case sf::Keyboard::Scan::E:      return sf::Keyboard::Key::E;
        case sf::Keyboard::Scan::Enter:  return sf::Keyboard::Key::Enter;
        case sf::Keyboard::Scan::Escape: return sf::Keyboard::Key::Escape;
        default: return sf::Keyboard::Key::Unknown;
    }
}

// detect whether the character is in the lawn
static bool isCharacterInLawn(const Character& character, const TMJMap* map) {
    if (!map) return false;
    sf::Vector2f feet = character.getFeetPoint();
    for (const auto& lawn : map->getLawnAreas()) {
        if (lawn.rect.contains(feet)) {
            return true;
        }
    }
    return false;
}

struct EntranceZone {
    sf::FloatRect rect;      //  Axis-Aligned rectangle
    std::string   building;  //  building property
};

// map the values of TimeManager::getWeekday() into strings
static std::string weekdayStringFrom(TimeManager& tm) {
    static const char* wk[] = {"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};
    int w = tm.getWeekday();
    if (w < 0 || w > 6) return "Monday";
    return wk[w];
}

// util: basename without extension 
static std::string basenameNoExt(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    std::string name = (p == std::string::npos) ? path : path.substr(p + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

// ------------------------------------------------------------
// search for the entrance layer of the current map
// rectangle only
// Each object read by outZones must contain a "building" attribute of type string
// ------------------------------------------------------------
static bool reloadEntranceZonesForMap(const std::string& tmjPath,
                                      const std::string& /*layerName_ignored*/,
                                      std::vector<EntranceZone>& outZones)
{
    using nlohmann::json;

    outZones.clear();

    std::ifstream ifs(tmjPath);
    if (!ifs.is_open()) {
        std::cout << "[Entrance] ERROR: cannot open tmj: " << tmjPath << "\n";
        return false;
    }

    json j;
    try { ifs >> j; }
    catch (const std::exception& e) {
        std::cout << "[Entrance] ERROR: json parse fail: " << e.what() << "\n";
        return false;
    }

    if (!j.contains("layers") || !j["layers"].is_array()) {
        std::cout << "[Entrance] ERROR: no 'layers' array in: " << tmjPath << "\n";
        return false;
    }

    bool foundLayer = false;

    for (const auto& L : j["layers"]) {
        if (!L.is_object()) continue;
        if (L.value("type", std::string{}) != "objectgroup") continue;

        const std::string lname = L.value("name", std::string{});
        if (lname != "entrance") continue;
        foundLayer = true;

        if (!L.contains("objects") || !L["objects"].is_array()) {
            std::cout << "[Entrance] WARNING: layer 'entrance' has no objects\n";
            continue;
        }

        for (const auto& obj : L["objects"]) {
            float x = obj.value("x", 0.0f);
            float y = obj.value("y", 0.0f);
            float w = obj.value("width", 0.0f);
            float h = obj.value("height", 0.0f);

            // polygon -> enclosing box
            if ((w == 0.f || h == 0.f) && obj.contains("polygon") && obj["polygon"].is_array()) {
                float minx = x, miny = y, maxx = x, maxy = y;
                for (const auto& p : obj["polygon"]) {
                    const float px = x + p.value("x", 0.0f);
                    const float py = y + p.value("y", 0.0f);
                    minx = std::min(minx, px); maxx = std::max(maxx, px);
                    miny = std::min(miny, py); maxy = std::max(maxy, py);
                }
                x = minx; y = miny;
                w = std::max(1.f, maxx - minx);
                h = std::max(1.f, maxy - miny);
            }

            // read the properties of building
            std::string building;
            if (obj.contains("properties") && obj["properties"].is_array()) {
                for (const auto& prop : obj["properties"]) {
                    if (prop.value("name", std::string{}) == "building") {
                        if (prop.contains("value") && prop["value"].is_string())
                            building = prop["value"].get<std::string>();
                        else if (prop.contains("string") && prop["string"].is_string())
                            building = prop["string"].get<std::string>();
                        break;
                    }
                }
            }
            if (building.empty()) {
                // if no building, skip
                continue;
            }

            EntranceZone ez;
            ez.rect = sf::FloatRect(sf::Vector2f{x, y}, sf::Vector2f{w, h}); // SFML3: position/size
            ez.building = building;
            outZones.push_back(std::move(ez));
        }
        // only read entrance layer once
        break;
    }

    if (!foundLayer) {
        std::cout << "[Entrance] WARNING: no 'entrance' layer in: " << tmjPath << "\n";
    }

    std::cout << "[Entrance] loaded " << outZones.size()
              << " zones from " << tmjPath
              << " (layer='entrance')\n";
    return !outZones.empty();
}

// ------------------------------------------------------------
// Each frame: Update the name of the building that passed through the entrance most recently based on the "footprint point"
// Feet point: character.getFeetPoint()
// Automatically reload the entrance cache when the map is switched
// ------------------------------------------------------------
static void updateEntranceHitByPlayer(const sf::Vector2f& playerFeet,
                                      const std::string& tmjPath,
                                      const std::string& layerName,
                                      std::string& lastEntranceBuilding,
                                      int& lastEntranceMinutes,
                                      std::vector<EntranceZone>& entranceZones,
                                      std::string& cachedEntranceMapPath,
                                      int minutesNow)
{
    if (cachedEntranceMapPath != tmjPath) {
        reloadEntranceZonesForMap(tmjPath, layerName, entranceZones);
        cachedEntranceMapPath = tmjPath;
    }

    bool hit = false;
    for (const auto& z : entranceZones) {
        if (z.rect.contains(playerFeet)) {
            hit = true;
            if (lastEntranceBuilding != z.building) {
                lastEntranceBuilding = z.building;
                lastEntranceMinutes  = minutesNow;
                std::cout << "[Entrance] building set to: " << lastEntranceBuilding
                          << " @ " << minutesNow << "min\n";
            }
            break;
        }
    }

    // If not step on any of the entrance areas, keep the "lastEntranceBuilding" unchanged
    // if (!hit) { lastEntranceBuilding.clear(); }
}




// calculate grades
FinalResult calculateFinalResult(int totalPoints) {
    FinalResult result;
    result.totalPoints = totalPoints;

    // number of stars（1-5）
    if (totalPoints >= 300) {
        result.starCount = 5;
        result.grade = Grade::A;
    } else if (totalPoints >= 225) {
        result.starCount = 4;
        result.grade = Grade::B;
    } else if (totalPoints >= 150) {
        result.starCount = 3;
        result.grade = Grade::C;
    } else if (totalPoints >= 75) {
        result.starCount = 2;
        result.grade = Grade::D;
    } else {
        result.starCount = 1;
        result.grade = Grade::F;
    }
    return result;
}

bool isFinalResultShown = false;     // whether show the result panel

bool showFinalResultScreen(Renderer& renderer, char grade, int starCount, const std::string& resultText) {
    sf::RenderWindow& window = renderer.getWindow();
    sf::Font font;
    if (!font.openFromFile("fonts/arial.ttf")) {
        Logger::error("Failed to load font for final result");
        return true;
    }

    // Background loading, scaling, centering
    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile("textures/dialog_bg.png")) {
        Logger::error("Failed to load dialog_bg.png");
        return true;
    }
    sf::Sprite bgSprite(bgTexture);
    const float PANEL_SCALE_RATIO = 0.7f; 
    sf::Vector2u windowSize = window.getSize();
    sf::Vector2u bgTexSize = bgTexture.getSize();
    float scaleX = (windowSize.x * PANEL_SCALE_RATIO) / bgTexSize.x;
    float scaleY = (windowSize.y * PANEL_SCALE_RATIO) / bgTexSize.y;
    float finalScale = std::min(scaleX, scaleY);
    bgSprite.setScale(sf::Vector2f(finalScale, finalScale));
    sf::FloatRect bgBounds = bgSprite.getGlobalBounds();
    float bgX = (windowSize.x - bgBounds.size.x) / 2.0f;  
    float bgY = (windowSize.y - bgBounds.size.y) / 2.0f; 
    bgSprite.setPosition(sf::Vector2f(bgX, bgY));

    // texts of results
    sf::Text gradeText(font, "", 36);
    std::string article = (grade == 'A') ? "an" : "a";
    gradeText.setString("You got " + article + " " + std::string(1, grade) + " in the game!");
    gradeText.setFillColor(sf::Color::White);
    gradeText.setCharacterSize(36);
    sf::FloatRect gradeBounds = gradeText.getLocalBounds();
    gradeText.setOrigin(sf::Vector2f(gradeBounds.size.x / 2, gradeBounds.size.y / 2));
    gradeText.setPosition(sf::Vector2f(
        windowSize.x / 2.0f,          
        bgY + bgBounds.size.y * 0.25f 
    ));

    // texts of health condition
    sf::Text healthText(font, resultText, 28);
    healthText.setFillColor(sf::Color(255, 215, 0)); // 金色突出
    healthText.setCharacterSize(28);
    sf::FloatRect healthBounds = healthText.getLocalBounds();
    healthText.setOrigin(sf::Vector2f(healthBounds.size.x / 2, healthBounds.size.y / 2));
    healthText.setPosition(sf::Vector2f(
        windowSize.x / 2.0f,
        bgY + bgBounds.size.y * 0.35f
    ));

    // show stars
    const float starSize = 50.f;
    sf::Texture starYTexture, starGTexture;
    if (!starYTexture.loadFromFile("textures/star_y.png") || !starGTexture.loadFromFile("textures/star_g.png")) {
        Logger::error("Failed to load star textures");
        return true;
    }
    std::vector<sf::Sprite> stars;
    float starStartX = (windowSize.x - (starSize * 5 + 20.f * 4)) / 2;
    float starY = bgY + bgBounds.size.y * 0.5f;
    for (int i = 0; i < 5; ++i) {
        sf::Sprite star(i < starCount ? starYTexture : starGTexture);
        star.setScale(sf::Vector2f(
            starSize / starYTexture.getSize().x, 
            starSize / starYTexture.getSize().y
        ));
        star.setPosition(sf::Vector2f(
            starStartX + i * (starSize + 20.f), 
            starY
        ));
        stars.push_back(star);
    }

    // Button layout, interaction
    const float btnWidth = 180.f;
    const float btnHeight = 60.f;
    sf::RectangleShape exitBtn(sf::Vector2f(btnWidth, btnHeight));
    exitBtn.setFillColor(sf::Color(139, 69, 19));
    exitBtn.setOutlineColor(sf::Color(80, 40, 10));
    exitBtn.setOutlineThickness(2.f);
    float btnX = (windowSize.x - btnWidth) / 2;
    float btnY = bgY + bgBounds.size.y * 0.7f;
    exitBtn.setPosition(sf::Vector2f(btnX, btnY));

    sf::Text exitText(font, "Exit", 24);
    exitText.setFillColor(sf::Color::White);
    sf::FloatRect exitTextBounds = exitText.getLocalBounds();
    exitText.setOrigin(sf::Vector2f(exitTextBounds.size.x / 2, exitTextBounds.size.y / 2));
    exitText.setPosition(sf::Vector2f(
        exitBtn.getPosition().x + btnWidth / 2,
        exitBtn.getPosition().y + btnHeight / 2
    ));

    // Event polling
    sf::View originalView = window.getView();
    window.setView(window.getDefaultView());
    bool shouldExit = false;
    bool isRunning = true;

    while (window.isOpen() && isRunning) {
        std::optional<sf::Event> event;
        while ((event = window.pollEvent()).has_value()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                isRunning = false;
                shouldExit = true;
            }
            if (const auto* mouseEvent = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouseEvent->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mousePos = window.mapPixelToCoords(
                        sf::Vector2i(mouseEvent->position.x, mouseEvent->position.y)
                    );
                    if (exitBtn.getGlobalBounds().contains(mousePos)) {
                        shouldExit = true;
                        isRunning = false;
                    }
                }
            }
        }

        // Mouse hover effect
        sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
        sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);
        if (exitBtn.getGlobalBounds().contains(mouseWorldPos)) {
            exitBtn.setFillColor(sf::Color(150, 80, 30));
        } else {
            exitBtn.setFillColor(sf::Color(139, 69, 19));
        }

        // render
        window.clear(sf::Color(40, 40, 40));
        window.draw(bgSprite);
        window.draw(gradeText);
        window.draw(healthText); // 绘制主循环传的健康文本
        for (const auto& star : stars) window.draw(star);
        window.draw(exitBtn);
        window.draw(exitText);
        window.display();
    }

    window.setView(originalView);
    return shouldExit;
}

struct ProfessorResponseState {
    bool pending = false;
    std::string professorName;
    std::string professorCourse;
    std::string dialogType;
    int selectedOption = -1;
    std::string selectedText;
};

// Struct to store clickable task areas for the Event Loop
struct TaskHitbox {
    sf::FloatRect bounds;
    std::string detailText;
};

struct SettlementData {
    char grade;
    int finalStarCount;
    std::string resultText;
};

SettlementData calculateSettlementData(long long points, int faintCount) {
    SettlementData data;
    int baseStarCount = 1;

    // Calculate the rating and stars
    if (points >= 450) {
        data.grade = 'A';
        baseStarCount = 5;
    } else if (points >= 350) {
        data.grade = 'B';
        baseStarCount = 4;
    } else if (points >= 250) {
        data.grade = 'C';
        baseStarCount = 3;
    } else if (points >= 150) {
        data.grade = 'D';
        baseStarCount = 2;
    } else {
        data.grade = 'F';
        baseStarCount = 1;
    }

    // calculate the health condition score
    std::string healthCondition;
    if (faintCount <= 1) {
        healthCondition = "good";
    } else if (faintCount == 2) {
        healthCondition = "medium";
    } else {
        healthCondition = "bad";
    }

    // calculate the final number of stars
    data.finalStarCount = std::max(baseStarCount - faintCount, 0);

    // result texts
    std::string article = (data.grade == 'A') ? "an" : "a";
    data.resultText = "You are " + article + " " + std::string(1, data.grade) + 
                      " student with " + healthCondition + " health condition!";

    return data;
}


AppResult runApp(
    Renderer& renderer,
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    Character& character,
    sf::View& view,
    ConfigManager& configManager
) {

    int currentDay = 1;               // initialize the date
    bool isFinalResultShown = false;  // whether showed the final result
    auto& inputManager = InputManager::getInstance();
    //Initialize time & tasks
    TimeManager timeManager;
    TaskManager taskManager;

    AppResult result = AppResult::QuitGame;   // Default: quit game

    // === Lesson trigger system ===
    LessonTrigger lessonTrigger;

    std::string lastEntranceBuilding;   // The name of the building at the most recent entrance that was passed through
    int         lastEntranceMinutes = -1; // Record timestamp

    std::vector<EntranceZone> entranceZones;
    std::string cachedEntranceMapPath;    // path of the map that has cached entrances

    // read the coure schedule
    if (!lessonTrigger.loadSchedule("config/quiz/course_schedule.json")) {
        Logger::error("[LessonTrigger] failed to load course_schedule.json");
    }

    // Load initial tasks
    // Params: id, description, detailed instruction, achievement name, points, energy

    taskManager.addTask("eat_food", 
        "Eat Food at Canteen", 
        "Go to the Student Centre and press E at the counter to order food, then sit at a table and press E to eat. This restores energy.", 
        "Foodie", 
        0, 0); 
    
    taskManager.addTask("attend_class", 
        "Attend Class (Quiz)", 
        "Find a classroom. Enter the trigger zone to start the class quiz. This awards points but deducts your energy.", 
        "Scholar", 
        20, 0);
    
    taskManager.addTask("rest_lawn", 
        "Rest on Lawn", 
        "Walk onto the green lawn before the library. Press E to rest and recover energy.", 
        "Nature Lover", 
        0, 0); 
    
    taskManager.addTask("buy_item", 
        "Buy Item at FamilyMart", 
        "Locate the FamilyMart shop. Press E at the entrance to buy items. This gives points.", 
        "Big Spender", 
        10, 0); 
    
    taskManager.addTask("talk_professor", 
        "Talk to a Professor", 
        "Find a professor on the map. Press E to start a conversation. Awards points.", 
        "Networker", 
        15, 0); 

    taskManager.addTask("bookstore_quiz", 
        "Solve Bookstore Puzzle", 
        "Go to the Bookstore. Enter the trigger area to solve the CUHK(SZ) questions. This gives lots of points.", 
        "Bookworm", 
        25, 0);

    if (!renderer.initializeChefTexture()) {
        Logger::error("Failed to initialize chef texture");
        return AppResult::QuitGame;
    }
    
    // initialize the professor textures
    if (!renderer.initializeProfessorTexture()) {
        Logger::error("Failed to initialize professor texture");
        return AppResult::QuitGame;
    }
    
    // Load the modal font
    sf::Font modalFont;
    if (!modalFont.openFromFile(configManager.getRenderConfig().text.fontPath)) {
        Logger::error("Failed to load modal font!");
        return AppResult::QuitGame;
    }
    
    // initialize the dialogue box
    DialogSystem dialogSys(modalFont, 24);
    bool dialogInitSuccess = false;
    try {
        std::string dialogBgPath ="textures/dialog/dialog_bg.png";
        std::string btnPath ="textures/dialog/btn.png";
        
        dialogSys.initialize(
            dialogBgPath,
            btnPath,
            modalFont,
            configManager.getRenderConfig().text.fontSize
        );
        dialogInitSuccess = true;
        Logger::info("Dialog system initialized successfully");
    } catch (const std::runtime_error& e) {
        Logger::error(std::string("Failed to init dialog system: ") + e.what());
        dialogInitSuccess = false;
    }

    // load the texture of food
    auto foodTextures = loadFoodTextures();
    // game state
    struct GameState {
        bool isEating = false;
        bool hasOrderedFood = false; 
        std::string currentTable;
        std::string selectedFood;
        float eatingProgress = 0.0f;
    };
    GameState gameState;

    struct ShoppingState {
    bool isShopping = false;

    std::string selectedCategory;
    std::string selectedItem;

    float shoppingProgress = 0.0f;

    // a state that control what dialogue to play next
    bool requestNextDialog = false;

    std::string nextDialogTitle;
    std::vector<std::string> nextDialogOptions;

    enum class NextDialogKind {
        None,
        ShowFirstLevel,    // show FamilyMart fist level
        ShowSecondLevel,  // show the items under each level
        ConfirmPurchase   // show the confirm box of purchasing
    };

    NextDialogKind nextDialogKind = NextDialogKind::None;
};
    ShoppingState shoppingState;

    static ProfessorResponseState profResponseState;

    // Fainting State
    bool isFainted = false;
    float faintTimer = 0.0f;
    bool isBlackScreen = false;  // black screen state
    float blackScreenTimer = 0.0f;  
    int faintCount = 0;  // times of faint
    bool showFaintReminder = false;  // whether show the faint reminder
    float faintReminderTimer = 0.0f;  // close the reminder after 5 seconds
    bool isExpelled = false;  // whether dropping out

    // confirm entrances states
    bool waitingForEntranceConfirmation = false;
    EntranceArea pendingEntrance;
    bool hasSuppressedEntrance = false;
    sf::FloatRect suppressedEntranceRect;

    // Vector to store hitboxes of tasks drawn in the previous frame
    std::vector<TaskHitbox> activeTaskHitboxes;

    // Unstuck State 
    sf::Vector2f lastFramePos = character.getPosition();
    float stuckTimer = 0.0f;

    // main loop
    sf::Clock clock;
    while (renderer.isRunning()) {
        // Execute the dialog callback safely when the new frame begins
        if (dialogSys.hasPendingCallback()) {
            Logger::info("Executing pending dialog callback...");
            auto cb = dialogSys.consumePendingCallback();
            cb();
            Logger::info("Dialog callback executed");
            // Close the dialog box after the callback is executed
            dialogSys.close();
            renderer.setModalActive(false);
        }

        float deltaTime = clock.restart().asSeconds();
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        timeManager.update(deltaTime);

        // Achievement Timer
        if (g_achievementTimer > 0.0f) {
            g_achievementTimer -= deltaTime;
        }

        // Decrement per frame Hint timer
        if (g_hintTimer > 0.f) {
            g_hintTimer -= deltaTime;
            if (g_hintTimer < 0.f) g_hintTimer = 0.f;
        }

        const float PASSIVE_DEPLETION_RATE = 10.0f / 30.0f;
        taskManager.modifyEnergy(-PASSIVE_DEPLETION_RATE * deltaTime);

        // close the reminder after 5 seconds
        if (showFaintReminder) {
            faintReminderTimer += deltaTime;
            if (faintReminderTimer >= 5.0f) {
                showFaintReminder = false;
                faintReminderTimer = 0.0f;
                Logger::info("Faint reminder auto-closed after 5 seconds");
            }
        }

    
        // deal with the professor's respond
        if (profResponseState.pending && !dialogSys.isActive()) {
            Logger::info("Processing professor response - pending: true, option: " + 
                std::to_string(profResponseState.selectedOption));
            std::string response;
            std::string profName = profResponseState.professorName;
            std::string profCourse = profResponseState.professorCourse;
            std::string profDialogType = profResponseState.dialogType;
            int optionIndex = profResponseState.selectedOption;
            Logger::info("Professor info: " + profName + ", course: " + profCourse + 
                ", dialogType: " + profDialogType);
            switch (optionIndex) {
                case 0:
                    if (profDialogType == "lecture") {
                        response = "I'm teaching " + profCourse + " this semester. It's a fascinating subject!";
                    } else {
                        response = "Studies are going well! Remember to review materials regularly.";
                    }
                    break;
                case 1:
                    if (profDialogType == "lecture") {
                        response = "My office hours are Monday and Wednesday 2-4 PM. Feel free to visit!";
                    } else {
                        response = "My advice: focus on understanding concepts rather than memorizing.";
                    }
                    break;
                case 2:
                    if (profDialogType == "lecture") {
                        response = "Hello! Nice to see you. Don't hesitate to ask questions.";
                    } else {
                        response = "Goodbye! Keep up the good work!";
                    }
                    break;
                default:
                    response = "Thank you for your interest!";
                    break;
            }
            
            Logger::info("Professor " + profName + " responds: " + response);
            
            // Trigger Task Completion & Deduct Energy 
            handleTaskCompletion(taskManager, "talk_professor");
            taskManager.modifyEnergy(-2.0f);            

            // show the response dialog
            dialogSys.setDialog(
                response,
                {"OK"},
                [](const std::string&) {
                    Logger::info("Professor response dialog closed");
                }
            );
            renderer.setModalActive(true);
            
            // 重置状态
            profResponseState.pending = false;
            profResponseState.selectedOption = -1;
            Logger::info("🔄 Professor response state reset");
        }

        // deal with the list of items (second level) in stores
        if (shoppingState.requestNextDialog && !dialogSys.isActive()) {
            Logger::info("requestNextDialog handling | kind = " + std::to_string(static_cast<int>(shoppingState.nextDialogKind)));

            // if presenting the second level list
            if (shoppingState.nextDialogKind == ShoppingState::NextDialogKind::ShowSecondLevel) {
                dialogSys.setDialog(
                    shoppingState.nextDialogTitle,
                    shoppingState.nextDialogOptions,
                    [&shoppingState](const std::string& selected) {
                        Logger::info("second-level callback selected: " + selected);
                        if (selected == "Back") {
                            // request for showing the first level list
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::ShowFirstLevel;
                            shoppingState.nextDialogTitle = "Welcome to FamilyMart! Which section would you like to browse?";
                            shoppingState.nextDialogOptions = {"Food", "Drink", "Daily Necessities", "Cancel"};
                            shoppingState.requestNextDialog = true;
                        } else {
                            // choose the item
                            shoppingState.selectedItem = selected;
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::ConfirmPurchase;
                            shoppingState.nextDialogTitle = "\n\nPrice:15yuan\n\nProceed with purchase?";
                            shoppingState.nextDialogOptions = {"Yes, buy it", "No, go back"};
                            shoppingState.requestNextDialog = true;
                        }
                    }
                );

                // request finished
                shoppingState.requestNextDialog = false;
                renderer.setModalActive(true);
            }
            // if presenting the first level list
            else if (shoppingState.nextDialogKind == ShoppingState::NextDialogKind::ShowFirstLevel) {
                dialogSys.setDialog(
                    shoppingState.nextDialogTitle,
                    shoppingState.nextDialogOptions,
                    [&shoppingState](const std::string& selected) {
                        Logger::info("Category Selected: " + selected);
                        if (selected == "Cancel") {
                            shoppingState.isShopping = false;
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::None;
                            shoppingState.requestNextDialog = false;
                            return;
                        }

                        shoppingState.selectedCategory = selected;
                        // Prepare the secondary list options based on the classification.
                        shoppingState.nextDialogKind = ShoppingState::NextDialogKind::ShowSecondLevel;
                        if (selected == "Food") {
                            shoppingState.nextDialogTitle = "Choose your food:";
                            shoppingState.nextDialogOptions = {"Sandwich", "Bento", "Onigiri", "Back"};
                        } else if (selected == "Drink") {
                            shoppingState.nextDialogTitle = "Choose your drink:";
                            shoppingState.nextDialogOptions = {"Water", "Coffee", "Tea", "Back"};
                        } else if (selected == "Daily Necessities") {
                            shoppingState.nextDialogTitle = "Choose your item:";
                            shoppingState.nextDialogOptions = {"Tissue", "Battery", "Umbrella", "Back"};
                        } else {
                            // Fallback: go back to the first level list
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::ShowFirstLevel;
                            shoppingState.nextDialogTitle = "Welcome to FamilyMart! Which section would you like to browse?";
                            shoppingState.nextDialogOptions = {"Food", "Drink", "Daily Necessities", "Cancel"};
                        }
                        shoppingState.requestNextDialog = true;
                    }
                );

                shoppingState.requestNextDialog = false;
                renderer.setModalActive(true);
            }
            // If it is a display of the purchase confirmation dialog
            else if (shoppingState.nextDialogKind == ShoppingState::NextDialogKind::ConfirmPurchase) {
                dialogSys.setDialog(
                    shoppingState.nextDialogTitle,
                    shoppingState.nextDialogOptions,
                    [&shoppingState, &taskManager](const std::string& choice) {
                        Logger::info("Purchase Choice: " + choice + " for item " + shoppingState.selectedItem);
                        if (choice == "Yes, buy it") {
                            // Implement the purchase
                            Logger::info("Purchased: " + shoppingState.selectedItem);
                            
                            // Trigger Task Completion & Deduct Energy
                            handleTaskCompletion(taskManager, "buy_item");
                            taskManager.modifyEnergy(-5.0f);      

                            shoppingState.isShopping = false;
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::None;
                            shoppingState.requestNextDialog = false;
                        } else {
                            // Return to the secondary item selection (of the same type)
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::ShowSecondLevel;
                            // reconstruct the title/options of second-level
                            if (shoppingState.selectedCategory == "Food") {
                                shoppingState.nextDialogTitle = "Choose your food:";
                                shoppingState.nextDialogOptions = {"Sandwich", "Bento", "Onigiri", "Back"};
                            } else if (shoppingState.selectedCategory == "Drink") {
                                shoppingState.nextDialogTitle = "Choose your drink:";
                                shoppingState.nextDialogOptions = {"Water", "Coffee", "Tea", "Back"};
                            } else if (shoppingState.selectedCategory == "Daily Necessities") {
                                shoppingState.nextDialogTitle = "Choose your item:";
                                shoppingState.nextDialogOptions = {"Tissue", "Battery", "Umbrella", "Back"};
                            } else {
                                // return to the fist level list
                                shoppingState.nextDialogKind = ShoppingState::NextDialogKind::ShowFirstLevel;
                                shoppingState.nextDialogTitle = "Welcome to FamilyMart! Which section would you like to browse?";
                                shoppingState.nextDialogOptions = {"Food", "Drink", "Daily Necessities", "Cancel"};
                            }
                            shoppingState.requestNextDialog = true;
                        }
                    }
                );

                shoppingState.requestNextDialog = false;
                renderer.setModalActive(true);
            }
            else {
                shoppingState.requestNextDialog = false;
                shoppingState.nextDialogKind = ShoppingState::NextDialogKind::None;
            }
        }

        //  Fainting Logic Check 
        // Must not be currently eating/interacting/fainted
        if (!isFainted && !isBlackScreen && !gameState.isEating && !dialogSys.isActive() && !isExpelled) {
            if (taskManager.getEnergy() <= 0) {
                isFainted = true;
                faintTimer = 0.0f;
                isBlackScreen = false;
                blackScreenTimer = 0.0f;
                faintCount++;  // count faint time
                // Force character direction Up (Visual for passing out)
                character.setCurrentDirection(Character::Direction::Up);
                Logger::info("Character passed out due to lack of energy! Faint count: " + std::to_string(faintCount));
                
                // whether faint time excess the maximum time
                const auto& respawnPoint = tmjMap->getRespawnPoint();
                if (faintCount > respawnPoint.maxCount) {
                    isExpelled = true;
                    Logger::error("Character has been expelled due to too many faints!");
                }
            }
        }
        
        // Handle Faint Timer and Black Screen
        if (isFainted) {
            faintTimer += deltaTime;
            
            // After displaying the message for 4 seconds, enter a black screen state
            if (faintTimer > 4.0f && !isBlackScreen) {
                isBlackScreen = true;
                blackScreenTimer = 0.0f;
                Logger::info("Entering black screen...");
            }
            
            // After a black screen for 2 seconds, respawn at the clinic entrance
            if (isBlackScreen) {
                blackScreenTimer += deltaTime;
                
                if (blackScreenTimer >= 2.0f) {
                    // check whether dropped out
                    if (isExpelled) {
                        // show the message of expulsion, exit the game
                        Logger::error("Character expelled! Game over.");
                    } else {
                        // Check if the current map is LG_campus_map. If not, switch to it.
                        std::string currentMapPath = mapLoader.getCurrentMapPath();
                        bool needSwitchMap = false;
                        if (currentMapPath.find("LG_campus_map") == std::string::npos) {
                            needSwitchMap = true;
                            Logger::info("Not in LG_campus_map, switching to LG_campus_map for respawn");
                            
                            // load LG_campus_map
                            std::string campusMapPath = mapLoader.getMapDirectory() + "LG_campus_map.tmj";
                            auto campusMap = mapLoader.loadTMJMap(campusMapPath);
                            if (campusMap) {
                                tmjMap = campusMap;
                                Logger::info("Switched to LG_campus_map successfully");
                            } else {
                                Logger::error("Failed to load LG_campus_map, using current map");
                            }
                        }
                        
                        // Prevent the entrance confirmation dialog from displaying
                        waitingForEntranceConfirmation = false;
                        hasSuppressedEntrance = true;
                        
                        // go back to the respawn place
                        const auto& respawnPoint = tmjMap->getRespawnPoint();
                        sf::Vector2f respawnPos = respawnPoint.position;
                        
                        Logger::info("Respawn point position: (" + std::to_string(respawnPos.x) + ", " + std::to_string(respawnPos.y) + ")");
                        
                        // if the respawn place is invalit, use the default spawn point
                        if (respawnPos.x == 0.0f && respawnPos.y == 0.0f) {
                            Logger::warn("Respawn point is at (0,0), using default spawn point");
                            if (tmjMap->getSpawnX() && tmjMap->getSpawnY()) {
                                respawnPos = sf::Vector2f(*tmjMap->getSpawnX(), *tmjMap->getSpawnY());
                            } else {
                                Logger::error("No valid respawn point or default spawn point available!");
                            }
                        }
                        
                        // Calculate the offset from the foot to the center
                        sf::Vector2f currentFeet = character.getFeetPoint();
                        sf::Vector2f currentCenter = character.getPosition();
                        sf::Vector2f feetToCenterOffset = currentCenter - currentFeet;
                        
                        // Search for walkable positions around the respawn point
                        float tileSize = static_cast<float>(std::max(tmjMap->getTileWidth(), tmjMap->getTileHeight()));
                        float step = tileSize * 0.5f;
                        
                        std::vector<sf::Vector2f> offsets = {
                            sf::Vector2f(0, -step * 2),      // up
                            sf::Vector2f(0, step * 2),        // down
                            sf::Vector2f(-step * 2, 0),      // left
                            sf::Vector2f(step * 2, 0),       // right
                            sf::Vector2f(-step, -step),     // upper-left
                            sf::Vector2f(step, -step),      // upper-right
                            sf::Vector2f(-step, step),       // lower-left
                            sf::Vector2f(step, step),       // lower-right
                            sf::Vector2f(0, -step),          // up (closer)
                            sf::Vector2f(0, step),           // down (closer)
                            sf::Vector2f(-step, 0),          // left (closer)
                            sf::Vector2f(step, 0),           // right (closer)
                        };
                        
                        bool foundWalkable = false;
                        
                        for (const auto& offset : offsets) {
                            sf::Vector2f candidateFeet = respawnPos + offset;
                            
                            if (candidateFeet.x >= 0 && candidateFeet.y >= 0 && 
                                candidateFeet.x < tmjMap->getWorldPixelWidth() && 
                                candidateFeet.y < tmjMap->getWorldPixelHeight()) {
                                
                                if (!tmjMap->feetBlockedAt(candidateFeet)) {
                                    respawnPos = candidateFeet + feetToCenterOffset;
                                    foundWalkable = true;
                                    break;
                                }
                            }
                        }
                        
                        if (!foundWalkable) {
                            // If there is no walkable place found, try using the default spawn point.
                            if (tmjMap->getSpawnX() && tmjMap->getSpawnY()) {
                                respawnPos = sf::Vector2f(*tmjMap->getSpawnX(), *tmjMap->getSpawnY());
                                Logger::warn("Could not find walkable position at respawn point, using default spawn");
                            }
                        }
                        
                        // set character positon
                        character.setPosition(respawnPos);
                        
                        // increase the time by two hours
                        timeManager.addHours(2);
                        
                        // restore the energy to 50
                        taskManager.modifyEnergy(50.0f);
                        
                        // reset status
                        isFainted = false;
                        isBlackScreen = false;
                        faintTimer = 0.0f;
                        blackScreenTimer = 0.0f;
                        
                        // show reminder
                        showFaintReminder = true;
                        faintReminderTimer = 0.0f;
                        
                        // update camera position
                        renderer.updateCamera(respawnPos, tmjMap->getWorldPixelWidth(), tmjMap->getWorldPixelHeight());
                        
                        Logger::info("Character respawned at respawn point (" + 
                                   std::to_string(respawnPos.x) + ", " + std::to_string(respawnPos.y) + 
                                   "). Faint count: " + std::to_string(faintCount));
                    }
                }
            }
        }
        // =================================

        // Unified event handling (polling only once)
        std::optional<sf::Event> eventOpt;
        while ((eventOpt = renderer.pollEvent()).has_value()) {
            sf::Event& event = eventOpt.value();

            // Prioritize handling of dialog box events
            if (dialogSys.isActive()) {
                dialogSys.handleEvent(event, renderer.getWindow());
                continue;
            }

            // Window close event
            if (event.is<sf::Event::Closed>()) {
                result = AppResult::QuitGame;
                renderer.quit();
                break;
            }


            // Full-screen map button
            if (event.is<sf::Event::MouseButtonPressed>()) {
                auto mb = event.getIf<sf::Event::MouseButtonPressed>();
                if (mb && mb->button == sf::Mouse::Button::Left) {
                    sf::Vector2i mpos = mb->position;
                    
                    // check whether clicked the Game Over button
                    if (isExpelled) {
                        sf::Vector2u windowSize = renderer.getWindow().getSize();
                        float uiWidth = static_cast<float>(windowSize.x);
                        float uiHeight = static_cast<float>(windowSize.y);
                        
                        // Game Over button position: Bottom of the center of the screen
                        float btnX = uiWidth / 2.0f - 100.f;
                        float btnY = uiHeight / 2.0f + 40.f;
                        float btnW = 200.f;
                        float btnH = 60.f;
                        
                        // Check whether the mouse click is within the range of the button.
                        if (mpos.x >= static_cast<int>(btnX) && mpos.x <= static_cast<int>(btnX + btnW) &&
                            mpos.y >= static_cast<int>(btnY) && mpos.y <= static_cast<int>(btnY + btnH)) {
                            result = AppResult::QuitGame;
                            renderer.quit();
                            break;
                        }
                    }
                    
                    // Check Schedule Button (to the left of Map)
                    if (renderer.scheduleButtonContainsPoint(mpos)) {
                        showScheduleModal(renderer, configManager);
                    }
                    // Check Map Button
                    else if (renderer.mapButtonContainsPoint(mpos)) {
                        showFullMapModal(renderer, tmjMap, configManager);
                    }
                    //  Check Task Clicks
                    else {
                        sf::Vector2f mouseUiPos(static_cast<float>(mpos.x), static_cast<float>(mpos.y));
                        for (const auto& hit : activeTaskHitboxes) {
                            if (hit.bounds.contains(mouseUiPos)) {
                                Logger::info("Clicked Task. Showing details.");
                                // Show detail dialog using existing system
                                dialogSys.setDialog(
                                    "Task Details",
                                    { hit.detailText, "Close" },
                                    [](const std::string&){}
                                );
                                renderer.setModalActive(true);
                                break; 
                            }
                        }
                    }
                }
            }
        }

        // update the input
        inputManager.update();

        // E key detection
        // === NEW: Block interactions if Fainted ===
        if (!isFainted && !waitingForEntranceConfirmation && !dialogSys.isActive() && inputManager.isKeyJustPressed(sf::Keyboard::Key::E)) {
            Logger::debug("E key pressed - checking for interaction");
            if (!gameState.isEating) {
                // detect counter interaction
                InteractionObject counterObj;
                Professor professor;  
                
                bool foundCounter = detectInteraction(character, tmjMap.get(), counterObj);
                bool foundProfessor = detectProfessorInteraction(character, tmjMap.get(), professor);  // 教授检测（从App.cpp补充）
                
                Logger::debug("   foundCounter: " + std::to_string(foundCounter));
                Logger::debug("   foundProfessor: " + std::to_string(foundProfessor));
                
                if (foundCounter) {
                    Logger::info("Triggering Counter interaction - show food select dialog");
                    if (dialogInitSuccess) {
                        dialogSys.setDialog(
                            "What do you want to eat?",  // texts in the dialog box
                            {"Chicken Steak", "Pasta", "Beef Noodles"}, // food choices
                            [&gameState](const std::string& selected) { 
                                Logger::error("FOOD CALLBACK EXECUTED ");
                                Logger::info("Selected: " + selected);
                                gameState.selectedFood = selected; // Assign to the game state
                                gameState.hasOrderedFood = true; // mark oerdered
                                Logger::info("Selected food from counter: " + selected);
                            }
                        );
                        renderer.setModalActive(true); // Activate mode
                    } else {
                        Logger::error("Dialog system not initialized - cannot show food select dialog");
                        renderer.renderModalPrompt("Dialog system not initialized", modalFont, 24, std::nullopt);
                    }
                    continue; 
                }
                // professor interaction
                else if (foundProfessor) {
                    Logger::info("Triggering Professor interaction - showing dialog");
                    if (dialogInitSuccess) {
                        std::vector<std::string> options;
                        if (professor.dialogType == "lecture") {
                            options = {"Ask about " + professor.course, "Request office hours", "Say hello"};
                        } else {
                            options = {"Talk about studies", "Ask for advice", "Say goodbye"};
                        }
                        
                        std::string greeting = "Hello! I'm " + professor.name + ". How can I help you today?";
                        
                        // Store the professor's information in the response state.
                        profResponseState.professorName = professor.name;
                        profResponseState.professorCourse = professor.course;
                        profResponseState.dialogType = professor.dialogType;
                        
                        dialogSys.setDialogWithIndex(
                            greeting,
                            options,
                            [](int optionIndex, const std::string& optionText) {
                                Logger::info("Player chose option " + std::to_string(optionIndex) + ": " + optionText);
                                
                                // Store the user's selection as the response state
                                profResponseState.selectedOption = optionIndex;
                                profResponseState.selectedText = optionText;
                                profResponseState.pending = true; // label the needs to show responses
                            }
                        );
                        renderer.setModalActive(true);
                    }
                    continue;
                }

                // detect the trigger zone of stores
                ShopTrigger shopTrigger;
                bool foundShop = detectShopTrigger(character, tmjMap.get(), shopTrigger);
                
                Logger::debug("   foundShop: " + std::to_string(foundShop));
                
                if (foundShop) {
                    Logger::info("Triggering Shop interaction - showing FamilyMart menu");
                    
                    // The store menu will only be displayed within the trigger area of the store.
                    DialogSystem* ds = &dialogSys;
                    Renderer* rd = &renderer;
                    auto state = &shoppingState;
                    
                    ds->setDialog(
                        "Welcome to FamilyMart! Which section would you like to browse?",
                        {"Food", "Drink", "Daily Necessities", "Cancel"},
                        [ds, rd, state](const std::string& selected) {
                            Logger::info("Category Selected: " + selected);
                            
                            if (selected == "Cancel") {
                                state->isShopping = false;
                                return;
                            }
                            
                            // secord the first level category
                            state->selectedCategory = selected;
                            
                            // Set the request for the next conversation step
                            state->requestNextDialog = true;
                            state->nextDialogKind = ShoppingState::NextDialogKind::ShowSecondLevel;
                            state->nextDialogTitle.clear();
                            state->nextDialogOptions.clear();
                            
                            if (selected == "Food") {
                                state->nextDialogTitle = "Choose your food:";
                                state->nextDialogOptions = {"Sandwich", "Bento", "Onigiri", "Back"};
                            }
                            else if (selected == "Drink") {
                                state->nextDialogTitle = "Choose your drink:";
                                state->nextDialogOptions = {"Water", "Coffee", "Tea", "Back"};
                            }
                            else if (selected == "Daily Necessities") {
                                state->nextDialogTitle = "Choose your item:";
                                state->nextDialogOptions = {"Tissue", "Battery", "Umbrella", "Back"};
                            }
                            
                            // handle this request in the main loop
                            rd->setModalActive(true);
                        }
                    );
                    rd->setModalActive(true);
                    continue; // skip table detection
                }

                // Execute any pending callbacks before conducting the table detection
                if (dialogSys.hasPendingCallback()) {
                    Logger::info("Executing pending dialog callback before table check");
                    auto cb = dialogSys.consumePendingCallback();
                    if (cb) {
                        cb();
                        Logger::info("Dialog callback executed - food should be selected now");
                    }
                }
                
                // verify food selection status
                Logger::info("Food selection status before table check: " + 
                            (gameState.selectedFood.empty() ? "[EMPTY]" : gameState.selectedFood));
        

                // detect table interaction
                TableObject currentTable;
                if (detectTableInteraction(character, tmjMap.get(), currentTable)) {
                    Logger::info("table interaction detected → selected food: " + (gameState.selectedFood.empty() ? "空" : gameState.selectedFood));
                    
                    if (!gameState.hasOrderedFood) {
                        Logger::info("Didn't select food");
                        renderer.renderModalPrompt("Please order food first!", modalFont, 24, std::nullopt);
                    } else {
                        // Verify the validity of seatPosition
                        if (currentTable.seatPosition.x == 0 && currentTable.seatPosition.y == 0) {
                            Logger::error("table " + currentTable.name + " has no valid seatPosition");
                            renderer.renderModalPrompt("No valid seatPosition!", modalFont, 24, std::nullopt);
                            continue;
                        }

                        // read the name of each table and set the orientation of them
                        Character::Direction facingDir;
                        bool isLeftTable = currentTable.name.find("left_table") != std::string::npos;
                        bool isRightTable = currentTable.name.find("right_table") != std::string::npos;

                        if (isLeftTable) {
                            facingDir  = Character::Direction::Right; // tables facing right
                        } else if (isRightTable) {
                            facingDir  = Character::Direction::Left;  // tables facing left
                        } else {
                            facingDir  = Character::Direction::Down;  // default orientation
                        }

                        // Move the character to the insertion point of the chair + Force the orientation setting
                        character.setPosition(currentTable.seatPosition);
                        character.setCurrentDirection(facingDir); 
                        Logger::info("Character has been moved to the seatPosition:(" + std::to_string(currentTable.seatPosition.x) + "," + std::to_string(currentTable.seatPosition.y) + 
                                    ") | direction: " + (isLeftTable ? "right" : "left"));

                        // Activate the feeding state
                        gameState.isEating = true;
                        gameState.currentTable = currentTable.name;
                        gameState.eatingProgress = 0.0f;
                        Logger::info("starts eating → table: " + currentTable.name + " | food: " + gameState.selectedFood);
                        gameState.hasOrderedFood = false;
                    }
                    continue;
                }
                // trigger lawn resting
                if (isCharacterInLawn(character, tmjMap.get()) && !character.getIsResting()) {
                    character.startResting(); // start resting
                    // Force the character's orientation to be set as "down"
                    character.setCurrentDirection(Character::Direction::Down);
                    Logger::info("Character started resting on lawn (facing down)");
                    // Complete Lawn Task
                    handleTaskCompletion(taskManager, "rest_lawn");
                }
            }
        }

        // game trigger detection
        static bool  gameTriggerLocked = false;
        static float gameTriggerCooldown = 0.0f;
        static sf::FloatRect activeTriggerRect;   // the rect we’re currently inside

        // Reduce the cooling time for each frame
        if (gameTriggerCooldown > 0.f) gameTriggerCooldown -= deltaTime;

        // Game trigger detection with cooldown & leave-to-unlock 
        GameTriggerArea detectedTrigger;
        bool insideAnyTrigger = false;

        if (!isFainted && detectGameTrigger(character, tmjMap.get(), detectedTrigger)) {
            insideAnyTrigger = true;

            sf::FloatRect thisRect(
                sf::Vector2f(detectedTrigger.x, detectedTrigger.y),
                sf::Vector2f(detectedTrigger.width, detectedTrigger.height)
            );

            if (!gameTriggerLocked && gameTriggerCooldown <= 0.f) {
                gameTriggerLocked   = true;
                activeTriggerRect   = thisRect;
                gameTriggerCooldown = 0.6f;

                Logger::info("Game Triggered: " + detectedTrigger.name + " | type = " + detectedTrigger.gameType);

                if (detectedTrigger.gameType == "bookstore_puzzle") {
                    QuizGame quizGame;
                    quizGame.run();
                    handleTaskCompletion(taskManager, "bookstore_quiz");

                } else if (detectedTrigger.gameType == "classroom_quiz") {
                    // Use "Monday/Tuesday/..." uniformly
                    const std::string weekday = weekdayStringFrom(timeManager);
                    const int minutesNow = timeManager.getHour() * 60 + timeManager.getMinute();

                    std::string quizJsonPath = "config/quiz/classroom_basic.json";

                    Logger::info("[Classroom] weekday=" + weekday +
                                " minutes=" + std::to_string(minutesNow) +
                                " building(lastEntrance)=" + lastEntranceBuilding);

                    // Present the prompt text and render
                    std::string hint;
                    auto r = lessonTrigger.tryTrigger(
                        weekday,
                        lastEntranceBuilding,   // detect the building name using entrance
                        minutesNow,
                        quizJsonPath,
                        taskManager,
                        &hint                   // let LessonTrigger provide specific reasons
                    );

                    Logger::info(std::string("[Classroom] tryTrigger result=") +
                                (r == LessonTrigger::Result::TriggeredQuiz ? "TriggeredQuiz" :
                                r == LessonTrigger::Result::WrongBuildingHintShown ? "WrongBuildingHintShown" :
                                r == LessonTrigger::Result::AlreadyFired ? "AlreadyFired" : "NoTrigger") +
                                (hint.empty() ? "" : (" | hint=" + hint)));

                    // if quiz not available, show the hint
                    if (r != LessonTrigger::Result::TriggeredQuiz) {
                        if (!hint.empty()) {
                            queueHint(hint, 3.0f);  // show for 3s
                        } else {
                            queueHint("No class quiz available now.", 2.5f);
                        }
                    }
                }
            }
        }

        // Unlock only when leaving the trigger area to avoid triggering every frame.
        if (gameTriggerLocked) {
            sf::Vector2f feet = character.getFeetPoint();
            if (!activeTriggerRect.contains(feet)) {
                gameTriggerLocked = false;
            }
        }


        // Automatic detection of the store's triggering area
        static bool shopTriggerLocked = false;   // prevent from triggering 60 time each frame


        ShopTrigger detectedShop;
        if (!isFainted && detectShopTrigger(character, tmjMap.get(), detectedShop)) {
            if (!shopTriggerLocked && !shoppingState.isShopping && !dialogSys.isActive()) {
                shopTriggerLocked = true; // lock

                std::cout << "Shop Triggered: " << detectedShop.name << std::endl;

                // Automatically display the FamilyMart dialog box
                if (detectedShop.name == "familymart") {
                    Logger::info("Auto-triggering FamilyMart dialog");
                    
                    DialogSystem* ds = &dialogSys;
                    auto state = &shoppingState;
                    
                    state->isShopping = true;
                    ds->setDialog(
                        "Welcome to FamilyMart! Which section would you like to browse?",
                        {"Food", "Drink", "Daily Necessities", "Cancel"},
                        [ds, state](const std::string& selected) {
                            Logger::info("Category Selected: " + selected);
                            
                            if (selected == "Cancel") {
                                state->isShopping = false;
                                return;
                            }
                            
                            // record the first level category
                            state->selectedCategory = selected;
                            
                            // Set the request for the next conversation
                            state->requestNextDialog = true;
                            state->nextDialogKind = ShoppingState::NextDialogKind::ShowSecondLevel;
                            state->nextDialogTitle.clear();
                            state->nextDialogOptions.clear();
                            
                            if (selected == "Food") {
                                state->nextDialogTitle = "Choose your food:";
                                state->nextDialogOptions = {"Sandwich", "Bento", "Onigiri", "Back"};
                            }
                            else if (selected == "Drink") {
                                state->nextDialogTitle = "Choose your drink:";
                                state->nextDialogOptions = {"Water", "Coffee", "Tea", "Back"};
                            }
                            else if (selected == "Daily Necessities") {
                                state->nextDialogTitle = "Choose your item:";
                                state->nextDialogOptions = {"Tissue", "Battery", "Umbrella", "Back"};
                            }
                        }
                    );
                }
            }
        } 
        else {
            // automatically unlock once the character leaves the trigger area
            shopTriggerLocked = false;
        }

        // update the character
        // Block movement if Fainted
        if (!isFainted && !isExpelled && !waitingForEntranceConfirmation && !dialogSys.isActive() && !gameState.isEating) {
            sf::Vector2f moveInput = inputManager.getMoveInput();
            // Sprint Feature (Z Key) 
            float speedMultiplier = 1.0f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z)) {
                speedMultiplier = 3.0f; // Walk 3x faster
                // Removed Sprint Task call here
            }
            // Pass the modified deltaTime to make character move faster
            character.update(deltaTime * speedMultiplier, moveInput, 
                            tmjMap->getWorldPixelWidth(), 
                            tmjMap->getWorldPixelHeight(),
                            tmjMap.get());

            // Unstuck Failsafe Logic
            // Check if player is trying to move but position isn't changing
            if ((moveInput.x != 0.f || moveInput.y != 0.f)) {
                sf::Vector2f currentPos = character.getPosition();
                
                // Calculate distance moved this frame
                float dist = std::sqrt(std::pow(currentPos.x - lastFramePos.x, 2) + 
                                       std::pow(currentPos.y - lastFramePos.y, 2));
                
                if (dist < 0.1f) {
                    stuckTimer += deltaTime;
                    if (stuckTimer > 3.0f) {
                        Logger::warn("Character appears stuck! Attempting emergency unstuck...");
                        
                        // Attempt to find a safe spot nearby
                        bool foundSafe = false;
                        float step = 32.0f; 
                        std::vector<sf::Vector2f> offsets = {
                            {0.f, step}, {0.f, -step}, {step, 0.f}, {-step, 0.f},
                            {step, step}, {step, -step}, {-step, step}, {-step, -step}
                        };

                        for (const auto& off : offsets) {
                            sf::Vector2f candidate = currentPos + off;
                            if (candidate.x >= 0 && candidate.y >= 0 && 
                                candidate.x < tmjMap->getWorldPixelWidth() && 
                                candidate.y < tmjMap->getWorldPixelHeight()) {
                                
                                if (!tmjMap->feetBlockedAt(candidate)) {
                                    character.setPosition(candidate);
                                    Logger::info("Unstuck successful! Moved to: " + 
                                        std::to_string(candidate.x) + ", " + std::to_string(candidate.y));
                                    foundSafe = true;
                                    stuckTimer = 0.0f;
                                    break;
                                }
                            }
                        }

                        if (!foundSafe) {
                            Logger::error("Failed to find safe spot. Resetting to spawn.");
                            if (tmjMap->getSpawnX() && tmjMap->getSpawnY()) {
                                character.setPosition(sf::Vector2f(*tmjMap->getSpawnX(), *tmjMap->getSpawnY()));
                            }
                            stuckTimer = 0.0f;
                        }
                    }
                } else {
                    stuckTimer = 0.0f;
                }
            } else {
                stuckTimer = 0.0f;
            }
            // Update last pos for next frame
            lastFramePos = character.getPosition();
        }

        // entrance scanning
        sf::Vector2f feet = character.getFeetPoint();
        std::string  tmj  = mapLoader.getCurrentMapPath();
        int minutesNow = timeManager.getHour()*60 + timeManager.getMinute();

        updateEntranceHitByPlayer(
            feet, tmj, "entrance",
            lastEntranceBuilding, lastEntranceMinutes,
            entranceZones, cachedEntranceMapPath,
            minutesNow
        );
        // entrance scanning end


        if (character.getIsResting()) {
            taskManager.modifyEnergy(2.0f * deltaTime);
        }

        // update the eating status
        if (gameState.isEating) {
            gameState.eatingProgress += deltaTime * 10;
            Logger::debug("Eating progress: " + std::to_string(gameState.eatingProgress) + "%");

            taskManager.modifyEnergy(3.0f * deltaTime);

            if (gameState.eatingProgress >= 100.0f) {
                gameState.isEating = false;
                gameState.selectedFood.clear();
                gameState.currentTable.clear();
                gameState.eatingProgress = 0.0f;
                Logger::info("Eating finished - reset state");
                // === NEW: Trigger Task Completion (Bonus Reward) ===
                handleTaskCompletion(taskManager, "eat_food");
            }
        }

        // reset the hasSupperssedEntrance label
        if (hasSuppressedEntrance) {
            // 检查角色是否离开了抑制的入口区域
            sf::Vector2f feet = character.getFeetPoint();
            if (!suppressedEntranceRect.contains(feet)) {
                hasSuppressedEntrance = false;
                Logger::info("Character left suppressed entrance area, re-enabling entrance detection");
            }
        }
        // if fainting or expulsion is being displayed, do not show the entry confirmation dialog box
        if (!waitingForEntranceConfirmation && !hasSuppressedEntrance && !showFaintReminder && !isExpelled) {
            EntranceArea detected;
            if (detectEntranceTrigger(character, tmjMap.get(), detected)) {
                waitingForEntranceConfirmation = true;
                pendingEntrance = detected;
                Logger::info("Detected entrance trigger: '" + detected.name + "' target='" + detected.target + "'");
                renderer.setModalActive(true);
            }
        }

        // Entrance confirmation
        if (waitingForEntranceConfirmation) {
            // If expelled, press Enter to exit the game.
            if (isExpelled && inputManager.isKeyJustPressed(sf::Keyboard::Key::Enter)) {
                result = AppResult::QuitGame;
                renderer.quit();
                break;
            }
            
            if (inputManager.isKeyJustPressed(sf::Keyboard::Key::Enter)) {
                std::string fromKey = mapLoader.getCurrentMapPath();
                if (!fromKey.empty()) {
                    // Compute an offset spawn position one tile away from the entrance area
                    sf::Vector2f originalPos = character.getPosition();
                    sf::Vector2f entranceCenter(
                        pendingEntrance.x + pendingEntrance.width * 0.5f,
                        pendingEntrance.y + pendingEntrance.height * 0.5f
                    );
                    sf::Vector2f dir = originalPos - entranceCenter;
                    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len < 1e-3f) {
                        // Default to upward offset if too close to center
                        dir = sf::Vector2f(0.f, -1.f);
                    } else {
                        dir.x /= len;
                        dir.y /= len;
                    }

                    float tileLen = static_cast<float>(tmjMap->getTileWidth());
                    tileLen = std::max(tileLen, static_cast<float>(tmjMap->getTileHeight()));

                    // Candidate offsets (prefer away direction, fallback to reverse and smaller steps)
                    std::vector<sf::Vector2f> candidates = {
                        originalPos + dir * tileLen,
                        originalPos - dir * tileLen,
                        originalPos + dir * (tileLen * 0.5f),
                        originalPos - dir * (tileLen * 0.5f)
                    };

                    sf::Vector2f chosen = originalPos;
                    float mapW = static_cast<float>(tmjMap->getWorldPixelWidth());
                    float mapH = static_cast<float>(tmjMap->getWorldPixelHeight());
                    for (auto cand : candidates) {
                        // Clamp to map bounds
                        cand.x = std::clamp(cand.x, 0.0f, mapW);
                        cand.y = std::clamp(cand.y, 0.0f, mapH);
                        // Check collision (feet point); if not blocked choose it
                        if (!tmjMap->feetBlockedAt(cand)) {
                            chosen = cand;
                            break;
                        }
                    }

                    Logger::info("Setting spawn override for map " + fromKey + " -> (" +
                                 std::to_string(chosen.x) + "," + std::to_string(chosen.y) + ")");
                    mapLoader.setSpawnOverride(fromKey, chosen.x, chosen.y);
                }
                bool ok = tryEnterTarget(mapLoader, tmjMap, pendingEntrance, character, renderer, configManager);
                if (!ok) {
                    waitingForEntranceConfirmation = false;
                } else {
                    sf::Vector2f pos = character.getPosition();
                    for (const auto& a : tmjMap->getEntranceAreas()) {
                        sf::FloatRect r(sf::Vector2f(a.x, a.y), sf::Vector2f(a.width, a.height));
                        if (r.contains(pos)) {
                            hasSuppressedEntrance = true;
                            suppressedEntranceRect = r;
                            break;
                        }
                    }
                    renderer.setModalActive(false);
                    waitingForEntranceConfirmation = false;
                }
            } else if (inputManager.isKeyJustPressed(sf::Keyboard::Key::Escape)) {
                // Close the reminder dialog box
                if (showFaintReminder) {
                    showFaintReminder = false;
                    faintReminderTimer = 0.0f;
                    continue;
                }
                
                // If expelled, press "Escape" to exit the game.
                if (isExpelled) {
                    result = AppResult::QuitGame;
                    renderer.quit();
                    break;
                }
                cancelEntranceMove(character, *tmjMap);
                waitingForEntranceConfirmation = false;
                renderer.setModalActive(false);
            }
        }

        // check if finished 7 days
        if (currentDay > 7 && !isFinalResultShown) {
            isFinalResultShown = true;
            SettlementData data = calculateSettlementData(taskManager.getPoints(), faintCount);
            bool shouldExit = showFinalResultScreen(renderer, data.grade, data.finalStarCount, data.resultText);
            if (shouldExit) {
                return AppResult::QuitGame;
            }
        }
        // detect the change of day
        if (timeManager.getDay() > currentDay) {
            currentDay = timeManager.getDay();
            Logger::info("Day " + std::to_string(currentDay) + " started");
        }

        // =update camera
        renderer.updateCamera(character.getPosition(),
                              tmjMap->getWorldPixelWidth(),
                              tmjMap->getWorldPixelHeight());

        // render
        renderer.clear();
        mapLoader.render(&renderer);
        renderer.renderTextObjects(tmjMap->getTextObjects());
        renderer.renderEntranceAreas(tmjMap->getEntranceAreas());
        renderer.renderGameTriggerAreas(tmjMap->getGameTriggers());  // 新增：渲染游戏触发区域
        renderer.renderChefs(tmjMap->getChefs());
        renderer.renderProfessors(tmjMap->getProfessors());  // 教授渲染（从App.cpp补充）
        renderer.renderShopTriggerAreas(tmjMap->getShopTriggers()); // 渲染便利店门口触发区域

        
        // adjustment information of professor's position
        static bool showProfessorDebug = true;
        if (showProfessorDebug) {
            for (const auto& prof : tmjMap->getProfessors()) {
                Logger::debug("📍 Professor '" + prof.name + 
                            "' at: (" + std::to_string((int)prof.rect.position.x) + 
                            ", " + std::to_string((int)prof.rect.position.y) + ")");
            }
            showProfessorDebug = false; // only show once
        }
        
        renderer.drawSprite(character.getSprite());

        // render the text "resting"
        if (character.getIsResting()) {
        sf::Text restingText(modalFont, "Resting......", 16);
        restingText.setFillColor(sf::Color::Green);
        restingText.setOutlineColor(sf::Color::Black);
        restingText.setOutlineThickness(1);
    
        sf::Vector2f charPos = character.getPosition();
        restingText.setPosition(sf::Vector2f(charPos.x, charPos.y - 30));
    
        sf::FloatRect textBounds = restingText.getLocalBounds();
        restingText.setOrigin(sf::Vector2f(
            textBounds.position.x + textBounds.size.x / 2,
            textBounds.position.y + textBounds.size.y / 2
        ));
    
        renderer.getWindow().draw(restingText);
    }
    // ==============================================
    // FIXED: UI & OVERLAY RENDER (SCREEN SPACE)
        // 1. Save the current Game Camera (View)
        sf::View gameView = renderer.getWindow().getView();
        
        // 2. Switch to UI Camera (Default Window Coordinates)
        // This ensures the UI and Night Overlay don't move with the player!
        renderer.getWindow().setView(renderer.getWindow().getDefaultView());
        
        // Get actual window size for UI calculations
        sf::Vector2u windowSize = renderer.getWindow().getSize();
        float uiWidth = static_cast<float>(windowSize.x);
        float uiHeight = static_cast<float>(windowSize.y);

        // --- A. DAY/NIGHT OVERLAY ---
        float brightness = timeManager.getDaylightFactor(); 
        if (brightness < 1.0f) {
            // Make a rectangle that covers the WHOLE screen
            sf::RectangleShape nightOverlay(sf::Vector2f(uiWidth, uiHeight));
            nightOverlay.setPosition(sf::Vector2f(0.f, 0.f));
            
            // Calculate Alpha: Brightness 1.0 -> Alpha 0. Brightness 0.3 -> Alpha ~180
            int alpha = static_cast<int>((1.0f - brightness) * 255); 
            
            // Dark Blue-ish tint
            nightOverlay.setFillColor(sf::Color(0, 0, 40, alpha)); 
            renderer.getWindow().draw(nightOverlay);
        }

        // --- B. TIME TEXT ---
        sf::Text timeText(modalFont, "Time: " + timeManager.getFormattedTime(), 24);
        // Position at top-left of SCREEN, not map
        timeText.setPosition(sf::Vector2f(20.f, 20.f)); 
        timeText.setFillColor(sf::Color::White);
        timeText.setOutlineColor(sf::Color::Black);
        timeText.setOutlineThickness(2);
        renderer.getWindow().draw(timeText);

        // --- C. ENERGY BAR ---
        sf::RectangleShape energyBarBg(sf::Vector2f(200.f, 20.f));
        energyBarBg.setPosition(sf::Vector2f(20.f, 60.f));
        energyBarBg.setFillColor(sf::Color(50, 50, 50));
        energyBarBg.setOutlineThickness(2);
        energyBarBg.setOutlineColor(sf::Color::White);
        
        float energyPct = taskManager.getEnergy() / 100.0f;
        sf::RectangleShape energyBarFg(sf::Vector2f(200.f * energyPct, 20.f));
        energyBarFg.setPosition(sf::Vector2f(20.f, 60.f));
        energyBarFg.setFillColor(sf::Color::Yellow);

        renderer.getWindow().draw(energyBarBg);
        renderer.getWindow().draw(energyBarFg);

        // === Numerical Display on Energy Bar ===
        sf::Text energyNumText(modalFont, "Energy: " + std::to_string(taskManager.getEnergy()) + "/" + std::to_string(taskManager.getMaxEnergy()), 14);
        energyNumText.setFillColor(sf::Color::White);
        energyNumText.setOutlineColor(sf::Color::Black);
        energyNumText.setOutlineThickness(1);
        sf::FloatRect enBounds = energyNumText.getLocalBounds();
        energyNumText.setOrigin(sf::Vector2f(enBounds.position.x + enBounds.size.x/2.0f, enBounds.position.y + enBounds.size.y/2.0f));
        energyNumText.setPosition(sf::Vector2f(20.f + 100.f, 60.f + 10.f)); // Center of bar
        renderer.getWindow().draw(energyNumText);

        // === REPLACED EXP BAR WITH POINTS TEXT ===
        sf::Text expNumText(modalFont, "Points: " + std::to_string(taskManager.getPoints()), 20);
        expNumText.setFillColor(sf::Color::Cyan); // Cyan for points
        expNumText.setOutlineColor(sf::Color::Black);
        expNumText.setOutlineThickness(2);
        expNumText.setPosition(sf::Vector2f(20.f, 90.f)); // Position where EXP bar used to be
        renderer.getWindow().draw(expNumText);
        // ===============================================

        // --- D. TASK LIST ---
        float taskY = 120.f;
        sf::Text taskHeader(modalFont, "Tasks:", 20);
        taskHeader.setPosition(sf::Vector2f(20.f, taskY));
        taskHeader.setFillColor(sf::Color::Cyan);
        taskHeader.setOutlineColor(sf::Color::Black);
        taskHeader.setOutlineThickness(1);
        renderer.getWindow().draw(taskHeader);
        
        taskY += 30.f;
        activeTaskHitboxes.clear(); // Reset hitboxes for this frame
        for (const auto& t : taskManager.getTasks()) {
            // === REMOVED "isCompleted" check so tasks always show ===
            sf::Text taskText(modalFont, "- " + t.description, 18);
            taskText.setPosition(sf::Vector2f(25.f, taskY));
            
            // Highlight if mouse is hovering
            sf::Vector2i mpos = sf::Mouse::getPosition(renderer.getWindow());
            sf::FloatRect bounds = taskText.getGlobalBounds();
            
            if (bounds.contains(sf::Vector2f(static_cast<float>(mpos.x), static_cast<float>(mpos.y)))) {
                taskText.setFillColor(sf::Color::Yellow);
            } else {
                taskText.setFillColor(sf::Color::White);
            }

            taskText.setOutlineColor(sf::Color::Black);
            taskText.setOutlineThickness(1);
            renderer.getWindow().draw(taskText);
            
            // Store hitbox for click detection in next frame
            activeTaskHitboxes.push_back({bounds, t.detailedInstruction});

            taskY += 25.f;
        }
        
        // --- E. FAINTED TEXT ---
        if (isFainted && !isBlackScreen) {
            sf::Text faintText(modalFont, "Character passed out due to lack of energy!", 30);
            faintText.setFillColor(sf::Color::Red);
            faintText.setOutlineColor(sf::Color::Black);
            faintText.setOutlineThickness(2);
            
            // Center on screen
            sf::FloatRect bounds = faintText.getLocalBounds();
            faintText.setOrigin(sf::Vector2f(bounds.size.x / 2.0f, bounds.size.y / 2.0f));
            faintText.setPosition(sf::Vector2f(uiWidth / 2.0f, uiHeight / 2.0f));
            
            renderer.getWindow().draw(faintText);
        }
        
        // --- F. BLACK SCREEN ---
        if (isBlackScreen) {
            // Draw a full-screen black overlay layer
            sf::RectangleShape blackOverlay(sf::Vector2f(uiWidth, uiHeight));
            blackOverlay.setPosition(sf::Vector2f(0.f, 0.f));
            blackOverlay.setFillColor(sf::Color::Black);
            renderer.getWindow().draw(blackOverlay);
        }
        
        // --- G. EXPULSION MESSAGE ---
        if (isExpelled) {
            // Draw a translucent background
            sf::RectangleShape expelBg(sf::Vector2f(uiWidth, uiHeight));
            expelBg.setPosition(sf::Vector2f(0.f, 0.f));
            expelBg.setFillColor(sf::Color(0, 0, 0, 200));
            renderer.getWindow().draw(expelBg);
            
            // Display the message of expulsion
            sf::Text expelText(modalFont, "Unfortunately, you have fainted too many times\nand have been expelled. Please go home!", 36);
            expelText.setFillColor(sf::Color::Red);
            expelText.setOutlineColor(sf::Color::Black);
            expelText.setOutlineThickness(3);
            
            sf::FloatRect expelBounds = expelText.getLocalBounds();
            expelText.setOrigin(sf::Vector2f(expelBounds.size.x / 2.0f, expelBounds.size.y / 2.0f));
            expelText.setPosition(sf::Vector2f(uiWidth / 2.0f, uiHeight / 2.0f - 60.0f));
            renderer.getWindow().draw(expelText);
            
            // display Game Over button
            sf::RectangleShape gameOverBtn(sf::Vector2f(200.f, 60.f));
            gameOverBtn.setPosition(sf::Vector2f(uiWidth / 2.0f - 100.f, uiHeight / 2.0f + 40.f));
            
            // Check if the cursor is on the button
            sf::Vector2i mousePos = sf::Mouse::getPosition(renderer.getWindow());
            sf::Vector2f mouseWorldPos = renderer.getWindow().mapPixelToCoords(mousePos);
            if (gameOverBtn.getGlobalBounds().contains(mouseWorldPos)) {
                gameOverBtn.setFillColor(sf::Color(100, 100, 100));
            } else {
                gameOverBtn.setFillColor(sf::Color(50, 50, 50));
            }
            gameOverBtn.setOutlineThickness(2);
            gameOverBtn.setOutlineColor(sf::Color::White);
            renderer.getWindow().draw(gameOverBtn);
            
            // texts on the button
            sf::Text btnText(modalFont, "Game Over", 28);
            btnText.setFillColor(sf::Color::White);
            sf::FloatRect btnBounds = btnText.getLocalBounds();
            btnText.setOrigin(sf::Vector2f(btnBounds.size.x / 2.0f, btnBounds.size.y / 2.0f));
            btnText.setPosition(sf::Vector2f(uiWidth / 2.0f, uiHeight / 2.0f + 70.f));
            renderer.getWindow().draw(btnText);
        }
        
        // --- H. FAINT REMINDER ---
        if (showFaintReminder && !isExpelled) {
            const auto& respawnPoint = tmjMap->getRespawnPoint();
            std::string reminderText = "You have fainted " + std::to_string(faintCount) + 
                                     " times. Exceeding " + std::to_string(respawnPoint.maxCount) + 
                                     " times will result in expulsion!";
            
            renderer.renderModalPrompt(reminderText, modalFont, 24, std::nullopt);
        }
        // =======================

        // === Achievement Popup ===
        if (g_achievementTimer > 0.0f) {
            sf::RectangleShape popBg(sf::Vector2f(uiWidth, 60.f));
            popBg.setPosition(sf::Vector2f(0.f, uiHeight / 2.0f - 30.f));
            popBg.setFillColor(sf::Color(0, 0, 0, 150)); // Semi-transparent black strip
            renderer.getWindow().draw(popBg);

            sf::Text achText(modalFont, g_achievementText, 30);
            achText.setFillColor(sf::Color::Yellow);
            achText.setOutlineColor(sf::Color::Black);
            achText.setOutlineThickness(2);
            sf::FloatRect ab = achText.getLocalBounds();
            achText.setOrigin(sf::Vector2f(ab.position.x + ab.size.x/2.0f, ab.position.y + ab.size.y/2.0f));
            achText.setPosition(sf::Vector2f(uiWidth/2.0f, uiHeight/2.0f));
            renderer.getWindow().draw(achText);
        }

        // --- HINT TOAST  ---
        if (g_hintTimer > 0.f && !g_hintText.empty()) {
            // Select screen size
            sf::Vector2u _sz = renderer.getWindow().getSize();
            float uiWidth  = static_cast<float>(_sz.x);
            float uiHeight = static_cast<float>(_sz.y);

            const float PADDING_X = 24.f;
            const float PADDING_Y = 14.f;

            sf::Text hintText(modalFont, g_hintText, 22);
            hintText.setFillColor(sf::Color::White);
            hintText.setOutlineColor(sf::Color::Black);
            hintText.setOutlineThickness(2.f);

            // Text bounding box
            sf::FloatRect tb = hintText.getLocalBounds();
            float boxW = tb.size.x + PADDING_X * 2.f;
            float boxH = tb.size.y + PADDING_Y * 2.f;

            // Centered at the bottom of the screen, 60 pixels away from the bottom
            float boxX = (uiWidth  - boxW) * 0.5f;
            float boxY = (uiHeight - boxH) - 60.f;

            // Translucent background bar
            sf::RectangleShape bg(sf::Vector2f(boxW, boxH));
            bg.setPosition(sf::Vector2f(boxX, boxY));
            bg.setFillColor(sf::Color(0, 0, 0, 170));
            bg.setOutlineThickness(2.f);
            bg.setOutlineColor(sf::Color(255, 255, 255, 60));

            // Text position: Subtract localBounds.position to avoid baseline offset
            hintText.setPosition(sf::Vector2f(
                boxX + PADDING_X - tb.position.x,
                boxY + PADDING_Y - tb.position.y
            ));

            renderer.getWindow().draw(bg);
            renderer.getWindow().draw(hintText);
        }

        
        // 3. Restore the Game Camera (So the next frame renders the map correctly)
        renderer.getWindow().setView(gameView);
        
        // ==========================================================

        // Draw schedule button (left) and map button (right)
        renderer.drawScheduleButton();
        renderer.drawMapButton();

        if (waitingForEntranceConfirmation) {
            std::string prompt = "Do you want to enter " + pendingEntrance.name + "?  Enter=Yes  Esc=No";
            renderer.renderModalPrompt(prompt, modalFont, configManager.getRenderConfig().text.fontSize, std::nullopt);
        }

        if (dialogSys.isActive()) {
            dialogSys.render(renderer.getWindow());
            if (!dialogSys.isActive()) {
                renderer.setModalActive(false);
            }
        }

        if (gameState.isEating && !gameState.selectedFood.empty() && !gameState.currentTable.empty()) {
            const auto& tables = tmjMap->getTables();
            auto tableIt = std::find_if(tables.begin(), tables.end(),
                [&](const TableObject& t) { return t.name == gameState.currentTable; });
            
            if (tableIt != tables.end()) {
                sf::Vector2f foodPos;
                const auto& foodAnchors = tmjMap->getFoodAnchors();
                auto anchorIt = std::find_if(foodAnchors.begin(), foodAnchors.end(),
                    [&](const FoodAnchor& a) { return a.tableName == gameState.currentTable; });
                
                if (anchorIt != foodAnchors.end()) {
                    foodPos = anchorIt->position;
                } else {
                    foodPos = sf::Vector2f(
                        tableIt->rect.position.x + tableIt->rect.size.x / 2,
                        tableIt->rect.position.y + tableIt->rect.size.y / 2
                    );
                }

                auto foodTexIt = foodTextures.find(gameState.selectedFood);
                if (foodTexIt != foodTextures.end()) {
                    sf::Sprite foodSprite(foodTexIt->second);
                    foodSprite.setOrigin(sf::Vector2f(
                        static_cast<float>(foodTexIt->second.getSize().x) / 2,
                        static_cast<float>(foodTexIt->second.getSize().y) / 2
                    ));
                    // SFML 3 Fix: Use Vector2f
                    foodSprite.setPosition(foodPos);
                    foodSprite.setScale(sf::Vector2f(0.5f, 0.5f));
                    renderer.getWindow().draw(foodSprite);
                } else {
                    sf::RectangleShape placeholder(sf::Vector2f(32, 32));
                    placeholder.setOrigin(sf::Vector2f(16.0f, 16.0f));
                    // SFML 3 Fix: Use Vector2f
                    placeholder.setPosition(foodPos);
                    placeholder.setFillColor(sf::Color::Red);
                    renderer.getWindow().draw(placeholder);
                }

                sf::Text eatingText(modalFont, "Eating...", 16);
                eatingText.setFillColor(sf::Color::White);
                eatingText.setOutlineColor(sf::Color::Black);
                eatingText.setOutlineThickness(1);
                
                sf::Vector2f charPos = character.getPosition();
                // SFML 3 Fix: Use Vector2f
                eatingText.setPosition(sf::Vector2f(charPos.x, charPos.y - 30));
                
                sf::FloatRect textBounds = eatingText.getLocalBounds();
                eatingText.setOrigin(sf::Vector2f(
                    textBounds.position.x + textBounds.size.x / 2,
                    textBounds.position.y + textBounds.size.y / 2
                ));
                
                renderer.getWindow().draw(eatingText);
            }
        }

        renderer.present();
    }
    return AppResult::QuitGame;
}

void showShopDialog(const ShopTrigger& shop) {
    Logger::info("Displaying shop dialog for: " + shop.name);
    Logger::info("Shop rect: (" + std::to_string(shop.rect.position.x) + ", " + std::to_string(shop.rect.position.y) + ") " +
                 std::to_string(shop.rect.size.x) + "x" + std::to_string(shop.rect.size.y));
    std::cout << "Welcome to " << shop.name << "!" << std::endl;
}
