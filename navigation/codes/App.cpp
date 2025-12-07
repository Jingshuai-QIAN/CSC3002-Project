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

// --- Global variables for Achievement System ---
static std::string g_achievementText = "";
static float g_achievementTimer = 0.0f;

// Helper to trigger task completion and show popup
static void handleTaskCompletion(TaskManager& taskManager, const std::string& taskId) {
    std::string achievement = taskManager.completeTask(taskId);
    if (!achievement.empty()) {
        g_achievementText = "Achievement Unlocked: " + achievement;
        g_achievementTimer = 3.0f; // Show for 3 seconds
        Logger::info("🏆 Achievement Unlocked: " + achievement);
    }
}

// Result of the "end of day" popup
enum class EndOfDayChoice {
    BackToHome,     // Go back to Home/Login
    KeepExploring,  // Stay in the current scene and keep exploring
    ExitGame        // Close the game window
};

// Popup that uses the same UI sheet as the Login screen.
static EndOfDayChoice showEndOfDayPopup(Renderer& renderer, const sf::Font& font)
{
    sf::RenderWindow& window = renderer.getWindow();
    window.setView(window.getDefaultView());

    const auto winSize = window.getSize();
    const float winW = static_cast<float>(winSize.x);
    const float winH = static_cast<float>(winSize.y);

    sf::Texture uiTexture;
    if (!uiTexture.loadFromFile("assets/uipack_rpg_sheet.png")) {
        std::cerr << "[EndOfDay] Failed to load assets/uipack_rpg_sheet.png\n";
        return EndOfDayChoice::KeepExploring;
    }

    const sf::IntRect BG_PANEL_RECT{ sf::Vector2i{0, 376}, sf::Vector2i{100, 100} };
    sf::Sprite bgPanel(uiTexture, BG_PANEL_RECT);
    bgPanel.setScale(sf::Vector2f{ winW / static_cast<float>(BG_PANEL_RECT.size.x), winH / static_cast<float>(BG_PANEL_RECT.size.y) });
    bgPanel.setPosition(sf::Vector2f{ 0.f, 0.f });

    const sf::Color deepBrown(150, 100, 60);
    const sf::IntRect BUTTON_KHAKI_RECT{ sf::Vector2i{2, 240}, sf::Vector2i{188, 40} };

    sf::Sprite btnYes(uiTexture, BUTTON_KHAKI_RECT);
    sf::Sprite btnNo (uiTexture, BUTTON_KHAKI_RECT);

    const float baseButtonW = static_cast<float>(BUTTON_KHAKI_RECT.size.x);
    const float baseButtonH = static_cast<float>(BUTTON_KHAKI_RECT.size.y);
    const float targetButtonWidth = winW * 0.25f;
    const float buttonScaleX      = targetButtonWidth / baseButtonW;
    const float buttonScaleY      = buttonScaleX * 1.3f;

    btnYes.setScale(sf::Vector2f{buttonScaleX, buttonScaleY});
    btnNo .setScale(sf::Vector2f{buttonScaleX, buttonScaleY});

    const float centerX = winW * 0.5f;
    const float btnY    = winH * 0.60f;
    const float gapX    = winW * 0.18f;

    btnYes.setOrigin(sf::Vector2f{baseButtonW / 2.f, baseButtonH / 2.f});
    btnNo .setOrigin(sf::Vector2f{baseButtonW / 2.f, baseButtonH / 2.f});
    btnYes.setPosition(sf::Vector2f{centerX - gapX, btnY});
    btnNo .setPosition(sf::Vector2f{centerX + gapX, btnY});

    sf::Text title(font, "Congratulations! You've completed a full day at CUHKSZ!", static_cast<unsigned int>(winH * 0.05f));
    title.setFillColor(sf::Color::White);
    sf::FloatRect b = title.getLocalBounds();
    title.setOrigin(sf::Vector2f{ b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f });
    title.setPosition(sf::Vector2f{centerX, winH * 0.30f});

    sf::Text msg(font, "Do you want to return to the real world?", static_cast<unsigned int>(winH * 0.035f));
    msg.setFillColor(sf::Color::White);
    b = msg.getLocalBounds();
    msg.setOrigin(sf::Vector2f{ b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f });
    msg.setPosition(sf::Vector2f{centerX, winH * 0.40f});

    sf::Text yesText(font, "Go back to Home", static_cast<unsigned int>(winH * 0.035f));
    sf::Text noText(font, "Keep exploring", static_cast<unsigned int>(winH * 0.035f));
    yesText.setFillColor(sf::Color::White);
    noText.setFillColor(sf::Color::White);

    auto centerTextOnButton = [](sf::Text& text, const sf::Sprite& btn) {
        sf::FloatRect tb = text.getLocalBounds();
        sf::FloatRect bb = btn.getGlobalBounds();
        text.setOrigin(sf::Vector2f{ tb.position.x + tb.size.x / 2.f, tb.position.y + tb.size.y / 2.f });
        text.setPosition(sf::Vector2f{ bb.position.x + bb.size.x / 2.f, bb.position.y + bb.size.y / 2.f });
    };
    centerTextOnButton(yesText, btnYes);
    centerTextOnButton(noText,  btnNo);

    while (window.isOpen()) {
        std::optional<sf::Event> evOpt;
        while ((evOpt = window.pollEvent()).has_value()) {
            const sf::Event& ev = *evOpt;
            if (ev.is<sf::Event::Closed>()) return EndOfDayChoice::ExitGame;
            if (const auto* mb = ev.getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mousePos(static_cast<float>(mb->position.x), static_cast<float>(mb->position.y));
                    if (btnYes.getGlobalBounds().contains(mousePos)) return EndOfDayChoice::BackToHome;
                    if (btnNo.getGlobalBounds().contains(mousePos)) return EndOfDayChoice::KeepExploring;
                }
            }
            if (const auto* key = ev.getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Enter) return EndOfDayChoice::BackToHome;
                if (key->code == sf::Keyboard::Key::Escape) return EndOfDayChoice::KeepExploring;
            }
        }
        window.clear(deepBrown);
        window.draw(bgPanel);
        window.draw(title);
        window.draw(msg);
        window.draw(btnYes);
        window.draw(btnNo);
        window.draw(yesText);
        window.draw(noText);
        window.display();
    }
    return EndOfDayChoice::ExitGame;
}

// Helper: detect whether character is inside an entrance and facing it.
static bool detectEntranceTrigger(const Character& character, const TMJMap* map, EntranceArea& outArea) {
    if (!map) return false;
    sf::Vector2f feet = character.getFeetPoint();
    for (const auto& a : map->getEntranceAreas()) {
        sf::FloatRect rect(sf::Vector2f(a.x, a.y), sf::Vector2f(a.width, a.height));
        
        // 完全移除朝向要求：只要角色在入口区域内就触发
        // 这样可以避免加速时因为朝向不对或移动过快而无法触发的问题
        if (rect.contains(feet)) {
            outArea = a;
            return true;
        }
    }
    return false;
}

// 修复 GameTriggerArea 的 rect 使用问题
static bool detectGameTrigger(const Character& character, const TMJMap* map, GameTriggerArea& outArea) {
    if (!map) return false;

    sf::Vector2f feet = character.getFeetPoint();
    for (const auto& gta : map->getGameTriggers()) {
        sf::FloatRect rect(sf::Vector2f(gta.x, gta.y), sf::Vector2f(gta.width, gta.height)); // 修正构造方式
        if (rect.contains(feet)) {
            outArea = gta;
            Logger::info("Auto-triggering game for: " + gta.name);
            return true;
        }
    }

    return false;
}

// 教授交互检测函数（从App.cpp补充）
static bool detectProfessorInteraction(const Character& character, const TMJMap* map, Professor& outProf) {
    if (!map) return false;

    sf::Vector2f center = character.getPosition();  // ✅ 用人物中心
    const auto& professors = map->getProfessors();

    for (const auto& prof : professors) {
        if (!prof.available) continue;

        if (prof.rect.contains(center)) {
            Logger::info("🎯 SUCCESS: Player touched Professor: " + prof.name);
            outProf = prof;
            return true;
        }
    }

    return false;
}

