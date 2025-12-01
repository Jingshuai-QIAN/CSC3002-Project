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
// Added in New Script:
#include "Manager/TimeManager.h"
#include "Manager/TaskManager.h"


// Helper: detect whether character is inside an entrance and facing it.
static bool detectEntranceTrigger(const Character& character, const TMJMap* map, EntranceArea& outArea) {
    if (!map) return false;
    sf::Vector2f feet = character.getFeetPoint();
    for (const auto& a : map->getEntranceAreas()) {
        sf::FloatRect rect(sf::Vector2f(a.x, a.y), sf::Vector2f(a.width, a.height));
        if (!rect.contains(feet)) continue;
        sf::Vector2f center(a.x + a.width * 0.5f, a.y + a.height * 0.5f);
        sf::Vector2f v = center - feet;
        Character::Direction desired;
        if (std::abs(v.x) > std::abs(v.y)) {
            desired = v.x > 0 ? Character::Direction::Right : Character::Direction::Left;
        } else {
            desired = v.y > 0 ? Character::Direction::Down : Character::Direction::Up;
        }
        if (desired == character.getCurrentDirection()) {
            outArea = a;
            return true;
        }
    }
    return false;
}

static bool detectGameTrigger(const Character& character, const TMJMap* map, GameTriggerArea& outArea) {
    if (!map) return false;
    sf::Vector2f feet = character.getFeetPoint(); // 角色脚部位置（现有逻辑）
    for (const auto& gta : map->getGameTriggers()) {
        // 检测角色是否在触发区域内
        sf::FloatRect rect(sf::Vector2f(gta.x, gta.y), sf::Vector2f(gta.width, gta.height));
        if (rect.contains(feet)) {
            outArea = gta;
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

void runApp(
    Renderer& renderer,
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    Character& character,
    sf::View& view,
    ConfigManager& configManager
) {
    auto& inputManager = InputManager::getInstance();
    // === NEW: Initialize Systems (Time & Tasks) ===
    TimeManager timeManager;
    TaskManager taskManager;

    // Load initial tasks (Updated to separate Energy Logic from Task Logic)
    // Energy reward is now small (Bonus), actual restoration happens during the action.
    taskManager.addTask("eat_food", "Eat Food at Canteen", 5, 2); 
    taskManager.addTask("attend_class", "Attend Class (Quiz)", 20, 5); // Completion bonus
    taskManager.addTask("rest_lawn", "Rest on Lawn", 10, 2);
    // =============================================
    if (!renderer.initializeChefTexture()) {
        Logger::error("Failed to initialize chef texture");
        return;
    }
    
    // 加载模态字体
    sf::Font modalFont;
    if (!modalFont.openFromFile(configManager.getRenderConfig().text.fontPath)) {
        Logger::error("Failed to load modal font!");
        return;
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
        std::string currentTable;
        std::string selectedFood;
        float eatingProgress = 0.0f;
    };
    GameState gameState;

    // === NEW: Fainting State ===
    bool isFainted = false;
    float faintTimer = 0.0f;
    // ==========================

    // 入口确认状态
    bool waitingForEntranceConfirmation = false;
    EntranceArea pendingEntrance;
    bool hasSuppressedEntrance = false;
    sf::FloatRect suppressedEntranceRect;

    // 主循环
    sf::Clock clock;
    while (renderer.isRunning()) {
        float deltaTime = clock.restart().asSeconds();
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        timeManager.update(deltaTime);

        // === NEW: Passive Energy Depletion ===
        // Goal: 5 Energy per Game Hour. 
        // 1 Real Sec = 2 Game Mins. 1 Game Hour = 30 Real Seconds.
        // Rate = 5 / 30 = ~0.1666 energy per second.
        const float PASSIVE_DEPLETION_RATE = 5.0f / 30.0f;
        taskManager.modifyEnergy(-PASSIVE_DEPLETION_RATE * deltaTime);
        // =====================================

        // === NEW: Fainting Logic Check ===
        // Must not be currently eating/interacting/fainted
        if (!isFainted && !gameState.isEating && !dialogSys.isActive()) {
            if (taskManager.getEnergy() <= 0) {
                isFainted = true;
                faintTimer = 0.0f;
                // Force character direction Up (Visual for passing out)
                character.setCurrentDirection(Character::Direction::Up);
                Logger::info("Character passed out due to lack of energy!");
            }
        }
        
        // Handle Faint Timer
        if (isFainted) {
            faintTimer += deltaTime;
            if (faintTimer > 6.0f) { // Pass out for 6 seconds
                isFainted = false;
                taskManager.modifyEnergy(5.0f); // Restore 5 Energy
                Logger::info("Character woke up.");
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

            // 窗口关闭事件
            if (event.is<sf::Event::Closed>()) {
                renderer.quit();
                break;
            }

            // 全屏地图按钮（原有逻辑）
            if (event.is<sf::Event::MouseButtonPressed>()) {
                auto mb = event.getIf<sf::Event::MouseButtonPressed>();
                if (mb && mb->button == sf::Mouse::Button::Left) {
                    sf::Vector2i mpos = mb->position;
                    if (renderer.mapButtonContainsPoint(mpos)) {
                        showFullMapModal(renderer, tmjMap, configManager);
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
                if (detectInteraction(character, tmjMap.get(), counterObj)) {
                    Logger::info("Detected counter (吧台) interaction - show food select dialog");
                    // 触发吧台选餐对话框
                    if (dialogInitSuccess) {
                        dialogSys.setDialog(
                            "What do you want to eat?",  // 对话框标题
                            {"Chicken Steak", "Pasta", "Beef Noodles"}, // 食物选项（匹配贴图名）
                            [&gameState](const std::string& selected) { // 选中回调
                                gameState.selectedFood = selected; // 赋值给游戏状态，供餐桌使用
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

                // 检测餐桌交互
                TableObject currentTable;
                if (detectTableInteraction(character, tmjMap.get(), currentTable)) {
                    Logger::info("table interaction detected → selected food: " + (gameState.selectedFood.empty() ? "空" : gameState.selectedFood));
                    
                    if (gameState.selectedFood.empty()) {
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
                    }
                    continue;
                }
                //草坪休息触发
                if (isCharacterInLawn(character, tmjMap.get()) && !character.getIsResting()) {
                    character.startResting(); // 进入休息状态
                    // 强制设置角色朝向为「下」
                    character.setCurrentDirection(Character::Direction::Down);
                    Logger::info("Character started resting on lawn (facing down)");
                    // === NEW: Completed Task (Reward is Bonus) ===
                    taskManager.completeTask("rest_lawn");
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
                }
                // 教室问答触发（可配置题库）
                else if (detectedTrigger.gameType == "classroom_quiz") {
                    std::string qid = detectedTrigger.questionSet.empty() ? "classroom_basic" : detectedTrigger.questionSet;
                    std::string qpath = std::string("config/quiz/") + qid + ".json";
                    std::cout << "✅ Launching Classroom Quiz (" << qpath << ")..." << std::endl;

                    QuizGame quiz(qpath);
                    quiz.run();

                    std::cout << "✅ Classroom Quiz finished, moving character out of trigger." << std::endl;
                    // Log the exp/energy effects so teammates can consume them
                    {
                        QuizGame::Effects eff = quiz.getResultEffects();
                        Logger::info("Classroom quiz effects -> exp: " + std::to_string(eff.exp) +
                                     " energy: " + std::to_string(eff.energy));
                    }
                    
                    // === NEW: Apply Energy Cost for Class ===
                    // Class makes you tired (-20 Energy)
                    taskManager.modifyEnergy(-20.0f); 
                    // ========================================

                    // 将角色移出触发区一格（按 tile 大小）
                    float tileW = tmjMap->getTileWidth();
                    float tileH = tmjMap->getTileHeight();
                    sf::FloatRect area(
                        sf::Vector2f(detectedTrigger.x, detectedTrigger.y),
                        sf::Vector2f(detectedTrigger.width, detectedTrigger.height)
                    );
                    sf::Vector2f feet = character.getFeetPoint();
                    sf::Vector2f center(area.position.x + area.size.x * 0.5f,
                                        area.position.y + area.size.y * 0.5f);
                    sf::Vector2f newPos = character.getPosition(); // sprite 中心

                    if (std::abs(feet.x - center.x) > std::abs(feet.y - center.y)) {
                        // 水平进入 -> 向水平方向移出
                        if (feet.x < center.x) newPos.x = area.position.x - tileW * 0.6f;
                        else newPos.x = area.position.x + area.size.x + tileW * 0.6f;
                    } else {
                        // 垂直进入 -> 向垂直方向移出
                        if (feet.y < center.y) newPos.y = area.position.y - tileH * 0.6f;
                        else newPos.y = area.position.y + area.size.y + tileH * 0.6f;
                    }

                    // Clamp 到地图范围
                    float mapW = static_cast<float>(tmjMap->getWorldPixelWidth());
                    float mapH = static_cast<float>(tmjMap->getWorldPixelHeight());
                    newPos.x = std::clamp(newPos.x, 0.0f, mapW);
                    newPos.y = std::clamp(newPos.y, 0.0f, mapH);

                    character.setPosition(newPos);
                    // === NEW: Task Completion Hook ===
                    taskManager.completeTask("attend_class");
                    // =================================
                }
            }
        } 
        else {
            // ✅ 角色离开触发区后自动解锁（允许下次再玩）
            gameTriggerLocked = false;
        }

        // 角色更新（只更一次，避免重复） 
        // === NEW: Block movement if Fainted ===
        if (!isFainted && !waitingForEntranceConfirmation && !dialogSys.isActive() && !gameState.isEating) {
            sf::Vector2f moveInput = inputManager.getMoveInput();
            // === NEW: Sprint Feature (Z Key) ===
            float speedMultiplier = 1.0f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z)) {
                speedMultiplier = 2.0f; // Walk 2x faster
            }
            // Pass the modified deltaTime to make character move faster
            character.update(deltaTime * speedMultiplier, moveInput, 
                            tmjMap->getWorldPixelWidth(), 
                            tmjMap->getWorldPixelHeight(),
                            tmjMap.get());
            // ===================================
        }
        
        // === NEW: Resting Energy Recovery (Nerfed) ===
        if (character.getIsResting()) {
            // Resting restores 10 Energy Total over 5 seconds (Duration defined in Character.h)
            // Rate = 10 / 5 = 2.0 energy per second.
            taskManager.modifyEnergy(2.0f * deltaTime);
        }
        // ====================================

        // 进食状态更新
        if (gameState.isEating) {
            // Eating Speed: Progress increases by 10 per second -> 10 seconds total.
            gameState.eatingProgress += deltaTime * 10;
            Logger::debug("Eating progress: " + std::to_string(gameState.eatingProgress) + "%");
            
            // === NEW: Eating Energy Recovery (Nerfed) ===
            // Target: 30 Energy Total. Duration: 10 seconds.
            // Rate = 30 / 10 = 3.0 energy per second.
            taskManager.modifyEnergy(3.0f * deltaTime);
            // ===================================

            if (gameState.eatingProgress >= 100.0f) {
                gameState.isEating = false;
                gameState.selectedFood.clear();
                gameState.currentTable.clear();
                gameState.eatingProgress = 0.0f;
                Logger::info("Eating finished - reset state");
                // === NEW: Task Completion Hook (Bonus Reward) ===
                taskManager.completeTask("eat_food");
                // =================================
            }
        }

        // ========== 原有入口检测逻辑（保留） ==========
        if (!waitingForEntranceConfirmation && !hasSuppressedEntrance) {
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
                cancelEntranceMove(character, *tmjMap);
                waitingForEntranceConfirmation = false;
                renderer.setModalActive(false);
            }
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

        // === NEW: EXP BAR (Purple, below Energy Bar) ===
        sf::RectangleShape expBarBg(sf::Vector2f(200.f, 10.f));
        expBarBg.setPosition(sf::Vector2f(20.f, 85.f)); // Below Energy Bar
        expBarBg.setFillColor(sf::Color(50, 50, 50));
        expBarBg.setOutlineThickness(1);
        expBarBg.setOutlineColor(sf::Color::White);
        
        // Calculate ratio
        float expPct = static_cast<float>(taskManager.getExp()) / static_cast<float>(taskManager.getMaxExp());
        if (expPct > 1.0f) expPct = 1.0f;

        sf::RectangleShape expBarFg(sf::Vector2f(200.f * expPct, 10.f));
        expBarFg.setPosition(sf::Vector2f(20.f, 85.f));
        expBarFg.setFillColor(sf::Color(150, 0, 255)); // Purple

        renderer.getWindow().draw(expBarBg);
        renderer.getWindow().draw(expBarFg);
        // ===============================================

        // --- D. TASK LIST ---
        float taskY = 110.f;
        sf::Text taskHeader(modalFont, "Tasks:", 20);
        taskHeader.setPosition(sf::Vector2f(20.f, taskY));
        taskHeader.setFillColor(sf::Color::Cyan);
        taskHeader.setOutlineColor(sf::Color::Black);
        taskHeader.setOutlineThickness(1);
        renderer.getWindow().draw(taskHeader);
        
        taskY += 30.f;
        for (const auto& t : taskManager.getTasks()) {
            if (!t.isCompleted) {
                sf::Text taskText(modalFont, "- " + t.description, 18);
                taskText.setPosition(sf::Vector2f(25.f, taskY));
                taskText.setFillColor(sf::Color::White);
                taskText.setOutlineColor(sf::Color::Black);
                taskText.setOutlineThickness(1);
                renderer.getWindow().draw(taskText);
                taskY += 25.f;
            }
        }
        
        // --- E. FAINTED TEXT ---
        if (isFainted) {
            sf::Text faintText(modalFont, "You passed out due to exhaustion...", 30);
            faintText.setFillColor(sf::Color::Red);
            faintText.setOutlineColor(sf::Color::Black);
            faintText.setOutlineThickness(2);
            
            // Center on screen
            sf::FloatRect bounds = faintText.getLocalBounds();
            faintText.setOrigin(sf::Vector2f(bounds.size.x / 2.0f, bounds.size.y / 2.0f));
            faintText.setPosition(sf::Vector2f(uiWidth / 2.0f, uiHeight / 2.0f));
            
            renderer.getWindow().draw(faintText);
        }
        // =======================
        
        // 3. Restore the Game Camera (So the next frame renders the map correctly)
        renderer.getWindow().setView(gameView);
        
        // ==========================================================

        renderer.drawMapButton();
        
        // ... [Rest of your rendering code: modal prompts, dialogs, present()] ...

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
}