// 检测商店触发区域（用于自动触发对话框）
static bool detectShopTrigger(const Character& character, const TMJMap* map, ShopTrigger& outShop) {
    if (!map) {
        Logger::debug("detectShopTrigger: map is null");
        return false;
    }

    sf::Vector2f feet = character.getFeetPoint();
    const auto& shopTriggers = map->getShopTriggers();

    for (const auto& shop : shopTriggers) {
        // 检测所有商店触发区域，包括 familymart
        if (shop.rect.contains(feet)) {
            outShop = shop;
            Logger::info("Detected shop trigger area: " + shop.name);
            return true;
        }
    }

    return false;
}

// Helper: show the full-map modal (blocking) 
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

    // 修复 displayW/displayH 未定义
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
        // SFML 3.0.2 事件轮询
        std::optional<sf::Event> evOpt = mapWin.pollEvent();
        while (evOpt.has_value()) {
            sf::Event& ev = evOpt.value();
            
            // 关闭窗口事件
            if (auto closed = ev.getIf<sf::Event::Closed>()) {
                mapWin.close(); 
                break; 
            }
            
            // 键盘按下事件 — 统一使用 Key 枚举（解决 Scan/Key 不匹配）
            if (auto keyPressed = ev.getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Escape) {
                    mapWin.close(); 
                    break; 
                }
            }

            // 鼠标滚轮事件
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

            // 鼠标按下事件
            if (auto mousePressed = ev.getIf<sf::Event::MouseButtonPressed>()) {
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    dragging = true;
                    prevDragPixel = mousePressed->position;
                }
            }
            
            // 鼠标释放事件
            if (auto mouseReleased = ev.getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseReleased->button == sf::Mouse::Button::Left) {
                    dragging = false;
                }
            }
            
            // 鼠标移动事件
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

// Helper: show the schedule image in a blocking modal window
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

// 修复：交互检测函数（添加日志）
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
        if (obj.type != "counter") continue; // 只检测Counter
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

// 餐桌交互检测函数
static bool detectTableInteraction(const Character& character, const TMJMap* map, TableObject& outTable) {
    if (!map) {
        Logger::error("detectTableInteraction: map is null");
        return false;
    }
    sf::Vector2f feet = character.getFeetPoint();
    Logger::info("detectTableInteraction: feet coordinates = (" + std::to_string(feet.x) + "," + std::to_string(feet.y) + ")");
    
    const auto& tables = map->getTables();
    // 新增日志：打印解析到的餐桌数量
    Logger::info("detectTableInteraction: Total tables in map: " + std::to_string(tables.size()));
    if (tables.empty()) {
        Logger::warn("detectTableInteraction: No tables found in map");
        return false;
    }

    for (const auto& table : tables) {
        // 2. 加5px容差（解决SFML坐标精度问题）
        sf::FloatRect tolerantRect = table.rect;
        tolerantRect.position.x -= 5;  // 替代 left
        tolerantRect.position.y -= 5;  // 替代 top
        tolerantRect.size.x += 10;     // 替代 width
        tolerantRect.size.y += 10;     // 替代 height

        Logger::debug("detectTableInteraction: detact table → name: " + table.name + 
                     " | original rect: (" + std::to_string(table.rect.position.x) + "," + std::to_string(table.rect.position.y) + 
                     ") | tolerant rect: (" + std::to_string(tolerantRect.position.x) + "," + std::to_string(tolerantRect.position.y) + ")");

        // 3. 检测脚部是否在容差范围内
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

// 食物纹理加载函数
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

// Scan → Key 转换函数（来自你的代码）
// 手动实现 Scan → Key 转换（SFML 3.0.2 无内置方法）
static sf::Keyboard::Key scanToKey(sf::Keyboard::Scan scanCode) {
    switch (scanCode) {
        case sf::Keyboard::Scan::E:      return sf::Keyboard::Key::E;
        case sf::Keyboard::Scan::Enter:  return sf::Keyboard::Key::Enter;
        case sf::Keyboard::Scan::Escape: return sf::Keyboard::Key::Escape;
        default: return sf::Keyboard::Key::Unknown;
    }
}

// 草坪休息检测函数
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

// 新增：计算最终评级
FinalResult calculateFinalResult(int totalPoints) {
    FinalResult result;
    result.totalPoints = totalPoints;

    // 计算星级（1-5）
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

bool isFinalResultShown = false;     // 结算面板显示标记
bool pendingEndOfDayCheck = false;   // 结束检查标记
bool endOfDayPopupShown = false;     // 结束弹窗标记


// 新增：显示结算界面
bool showFinalResultScreen(Renderer& renderer, const FinalResult& result) {
    sf::RenderWindow& window = renderer.getWindow();
    sf::Font font;
    if (!font.openFromFile("fonts/arial.ttf")) {
        Logger::error("Failed to load font for final result");
        return true;
    }

    //  独立加载结束面板背景（复用素材但独立控制） 
    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile("textures/dialog_bg.png")) { // 复用素材文件
        Logger::error("Failed to load dialog_bg.png");
        return true;
    }
    sf::Sprite bgSprite(bgTexture);

    //  独立计算结束面板的缩放和居中（核心解耦逻辑）
    // 目标尺寸：窗口的 70% 宽高（可独立调整，不影响DialogSystem）
    const float PANEL_SCALE_RATIO = 0.7f; 
    sf::Vector2u windowSize = window.getSize();
    sf::Vector2u bgTexSize = bgTexture.getSize();

    // 计算缩放比例（等比缩放，适配窗口70%尺寸）
    float scaleX = (windowSize.x * PANEL_SCALE_RATIO) / bgTexSize.x;
    float scaleY = (windowSize.y * PANEL_SCALE_RATIO) / bgTexSize.y;
    float finalScale = std::min(scaleX, scaleY); // 等比缩放，避免拉伸

    // 设置背景缩放（独立于DialogSystem的缩放）
    bgSprite.setScale(sf::Vector2f(finalScale, finalScale));

    // 计算居中位置（独立计算，不依赖DialogSystem的居中逻辑）
    sf::FloatRect bgBounds = bgSprite.getGlobalBounds();
    float bgX = (windowSize.x - bgBounds.size.x) / 2.0f;  
    float bgY = (windowSize.y - bgBounds.size.y) / 2.0f; 
    bgSprite.setPosition(sf::Vector2f(bgX, bgY));

    // 等级文本（独立排版）
    sf::Text gradeText(font, "", 36);
    std::string gradeStr;
    switch (result.grade) {
        case Grade::A: gradeStr = "A"; break;
        case Grade::B: gradeStr = "B"; break;
        case Grade::C: gradeStr = "C"; break;
        case Grade::D: gradeStr = "D"; break;
        case Grade::F: gradeStr = "F"; break;
    }
    gradeText.setString("You got an " + gradeStr + " in the game!");
    gradeText.setFillColor(sf::Color::White);
    gradeText.setCharacterSize(36);
    // 文本居中（相对于面板）
    sf::FloatRect gradeBounds = gradeText.getLocalBounds();
    gradeText.setOrigin(sf::Vector2f(gradeBounds.size.x / 2, gradeBounds.size.y / 2));
    gradeText.setPosition(sf::Vector2f(
        windowSize.x / 2.0f,          // 窗口水平居中
        bgY + bgBounds.size.y * 0.3f  // 面板垂直30%位置（修复：height → size.y）
    ));

    //  星星显示
    const float starSize = 50.f;
    sf::Texture starYTexture, starGTexture;
    if (!starYTexture.loadFromFile("textures/star_y.png") || !starGTexture.loadFromFile("textures/star_g.png")) {
        Logger::error("Failed to load star textures");
        return true;
    }
    std::vector<sf::Sprite> stars;
    // 星星区域居中
    float starStartX = (windowSize.x - (starSize * 5 + 20.f * 4)) / 2;
    float starY = bgY + bgBounds.size.y * 0.5f;
    for (int i = 0; i < 5; ++i) {
        sf::Sprite star(i < result.starCount ? starYTexture : starGTexture);
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

    // 双按钮布局（退出+重来）
    const float btnWidth = 180.f;  // 按钮宽度（缩小一点适配双按钮）
    const float btnHeight = 60.f;  // 按钮高度

    // 按钮2：退出（深棕色）
    sf::RectangleShape exitBtn(sf::Vector2f(btnWidth, btnHeight));
    exitBtn.setFillColor(sf::Color(139, 69, 19)); // 深棕色
    exitBtn.setOutlineColor(sf::Color(80, 40, 10));
    exitBtn.setOutlineThickness(2.f);

    // 双按钮居中布局（整体居中，左右分布）
    float btnX = (windowSize.x - btnWidth) / 2;  // 水平居中
    float btnY = bgY + bgBounds.size.y * 0.7f;    // 垂直位置（面板70%处）
    exitBtn.setPosition(sf::Vector2f(btnX, btnY));


    // 按钮文本：退出
    sf::Text exitText(font, "Exit", 24);
    exitText.setFillColor(sf::Color::White);
    sf::FloatRect exitTextBounds = exitText.getLocalBounds();
    exitText.setOrigin(sf::Vector2f(exitTextBounds.size.x / 2, exitTextBounds.size.y / 2));
    exitText.setPosition(sf::Vector2f(
        exitBtn.getPosition().x + btnWidth / 2,
        exitBtn.getPosition().y + btnHeight / 2
    ));

    // 事件循环（双按钮交互）
    sf::View originalView = window.getView();
    window.setView(window.getDefaultView());

    bool shouldExit = false;
    bool isRunning = true;

    while (window.isOpen() && isRunning) {
        std::optional<sf::Event> event;
        while ((event = window.pollEvent()).has_value()) {
            // 窗口关闭
            if (event->is<sf::Event::Closed>()) {
                window.close();
                isRunning = false;
                shouldExit = true;
            }

            // 鼠标点击事件（仅检测 Exit 按钮）
            if (const auto* mouseEvent = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouseEvent->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mousePos = window.mapPixelToCoords(
                        sf::Vector2i(mouseEvent->position.x, mouseEvent->position.y)
                    );

                    // 仅处理退出按钮点击
                    if (exitBtn.getGlobalBounds().contains(mousePos)) {
                        shouldExit = true;
                        isRunning = false;
                    }
                }
            }
        }

        //  鼠标悬停效果
        sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
        sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);
        if (exitBtn.getGlobalBounds().contains(mouseWorldPos)) {
            exitBtn.setFillColor(sf::Color(150, 80, 30));
        } else {
            exitBtn.setFillColor(sf::Color(139, 69, 19));
        }


        // 独立渲染
        window.clear(sf::Color(40, 40, 40));
        window.draw(bgSprite);
        window.draw(gradeText);
        for (const auto& star : stars) window.draw(star);
        window.draw(exitBtn);
        window.draw(exitText);
        window.display();
    }

    // 恢复视图（防止影响后续游戏渲染）
    window.setView(originalView);
    return shouldExit;
}

// 教授回应状态结构体（从App.cpp补充）
struct ProfessorResponseState {
    bool pending = false;
    std::string professorName;
    std::string professorCourse;
    std::string dialogType;
    int selectedOption = -1;
    std::string selectedText;
};

// NEW: Struct to store clickable task areas for the Event Loop
struct TaskHitbox {
    sf::FloatRect bounds;
    std::string detailText;
};



AppResult runApp(
    Renderer& renderer,
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    Character& character,
    sf::View& view,
    ConfigManager& configManager
) {

    int currentDay = 1;               // 初始天数
    bool isFinalResultShown = false;  // 是否显示过最终结果
    auto& inputManager = InputManager::getInstance();
    // === NEW: Initialize Systems (Time & Tasks) ===
    TimeManager timeManager;
    TaskManager taskManager;

    AppResult result = AppResult::QuitGame;   // Default: quit game
    bool endOfDayPopupShown = false;          // Ensure we only show the popup once
    bool pendingEndOfDayCheck = false;        // NEW: 已达到"可以结束一天"，但还在等当前任务结束

    // Load initial tasks
    // Params: id, description, detailed instruction, achievement name, points, energy
    
    // Sum = 100 XP Total
    taskManager.addTask("eat_food", 
        "Eat Food at Canteen", 
        "Go to the Student Centre and press E at the counter to order food, then sit at a table and press E to eat.", 
        "Foodie", 
        10, 0); 
    
    taskManager.addTask("attend_class", 
        "Attend Class (Quiz)", 
        "Find a classroom. Enter the trigger zone to start the class quiz.", 
        "Scholar", 
        20, 0);
    
    taskManager.addTask("rest_lawn", 
        "Rest on Lawn", 
        "Walk onto the green lawn before the library. Press E to rest and recover energy.", 
        "Nature Lover", 
        10, 0); 
    
    taskManager.addTask("buy_item", 
        "Buy Item at FamilyMart", 
        "Locate the FamilyMart shop. Press E at the entrance to buy items.", 
        "Big Spender", 
        10, 0); 
    
    taskManager.addTask("talk_professor", 
        "Talk to a Professor", 
        "Find a professor on the map. Press E to start a conversation.", 
        "Networker", 
        15, 0); 

    taskManager.addTask("bookstore_quiz", 
        "Solve Bookstore Puzzle", 
        "Go to the Bookstore. Enter the trigger area to solve the CUHK(SZ) questions.", 
        "Bookworm", 
        25, 0);

    // === REMOVED "sprint_practice" TASK ===
    // =============================================
    
    if (!renderer.initializeChefTexture()) {
        Logger::error("Failed to initialize chef texture");
        return AppResult::QuitGame;
    }
    
    // 教授纹理初始化（从App.cpp补充）
    if (!renderer.initializeProfessorTexture()) {
        Logger::error("Failed to initialize professor texture");
        return AppResult::QuitGame;
    }
    
    // 加载模态字体
    sf::Font modalFont;
    if (!modalFont.openFromFile(configManager.getRenderConfig().text.fontPath)) {
        Logger::error("Failed to load modal font!");
        return AppResult::QuitGame;
    }
    
    // ：只初始化一次对话框（避免重复加载）
    DialogSystem dialogSys(modalFont, 24);
    bool dialogInitSuccess = false;
    try {
        // 拼接完整的素材路径（根据你的项目目录调整）
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

    // 加载食物贴图
    auto foodTextures = loadFoodTextures();
    // 游戏状态（进食相关）
    struct GameState {
        bool isEating = false;
        bool hasOrderedFood = false; // 从App.cpp补充：标记是否已点餐
        std::string currentTable;
        std::string selectedFood;
        float eatingProgress = 0.0f;
    };
    GameState gameState;

    struct ShoppingState {
    bool isShopping = false;

    // 一级分类 & 二级商品
    std::string selectedCategory;
    std::string selectedItem;

    // 购物进度
    float shoppingProgress = 0.0f;

    // ===== 控制"下一步要弹什么对话"的核心状态 =====
    bool requestNextDialog = false;

    std::string nextDialogTitle;
    std::vector<std::string> nextDialogOptions;

    // ✅ ✅ ✅ 你必须新增的成员（本问题的关键）
    enum class NextDialogKind {
        None,
        ShowFirstLevel,    // 显示 FamilyMart 一级分类
        ShowSecondLevel,  // 显示 某一分类下的商品
        ConfirmPurchase   // 显示 购买确认框
    };

    NextDialogKind nextDialogKind = NextDialogKind::None;
};
    ShoppingState shoppingState;

    // 教授回应状态（从App.cpp补充）
    static ProfessorResponseState profResponseState;

    // === NEW: Fainting State ===
    bool isFainted = false;
    float faintTimer = 0.0f;
    bool isBlackScreen = false;  // 黑屏状态
    float blackScreenTimer = 0.0f;  // 黑屏计时器
    int faintCount = 0;  // 晕倒次数
    bool showFaintReminder = false;  // 是否显示晕倒提醒
    float faintReminderTimer = 0.0f;  // 提醒显示计时器（5秒后自动关闭）
    bool isExpelled = false;  // 是否被退学
    // ==========================

    // 入口确认状态
    bool waitingForEntranceConfirmation = false;
    EntranceArea pendingEntrance;
    bool hasSuppressedEntrance = false;
    sf::FloatRect suppressedEntranceRect;

    // Vector to store hitboxes of tasks drawn in the previous frame
    std::vector<TaskHitbox> activeTaskHitboxes;

    // === NEW: Unstuck State ===
    sf::Vector2f lastFramePos = character.getPosition();
    float stuckTimer = 0.0f;

    // 主循环
    sf::Clock clock;
    while (renderer.isRunning()) {
        // ✅✅✅ 在"新的一帧刚开始"时安全执行对话回调（从App.cpp补充）
        if (dialogSys.hasPendingCallback()) {
            Logger::info("🔄 Executing pending dialog callback...");
            auto cb = dialogSys.consumePendingCallback();
            cb();
            Logger::info("🔄 Dialog callback executed");
            // 回调执行后关闭对话框
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

        const float PASSIVE_DEPLETION_RATE = 10.0f / 30.0f;
        taskManager.modifyEnergy(-PASSIVE_DEPLETION_RATE * deltaTime);
        // =====================================

        // 处理提醒计时器（5秒后自动关闭）
        if (showFaintReminder) {
            faintReminderTimer += deltaTime;
            if (faintReminderTimer >= 5.0f) {
                showFaintReminder = false;
                faintReminderTimer = 0.0f;
                Logger::info("Faint reminder auto-closed after 5 seconds");
            }
        }

        // === NEW: End-of-day Check Logic (Teammate's update) ===
        // 1) 先记录: Points 已经达到"可以结束一天"的条件
        if (!endOfDayPopupShown && !pendingEndOfDayCheck && taskManager.getPoints() >= taskManager.getDailyGoal()) {
            pendingEndOfDayCheck = true;
        }

        // 2) 当前是否在忙碌"任务动作"
        bool isBusyWithTask = 
            dialogSys.isActive() ||         // 还在对话框
            gameState.isEating ||           // 正在吃饭
            shoppingState.isShopping ||     // 正在便利店购物
            isFainted ||                    // 晕倒动画中
            isExpelled ||                   // 被退学
            waitingForEntranceConfirmation; // 正在问"是否进入某建筑"

        // 3) 只有当: 已经满足结束条件 + 不在忙任务 + 成就提示已经结束, 才弹结束弹窗
        if (pendingEndOfDayCheck && 
            !endOfDayPopupShown && 
            !isBusyWithTask && 
            g_achievementTimer <= 0.0f) // ⭐ 确保 Achievement Popup 已经展示完
        {
            endOfDayPopupShown = true;
            pendingEndOfDayCheck = false;

            EndOfDayChoice choice = showEndOfDayPopup(renderer, modalFont);

            if (choice == EndOfDayChoice::BackToHome) {
                // Player chose to go back to Home/Login
                // 回到 Home/Login
                result = AppResult::BackToLogin;
                break; // Leave the main game loop
            } else if (choice == EndOfDayChoice::ExitGame) {
                // Player closed the popup window / chose exit
                // 直接退出游戏
                AppResult appResult = AppResult::QuitGame;
                renderer.quit();
                break;
            }
            // If choice == KeepExploring: 什么都不做，玩家继续在地图上走
        }
        // ========================================================
        if (timeManager.getFormattedTime() == "23:59" && !isFinalResultShown) {
            // 触发结算
            FinalResult result;
            result.starCount = (taskManager.getPoints() >= taskManager.getDailyGoal()) ? 5 : taskManager.getPoints() / 100;
            result.grade = (result.starCount >= 5) ? Grade::A : Grade::F;

            // 调用结束面板（仅返回是否退出）
            bool shouldExit = showFinalResultScreen(renderer, result);
            if (shouldExit) {
                return AppResult::QuitGame; // 点击退出则退出游戏
            }
            isFinalResultShown = true;
        }
        // ========== 处理教授回应的逻辑（从App.cpp补充） ==========
        if (profResponseState.pending && !dialogSys.isActive()) {
            Logger::info("🔄 Processing professor response - pending: true, option: " + 
                std::to_string(profResponseState.selectedOption));
            std::string response;
            std::string profName = profResponseState.professorName;
            std::string profCourse = profResponseState.professorCourse;
            std::string profDialogType = profResponseState.dialogType;
            int optionIndex = profResponseState.selectedOption;
            Logger::info("📋 Professor info: " + profName + ", course: " + profCourse + 
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
            
            // === NEW: Trigger Task Completion & Deduct Energy ===
            handleTaskCompletion(taskManager, "talk_professor");
            taskManager.modifyEnergy(-2.0f);            
            // ====================================================

            // 显示回应对话框
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

        // ========== 处理商店购物二级菜单（新增） ==========
        if (shoppingState.requestNextDialog && !dialogSys.isActive()) {
            Logger::info("🛒 requestNextDialog handling | kind = " + std::to_string(static_cast<int>(shoppingState.nextDialogKind)));

            // 1) 如果是展示二级菜单（例如 Food/Drink/…）
            if (shoppingState.nextDialogKind == ShoppingState::NextDialogKind::ShowSecondLevel) {
                dialogSys.setDialog(
                    shoppingState.nextDialogTitle,
                    shoppingState.nextDialogOptions,
                    // 回调：只写状态，不直接调用 dialogSys.setDialog()
                    [&shoppingState](const std::string& selected) {
                        Logger::info("🔔 second-level callback selected: " + selected);
                        if (selected == "Back") {
                            // 请求显示一级菜单（通过设置 nextDialogKind）
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::ShowFirstLevel;
                            shoppingState.nextDialogTitle = "Welcome to FamilyMart! Which section would you like to browse?";
                            shoppingState.nextDialogOptions = {"Food", "Drink", "Daily Necessities", "Cancel"};
                            shoppingState.requestNextDialog = true;
                        } else {
                            // 选中具体商品，准备弹出确认对话
                            shoppingState.selectedItem = selected;
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::ConfirmPurchase;
                            shoppingState.nextDialogTitle = "\n\nPrice:15yuan\n\nProceed with purchase?";
                            shoppingState.nextDialogOptions = {"Yes, buy it", "No, go back"};
                            shoppingState.requestNextDialog = true;
                        }
                    }
                );

                // 完成请求处理
                shoppingState.requestNextDialog = false;
                renderer.setModalActive(true);
            }
            // 2) 如果是显示一级菜单
            else if (shoppingState.nextDialogKind == ShoppingState::NextDialogKind::ShowFirstLevel) {
                dialogSys.setDialog(
                    shoppingState.nextDialogTitle,
                    shoppingState.nextDialogOptions,
                    // 回调：处理用户选一级菜单（仍然只写状态）
                    [&shoppingState](const std::string& selected) {
                        Logger::info("🛒 Category Selected: " + selected);
                        if (selected == "Cancel") {
                            shoppingState.isShopping = false;
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::None;
                            shoppingState.requestNextDialog = false;
                            return;
                        }

                        shoppingState.selectedCategory = selected;
                        // 根据分类准备二级菜单选项
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
                            // Fallback：回到一级菜单
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
            // 3) 如果是显示购买确认对话
            else if (shoppingState.nextDialogKind == ShoppingState::NextDialogKind::ConfirmPurchase) {
                dialogSys.setDialog(
                    shoppingState.nextDialogTitle,
                    shoppingState.nextDialogOptions,
                    // 购买确认回调：不要直接生成新的 dialog，直接修改状态
                    [&shoppingState, &taskManager](const std::string& choice) {
                        Logger::info("🛒 Purchase Choice: " + choice + " for item " + shoppingState.selectedItem);
                        if (choice == "Yes, buy it") {
                            // 执行购买逻辑
                            Logger::info("🛒 Purchased: " + shoppingState.selectedItem);
                            
                            // === NEW: Trigger Task Completion & Deduct Energy ===
                            handleTaskCompletion(taskManager, "buy_item");
                            taskManager.modifyEnergy(-5.0f);      
                            // ====================================================

                            shoppingState.isShopping = false;
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::None;
                            shoppingState.requestNextDialog = false;
                        } else {
                            // 回到二级商品选择（同类）
                            shoppingState.nextDialogKind = ShoppingState::NextDialogKind::ShowSecondLevel;
                            // 重新构建 second-level 的 title/options（基于 selectedCategory）
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
                                // 保险回到一级菜单
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
            // 其他情况：忽略
            else {
                shoppingState.requestNextDialog = false;
                shoppingState.nextDialogKind = ShoppingState::NextDialogKind::None;
            }
        }

        // === NEW: Fainting Logic Check ===
        // Must not be currently eating/interacting/fainted
        if (!isFainted && !isBlackScreen && !gameState.isEating && !dialogSys.isActive() && !isExpelled) {
            if (taskManager.getEnergy() <= 0) {
                isFainted = true;
                faintTimer = 0.0f;
                isBlackScreen = false;
                blackScreenTimer = 0.0f;
                faintCount++;  // 增加晕倒次数
                // Force character direction Up (Visual for passing out)
                character.setCurrentDirection(Character::Direction::Up);
                Logger::info("Character passed out due to lack of energy! Faint count: " + std::to_string(faintCount));
                
                // 检查是否超过最大次数
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
            
            // 显示消息4秒后，进入黑屏状态
            if (faintTimer > 4.0f && !isBlackScreen) {
                isBlackScreen = true;
                blackScreenTimer = 0.0f;
                Logger::info("Entering black screen...");
            }
            
            // 黑屏2秒后，重生到 clinic 门口
            if (isBlackScreen) {
                blackScreenTimer += deltaTime;
                
                if (blackScreenTimer >= 2.0f) {
                    // 检查是否被退学
                    if (isExpelled) {
                        // 显示退学消息，游戏结束
                        Logger::error("Character expelled! Game over.");
                        // 游戏结束逻辑将在渲染部分处理
                    } else {
                        // 检查当前地图是否是 LG_campus_map，如果不是则切换
                        std::string currentMapPath = mapLoader.getCurrentMapPath();
                        bool needSwitchMap = false;
                        if (currentMapPath.find("LG_campus_map") == std::string::npos) {
                            needSwitchMap = true;
                            Logger::info("Not in LG_campus_map, switching to LG_campus_map for respawn");
                            
                            // 加载 LG_campus_map
                            std::string campusMapPath = mapLoader.getMapDirectory() + "LG_campus_map.tmj";
                            auto campusMap = mapLoader.loadTMJMap(campusMapPath);
                            if (campusMap) {
                                tmjMap = campusMap;
                                Logger::info("Switched to LG_campus_map successfully");
                            } else {
                                Logger::error("Failed to load LG_campus_map, using current map");
                            }
                        }
                        
                        // 阻止入口确认对话框显示
                        waitingForEntranceConfirmation = false;
                        hasSuppressedEntrance = true;
                        
                        // 重生到重生点
                        const auto& respawnPoint = tmjMap->getRespawnPoint();
                        sf::Vector2f respawnPos = respawnPoint.position;
                        
                        Logger::info("Respawn point position: (" + std::to_string(respawnPos.x) + ", " + std::to_string(respawnPos.y) + ")");
                        
                        // 如果重生点位置无效（为0或未设置），使用默认spawn点
                        if (respawnPos.x == 0.0f && respawnPos.y == 0.0f) {
                            Logger::warn("Respawn point is at (0,0), using default spawn point");
                            if (tmjMap->getSpawnX() && tmjMap->getSpawnY()) {
                                respawnPos = sf::Vector2f(*tmjMap->getSpawnX(), *tmjMap->getSpawnY());
                            } else {
                                Logger::error("No valid respawn point or default spawn point available!");
                            }
                        }
                        
                        // 计算脚部到中心的偏移量（用于从脚部位置反推中心位置）
                        sf::Vector2f currentFeet = character.getFeetPoint();
                        sf::Vector2f currentCenter = character.getPosition();
                        sf::Vector2f feetToCenterOffset = currentCenter - currentFeet;
                        
                        // 在重生点周围搜索可行走的位置
                        float tileSize = static_cast<float>(std::max(tmjMap->getTileWidth(), tmjMap->getTileHeight()));
                        float step = tileSize * 0.5f;
                        
                        std::vector<sf::Vector2f> offsets = {
                            sf::Vector2f(0, -step * 2),      // 上
                            sf::Vector2f(0, step * 2),        // 下
                            sf::Vector2f(-step * 2, 0),      // 左
                            sf::Vector2f(step * 2, 0),       // 右
                            sf::Vector2f(-step, -step),     // 左上
                            sf::Vector2f(step, -step),      // 右上
                            sf::Vector2f(-step, step),       // 左下
                            sf::Vector2f(step, step),       // 右下
                            sf::Vector2f(0, -step),          // 上（更近）
                            sf::Vector2f(0, step),           // 下（更近）
                            sf::Vector2f(-step, 0),          // 左（更近）
                            sf::Vector2f(step, 0),           // 右（更近）
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
                            // 如果找不到可行走位置，尝试使用默认 spawn 点
                            if (tmjMap->getSpawnX() && tmjMap->getSpawnY()) {
                                respawnPos = sf::Vector2f(*tmjMap->getSpawnX(), *tmjMap->getSpawnY());
                                Logger::warn("Could not find walkable position at respawn point, using default spawn");
                            }
                        }
                        
                        // 设置角色位置
                        character.setPosition(respawnPos);
                        
                        // 时间增加2小时
                        timeManager.addHours(2);
                        
                        // 恢复精力到一定值
                        taskManager.modifyEnergy(50.0f);
                        
                        // 重置状态
                        isFainted = false;
                        isBlackScreen = false;
                        faintTimer = 0.0f;
                        blackScreenTimer = 0.0f;
                        
                        // 显示提醒（5秒后自动关闭）
                        showFaintReminder = true;
                        faintReminderTimer = 0.0f;
                        
                        // 更新相机位置
                        renderer.updateCamera(respawnPos, tmjMap->getWorldPixelWidth(), tmjMap->getWorldPixelHeight());
                        
                        Logger::info("Character respawned at respawn point (" + 
                                   std::to_string(respawnPos.x) + ", " + std::to_string(respawnPos.y) + 
                                   "). Faint count: " + std::to_string(faintCount));
                    }
                }
            }
        }
        // =================================

        //统一事件处理（只轮询一次）
        std::optional<sf::Event> eventOpt;
        while ((eventOpt = renderer.pollEvent()).has_value()) {
            sf::Event& event = eventOpt.value();

            // 优先处理对话框事件
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


            // 全屏地图按钮（原有逻辑）
            if (event.is<sf::Event::MouseButtonPressed>()) {
                auto mb = event.getIf<sf::Event::MouseButtonPressed>();
                if (mb && mb->button == sf::Mouse::Button::Left) {
                    sf::Vector2i mpos = mb->position;
                    
                    // 检查是否点击了 Game Over 按钮
                    if (isExpelled) {
                        sf::Vector2u windowSize = renderer.getWindow().getSize();
                        float uiWidth = static_cast<float>(windowSize.x);
                        float uiHeight = static_cast<float>(windowSize.y);
                        
                        // Game Over 按钮位置：屏幕中心下方
                        float btnX = uiWidth / 2.0f - 100.f;
                        float btnY = uiHeight / 2.0f + 40.f;
                        float btnW = 200.f;
                        float btnH = 60.f;
                        
                        // 检查鼠标点击是否在按钮范围内（使用屏幕坐标）
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
                    // === NEW: Check Task Clicks ===
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

        // 更新输入（只更一次） 
        inputManager.update();

        // E键检测（移到主循环，非事件轮询内）
        // === NEW: Block interactions if Fainted ===
        if (!isFainted && !waitingForEntranceConfirmation && !dialogSys.isActive() && inputManager.isKeyJustPressed(sf::Keyboard::Key::E)) {
            Logger::debug("E key pressed - checking for interaction");
            if (!gameState.isEating) {
                // 优先检测吧台（counter）交互
                InteractionObject counterObj;
                Professor professor;  // 教授对象（从App.cpp补充）
                
                bool foundCounter = detectInteraction(character, tmjMap.get(), counterObj);
                bool foundProfessor = detectProfessorInteraction(character, tmjMap.get(), professor);  // 教授检测（从App.cpp补充）
                
                Logger::debug("   foundCounter: " + std::to_string(foundCounter));
                Logger::debug("   foundProfessor: " + std::to_string(foundProfessor));
                
                if (foundCounter) {
                    Logger::info("🎯 Triggering Counter interaction - show food select dialog");
                    if (dialogInitSuccess) {
                        dialogSys.setDialog(
                            "What do you want to eat?",  // 对话框标题
                            {"Chicken Steak", "Pasta", "Beef Noodles"}, // 食物选项（匹配贴图名）
                            [&gameState](const std::string& selected) { // 选中回调
                                Logger::error("🔥🔥🔥 FOOD CALLBACK EXECUTED 🔥🔥🔥");
                                Logger::info("🍽️ Selected: " + selected);
                                gameState.selectedFood = selected; // 赋值给游戏状态，供餐桌使用
                                gameState.hasOrderedFood = true; // 标记已点餐
                                Logger::info("Selected food from counter: " + selected);
                            }
                        );
                        renderer.setModalActive(true); // 激活模态（遮挡其他交互）
                    } else {
                        Logger::error("Dialog system not initialized - cannot show food select dialog");
                        renderer.renderModalPrompt("Dialog system not initialized", modalFont, 24, std::nullopt);
                    }
                    continue; // 优先处理吧台，跳过原有逻辑
                }
                // 教授交互部分（从App.cpp补充）
                else if (foundProfessor) {
                    Logger::info("🎓 Triggering Professor interaction - showing dialog");
                    if (dialogInitSuccess) {
                        std::vector<std::string> options;
                        if (professor.dialogType == "lecture") {
                            options = {"Ask about " + professor.course, "Request office hours", "Say hello"};
                        } else {
                            options = {"Talk about studies", "Ask for advice", "Say goodbye"};
                        }
                        
                        std::string greeting = "Hello! I'm " + professor.name + ". How can I help you today?";
                        
                        // 存储教授信息到回应状态
                        profResponseState.professorName = professor.name;
                        profResponseState.professorCourse = professor.course;
                        profResponseState.dialogType = professor.dialogType;
                        
                        dialogSys.setDialogWithIndex(
                            greeting,
                            options,
                            [](int optionIndex, const std::string& optionText) {
                                Logger::info("Player chose option " + std::to_string(optionIndex) + ": " + optionText);
                                
                                // 存储用户选择到回应状态
                                profResponseState.selectedOption = optionIndex;
                                profResponseState.selectedText = optionText;
                                profResponseState.pending = true; // 标记需要显示回应
                            }
                        );
                        renderer.setModalActive(true);
                    }
                    continue;
                }

                // 2. 新增：检测商店触发区域
                ShopTrigger shopTrigger;
                bool foundShop = detectShopTrigger(character, tmjMap.get(), shopTrigger);
                
                Logger::debug("   foundShop: " + std::to_string(foundShop));
                
                if (foundShop) {
                    Logger::info("🛒 Triggering Shop interaction - showing FamilyMart menu");
                    
                    // 只有在商店触发区域内才显示商店菜单
                    DialogSystem* ds = &dialogSys;
                    Renderer* rd = &renderer;
                    auto state = &shoppingState;
                    
                    ds->setDialog(
                        "Welcome to FamilyMart! Which section would you like to browse?",
                        {"Food", "Drink", "Daily Necessities", "Cancel"},
                        [ds, rd, state](const std::string& selected) {
                            Logger::info("🛒 Category Selected: " + selected);
                            
                            if (selected == "Cancel") {
                                state->isShopping = false;
                                return;
                            }
                            
                            // 记录第一层分类
                            state->selectedCategory = selected;
                            
                            // 设置下一步对话请求
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
                            
                            // 让主循环在安全位置处理这个请求
                            rd->setModalActive(true);
                        }
                    );
                    rd->setModalActive(true);
                    continue; // 跳过后续的餐桌检测
                }

                // ⭐⭐⭐⭐ 修复点：在餐桌检测前执行任何待处理的回调 ⭐⭐⭐⭐
                if (dialogSys.hasPendingCallback()) {
                    Logger::info("Executing pending dialog callback before table check");
                    auto cb = dialogSys.consumePendingCallback();
                    if (cb) {
                        cb();
                        Logger::info("Dialog callback executed - food should be selected now");
                    }
                }
                
                // 添加食物选择状态验证
                Logger::info("Food selection status before table check: " + 
                            (gameState.selectedFood.empty() ? "[EMPTY]" : gameState.selectedFood));
        

                // 检测餐桌交互
                TableObject currentTable;
                if (detectTableInteraction(character, tmjMap.get(), currentTable)) {
                    Logger::info("table interaction detected → selected food: " + (gameState.selectedFood.empty() ? "空" : gameState.selectedFood));
                    
                    if (!gameState.hasOrderedFood) {
                        Logger::info("Didn't select food");
                        renderer.renderModalPrompt("Please order food first!", modalFont, 24, std::nullopt);
                    } else {
                        // 1. 验证seatPosition有效性（核心：避免移动到(0,0)）
                        if (currentTable.seatPosition.x == 0 && currentTable.seatPosition.y == 0) {
                            Logger::error("table " + currentTable.name + " has no valid seatPosition");
                            renderer.renderModalPrompt("No valid seatPosition!", modalFont, 24, std::nullopt);
                            continue;
                        }

                        // 2. 解析left/right_table命名，设置朝向
                        Character::Direction facingDir;
                        bool isLeftTable = currentTable.name.find("left_table") != std::string::npos;
                        bool isRightTable = currentTable.name.find("right_table") != std::string::npos;

                        if (isLeftTable) {
                            facingDir  = Character::Direction::Right; // 左桌朝右
                        } else if (isRightTable) {
                            facingDir  = Character::Direction::Left;  // 右桌朝左
                        } else {
                            facingDir  = Character::Direction::Down;  // 兜底默认朝向
                        }

                        // 3. 移动角色到椅子插入点 + 强制设置朝向
                        character.setPosition(currentTable.seatPosition);
                        character.setCurrentDirection(facingDir); // 同步朝向
                        Logger::info("Character has been moved to the seatPosition:(" + std::to_string(currentTable.seatPosition.x) + "," + std::to_string(currentTable.seatPosition.y) + 
                                    ") | direction: " + (isLeftTable ? "right" : "left"));

                        // 4. 激活进食状态
                        gameState.isEating = true;
                        gameState.currentTable = currentTable.name;
                        gameState.eatingProgress = 0.0f;
                        Logger::info("starts eating → table: " + currentTable.name + " | food: " + gameState.selectedFood);
                        gameState.hasOrderedFood = false;
                    }
                    continue;
                }
                //草坪休息触发
                if (isCharacterInLawn(character, tmjMap.get()) && !character.getIsResting()) {
                    character.startResting(); // 进入休息状态
                    // 强制设置角色朝向为「下」
                    character.setCurrentDirection(Character::Direction::Down);
                    Logger::info("Character started resting on lawn (facing down)");
                    // === NEW: Complete Lawn Task ===
                    handleTaskCompletion(taskManager, "rest_lawn");
                    // ===============================
                }
            }
        }

        // 在此处添加游戏触发检测代码
        static bool gameTriggerLocked = false;   // ✅ 防止一帧触发 60 次

        // === NEW: Block Game Triggers if Fainted ===
        GameTriggerArea detectedTrigger;
        if (!isFainted && detectGameTrigger(character, tmjMap.get(), detectedTrigger)) {
            if (!gameTriggerLocked) {
                gameTriggerLocked = true; // ✅ 立刻上锁

                std::cout << "🎮 Game Triggered: " << detectedTrigger.name
                        << " | type = " << detectedTrigger.gameType << std::endl;

                // ✅ 你 Tiled 里写的是 bookstore_puzzle
                if (detectedTrigger.gameType == "bookstore_puzzle") {
                    std::cout << "✅ Launching QuizGame..." << std::endl;

                    QuizGame quizGame;
                    quizGame.run();   // ✅ 正式进入小游戏（阻塞式）

                    std::cout << "✅ QuizGame finished, returning to map." << std::endl;
                    // === NEW: Trigger Task Completion ===
                    handleTaskCompletion(taskManager, "bookstore_quiz");
                    // ===================================
                }
                // 教室问答触发（可配置题库）
                else if (detectedTrigger.gameType == "classroom_quiz") {
                    using json = nlohmann::json;
                    std::string fallbackQid = detectedTrigger.questionSet.empty() ? "classroom_basic" : detectedTrigger.questionSet;
                    std::string selectedQid = fallbackQid;

                    std::string forcedCategory = "";
                    try {
                        Logger::info("Schedule-based quiz selection: current time = " + timeManager.getFormattedTime());
                        std::ifstream schedFile("config/quiz/course_schedule.json");
                        if (schedFile.is_open()) {
                            json schedJson;
                            schedFile >> schedJson;

                            static const char* wkNames[] = {"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};
                            int w = timeManager.getWeekday();
                            std::string wkKey = (w >= 0 && w <= 6) ? wkNames[w] : "Monday";

                            if (schedJson.contains("schedule") && schedJson["schedule"].contains(wkKey)) {
                                const auto& dayArr = schedJson["schedule"][wkKey];
                                int curMin = timeManager.getHour() * 60 + timeManager.getMinute();
                                Logger::info("Weekday key: " + wkKey + ", curMin: " + std::to_string(curMin) + ", entries: " + std::to_string((int)dayArr.size()));

                                for (const auto& item : dayArr) {
                                    if (!item.contains("time") || !item.contains("course")) continue;
                                    std::string timestr = item["time"].get<std::string>();
                                    size_t dash = timestr.find('-');
                                    if (dash == std::string::npos) continue;
                                    std::string left = timestr.substr(0, dash);
                                    std::string right = timestr.substr(dash + 1);
                                    auto trim = [](std::string s) {
                                        size_t a = s.find_first_not_of(" \t\n\r");
                                        size_t b = s.find_last_not_of(" \t\n\r");
                                        if (a == std::string::npos) return std::string();
                                        return s.substr(a, b - a + 1);
                                    };
                                    left = trim(left); right = trim(right);
                                    auto parseHM = [](const std::string& s)->int {
                                        int h = 0, m = 0;
                                        if (sscanf(s.c_str(), "%d:%d", &h, &m) >= 1) return h * 60 + m;
                                        return 0;
                                    };
                                    int startMin = parseHM(left);
                                    int endMin = parseHM(right);
                                    if (startMin <= curMin && curMin <= endMin) {
                                        std::string courseName = item["course"].get<std::string>();
                                        std::string q = courseName;
                                        for (auto &c : q) { if (c == ' ') c = '_'; else c = static_cast<char>(std::tolower(c)); }
                                        std::string candidatePath = std::string("config/quiz/") + q + ".json";
                                        std::ifstream chk(candidatePath);
                                        if (chk.is_open()) {
                                            selectedQid = q;
                                            Logger::info("Selected quiz based on schedule (file): " + courseName + " -> " + selectedQid);
                                        } else {
                                            // If no dedicated file, check inside classroom_basic.json categories for a matching category key
                                            std::ifstream classIfs("config/quiz/classroom_basic.json");
                                            if (classIfs.is_open()) {
                                                nlohmann::json classJ;
                                                classIfs >> classJ;
                                                if (classJ.contains("categories") && classJ["categories"].is_object()) {
                                                    std::string forced = q; // category key candidate
                                                    if (classJ["categories"].contains(forced)) {
                                                        selectedQid = "classroom_basic";
                                                        forcedCategory = forced;
                                                        Logger::info("Selected quiz based on schedule (category in classroom_basic): " + courseName + " -> category=" + forced);
                                                        // store forced category via a temporary JSON key trick by passing forcedCategory later
                                                        // we'll set forcedCategory below outside the try-block
                                                        // mark forcedCategory by writing variable (handled later)
                                                        // To communicate this, set a local variable via outer scope (see after try)
                                                        // We'll set forcedCategory via a placeholder in sched selection scope
                                                        // For now signal via selecting classroom_basic and store forced in a temp variable
                                                    } else {
                                                        Logger::info("No quiz file for course '" + courseName + "' and no category in classroom_basic; using fallback: " + fallbackQid);
                                                    }
                                                } else {
                                                    Logger::info("classroom_basic.json has no categories; using fallback: " + fallbackQid);
                                                }
                                            } else {
                                                Logger::info("No quiz file for course '" + courseName + "' at path: " + candidatePath + "; using fallback: " + fallbackQid);
                                            }
                                        }
                                        break;
                                    }
                                }
                            } else {
                                Logger::info("No schedule entry for current weekday: " + wkKey);
                            }
                        } else {
                            Logger::info("course_schedule.json not found; using fallback quiz id");
                        }
                    } catch (const std::exception& e) {
                        Logger::error(std::string("Error reading schedule json: ") + e.what());
                    }

                    std::string qpath = std::string("config/quiz/") + selectedQid + ".json";
                    Logger::info("Launching Classroom Quiz: " + qpath + " (selectedQid=" + selectedQid + ", forcedCategory=" + forcedCategory + ")");
                    std::cout << "✅ Launching Classroom Quiz (" << qpath << ")..." << std::endl;

                    QuizGame quiz(qpath, forcedCategory);
                    quiz.run();
                }
            }
        } 
        else {
            // ✅ 角色离开触发区后自动解锁（允许下次再玩）
            gameTriggerLocked = false;
        }

        // ========== 商店触发区域自动检测（类似 bookstore quiz game） ==========
        static bool shopTriggerLocked = false;   // ✅ 防止一帧触发 60 次

        ShopTrigger detectedShop;
        if (!isFainted && detectShopTrigger(character, tmjMap.get(), detectedShop)) {
            if (!shopTriggerLocked && !shoppingState.isShopping && !dialogSys.isActive()) {
                shopTriggerLocked = true; // ✅ 立刻上锁

                std::cout << "🛒 Shop Triggered: " << detectedShop.name << std::endl;

                // 自动显示 FamilyMart 对话框
                if (detectedShop.name == "familymart") {
                    Logger::info("🛒 Auto-triggering FamilyMart dialog");
                    
                    DialogSystem* ds = &dialogSys;
                    auto state = &shoppingState;
                    
                    state->isShopping = true;
                    ds->setDialog(
                        "Welcome to FamilyMart! Which section would you like to browse?",
                        {"Food", "Drink", "Daily Necessities", "Cancel"},
                        [ds, state](const std::string& selected) {
                            Logger::info("🛒 Category Selected: " + selected);
                            
                            if (selected == "Cancel") {
                                state->isShopping = false;
                                return;
                            }
                            
                            // 记录第一层分类
                            state->selectedCategory = selected;
                            
                            // 设置下一步对话请求
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
            // ✅ 角色离开触发区后自动解锁（允许下次再触发）
            shopTriggerLocked = false;
        }

        // 角色更新（只更一次，避免重复） 
        // === NEW: Block movement if Fainted ===
        if (!isFainted && !isExpelled && !waitingForEntranceConfirmation && !dialogSys.isActive() && !gameState.isEating) {
            sf::Vector2f moveInput = inputManager.getMoveInput();
            // === NEW: Sprint Feature (Z Key) ===
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
            // ===================================

            // === NEW: Unstuck Failsafe Logic ===
            // Check if player is trying to move but position isn't changing
            if ((moveInput.x != 0.f || moveInput.y != 0.f)) {
                sf::Vector2f currentPos = character.getPosition();
                
                // Calculate distance moved this frame
                float dist = std::sqrt(std::pow(currentPos.x - lastFramePos.x, 2) + 
                                       std::pow(currentPos.y - lastFramePos.y, 2));
                
                if (dist < 0.1f) {
                    stuckTimer += deltaTime;
                    if (stuckTimer > 3.0f) {
                        Logger::warn("⚠️ Character appears stuck! Attempting emergency unstuck...");
                        
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
                                    Logger::info("✅ Unstuck successful! Moved to: " + 
                                        std::to_string(candidate.x) + ", " + std::to_string(candidate.y));
                                    foundSafe = true;
                                    stuckTimer = 0.0f;
                                    break;
                                }
                            }
                        }

                        if (!foundSafe) {
                            Logger::error("❌ Failed to find safe spot. Resetting to spawn.");
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
            // ======================================
        }

        if (character.getIsResting()) {
            taskManager.modifyEnergy(2.0f * deltaTime);
        }
        // ====================================

        // 进食状态更新
        if (gameState.isEating) {
            gameState.eatingProgress += deltaTime * 10;
            Logger::debug("Eating progress: " + std::to_string(gameState.eatingProgress) + "%");

            taskManager.modifyEnergy(3.0f * deltaTime);
            // ===================================

            if (gameState.eatingProgress >= 100.0f) {
                gameState.isEating = false;
                gameState.selectedFood.clear();
                gameState.currentTable.clear();
                gameState.eatingProgress = 0.0f;
                Logger::info("Eating finished - reset state");
                // === NEW: Trigger Task Completion (Bonus Reward) ===
                handleTaskCompletion(taskManager, "eat_food");
                // =================================
            }
        }

        // ========== 重置入口抑制标志 ==========
        if (hasSuppressedEntrance) {
            // 检查角色是否离开了抑制的入口区域
            sf::Vector2f feet = character.getFeetPoint();
            if (!suppressedEntranceRect.contains(feet)) {
                hasSuppressedEntrance = false;
                Logger::info("Character left suppressed entrance area, re-enabling entrance detection");
            }
        }
        // ========== 原有入口检测逻辑（保留） ==========
        // 如果正在显示晕倒提醒或被退学，不显示入口确认对话框
        if (!waitingForEntranceConfirmation && !hasSuppressedEntrance && !showFaintReminder && !isExpelled) {
            EntranceArea detected;
            if (detectEntranceTrigger(character, tmjMap.get(), detected)) {
                waitingForEntranceConfirmation = true;
                pendingEntrance = detected;
                Logger::info("Detected entrance trigger: '" + detected.name + "' target='" + detected.target + "'");
                renderer.setModalActive(true);
            }
        }

        // ========== 入口确认逻辑（原有） ==========
        if (waitingForEntranceConfirmation) {
            // 如果被退学，按 Enter 退出游戏
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
                // 关闭提醒对话框
                if (showFaintReminder) {
                    showFaintReminder = false;
                    faintReminderTimer = 0.0f;
                    continue;
                }
                
                // 如果被退学，按 Escape 退出游戏
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

        // 新增：检查是否达到7天
        if (currentDay > 7 && !isFinalResultShown) {
            FinalResult result = calculateFinalResult(taskManager.getPoints());
            bool shouldExit = showFinalResultScreen(renderer, result);
            if (shouldExit) {
                return AppResult::QuitGame; // 仅处理退出
            }
            isFinalResultShown = true;
        }
        // 新增：检测天数变化（假设TimeManager有获取当前天数的方法）
        if (timeManager.getDay() > currentDay) {
            currentDay = timeManager.getDay();
            Logger::info("Day " + std::to_string(currentDay) + " started");
        }

        // ========== 相机更新（改进：始终以角色为中心，避免弹窗时切换到地图中心） ==========
        renderer.updateCamera(character.getPosition(),
                              tmjMap->getWorldPixelWidth(),
                              tmjMap->getWorldPixelHeight());

        // ========== 渲染逻辑（原有+修复） ==========
        renderer.clear();
        mapLoader.render(&renderer);
        renderer.renderTextObjects(tmjMap->getTextObjects());
        renderer.renderEntranceAreas(tmjMap->getEntranceAreas());
        renderer.renderGameTriggerAreas(tmjMap->getGameTriggers());  // 新增：渲染游戏触发区域
        renderer.renderChefs(tmjMap->getChefs());
        renderer.renderProfessors(tmjMap->getProfessors());  // 教授渲染（从App.cpp补充）
        renderer.renderShopTriggerAreas(tmjMap->getShopTriggers()); // 渲染便利店门口触发区域

        
        // 教授位置调试信息（从App.cpp补充）
        static bool showProfessorDebug = true;
        if (showProfessorDebug) {
            for (const auto& prof : tmjMap->getProfessors()) {
                Logger::debug("📍 Professor '" + prof.name + 
                            "' at: (" + std::to_string((int)prof.rect.position.x) + 
                            ", " + std::to_string((int)prof.rect.position.y) + ")");
            }
            showProfessorDebug = false; // 只显示一次
        }
        
        renderer.drawSprite(character.getSprite());

        //休息状态文本渲染
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
        // ------------------------------------------------
    // ==========================================================
    // === FIXED: UI & OVERLAY RENDER (SCREEN SPACE) ===
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

        // === NEW: Numerical Display on Energy Bar ===
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
            
            // === FIXED SFML 3 CHECK HERE ===
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
            // 绘制全屏黑色覆盖层
            sf::RectangleShape blackOverlay(sf::Vector2f(uiWidth, uiHeight));
            blackOverlay.setPosition(sf::Vector2f(0.f, 0.f));
            blackOverlay.setFillColor(sf::Color::Black);
            renderer.getWindow().draw(blackOverlay);
        }
        
        // --- G. EXPULSION MESSAGE ---
        if (isExpelled) {
            // 绘制半透明背景
            sf::RectangleShape expelBg(sf::Vector2f(uiWidth, uiHeight));
            expelBg.setPosition(sf::Vector2f(0.f, 0.f));
            expelBg.setFillColor(sf::Color(0, 0, 0, 200));
            renderer.getWindow().draw(expelBg);
            
            // 显示退学消息
            sf::Text expelText(modalFont, "Unfortunately, you have fainted too many times\nand have been expelled. Please go home!", 36);
            expelText.setFillColor(sf::Color::Red);
            expelText.setOutlineColor(sf::Color::Black);
            expelText.setOutlineThickness(3);
            
            sf::FloatRect expelBounds = expelText.getLocalBounds();
            expelText.setOrigin(sf::Vector2f(expelBounds.size.x / 2.0f, expelBounds.size.y / 2.0f));
            expelText.setPosition(sf::Vector2f(uiWidth / 2.0f, uiHeight / 2.0f - 60.0f));
            renderer.getWindow().draw(expelText);
            
            // 显示 Game Over 按钮
            sf::RectangleShape gameOverBtn(sf::Vector2f(200.f, 60.f));
            gameOverBtn.setPosition(sf::Vector2f(uiWidth / 2.0f - 100.f, uiHeight / 2.0f + 40.f));
            
            // 检查鼠标是否在按钮上
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
            
            // 按钮文字
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

        // === NEW: Achievement Popup ===
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

// 修复日志记录中的字段访问问题
void showShopDialog(const ShopTrigger& shop) {
    Logger::info("Displaying shop dialog for: " + shop.name);
    Logger::info("Shop rect: (" + std::to_string(shop.rect.position.x) + ", " + std::to_string(shop.rect.position.y) + ") " +
                 std::to_string(shop.rect.size.x) + "x" + std::to_string(shop.rect.size.y));
    // 示例实现：显示商店对话框的逻辑
    std::cout << "Welcome to " << shop.name << "!" << std::endl;
}
