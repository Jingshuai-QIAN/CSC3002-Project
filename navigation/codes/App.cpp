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

// 在detectEntranceTrigger函数下方添加
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

// Helper: show the full-map modal (blocking) — extracted from main loop.
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
        while (auto ev = mapWin.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) { mapWin.close(); break; }
            if (ev->is<sf::Event::KeyPressed>()) {
                if (ev->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape) {
                    mapWin.close(); break;
                }
            }

            if (ev->is<sf::Event::MouseWheelScrolled>()) {
                auto mw = ev->getIf<sf::Event::MouseWheelScrolled>();
                if (mw) {
                    float delta = mw->delta;
                    if (delta > 0) zoomFactor *= 1.1f;
                    else if (delta < 0) zoomFactor /= 1.1f;
                    zoomFactor = std::clamp(zoomFactor, ZOOM_MIN, ZOOM_MAX);
                    view.setSize(sf::Vector2f{
                        mapWf / zoomFactor,
                        mapHf / zoomFactor
                    });
                    mapWin.setView(view);
                }
            }

            if (ev->is<sf::Event::MouseButtonPressed>()) {
                auto mb = ev->getIf<sf::Event::MouseButtonPressed>();
                if (mb && mb->button == sf::Mouse::Button::Left) {
                    dragging = true;
                    prevDragPixel = mb->position;
                }
            }
            if (ev->is<sf::Event::MouseButtonReleased>()) {
                auto mr = ev->getIf<sf::Event::MouseButtonReleased>();
                if (mr && mr->button == sf::Mouse::Button::Left) {
                    dragging = false;
                }
            }
            if (ev->is<sf::Event::MouseMoved>()) {
                auto mm = ev->getIf<sf::Event::MouseMoved>();
                if (mm && dragging) {
                    sf::Vector2i curPixel = mm->position;
                    sf::Vector2f prevWorld = mapWin.mapPixelToCoords(prevDragPixel);
                    sf::Vector2f curWorld  = mapWin.mapPixelToCoords(curPixel);
                    sf::Vector2f diff = prevWorld - curWorld;
                    view.setCenter(view.getCenter() + diff);
                    mapWin.setView(view);
                    prevDragPixel = curPixel;
                }
            }
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

void runApp(
    Renderer& renderer,
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    Character& character,
    sf::View& view,
    ConfigManager& configManager
) {
    auto& inputManager = InputManager::getInstance();

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
    
    // ========== 修复1：只初始化一次对话框（避免重复加载） ==========
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

    // 入口确认状态
    bool waitingForEntranceConfirmation = false;
    EntranceArea pendingEntrance;
    bool hasSuppressedEntrance = false;
    sf::FloatRect suppressedEntranceRect;

    // 主循环
    sf::Clock clock;
    while (renderer.isRunning()) {
        float deltaTime = clock.restart().asSeconds();

        // ========== 修复2：统一事件处理（只轮询一次） ==========
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

        // ========== 修复3：更新输入（只更一次） ==========
        inputManager.update();

        // ========== 修复4：E键检测（移到主循环，非事件轮询内） ==========
        if (!waitingForEntranceConfirmation && !dialogSys.isActive() && inputManager.isKeyJustPressed(sf::Keyboard::Key::E)) {
            Logger::debug("E key pressed - checking for interaction");
            InteractionObject interactObj;
            if (detectInteraction(character, tmjMap.get(), interactObj)) {
                if (dialogInitSuccess) {
                    Logger::info("Triggering Counter interaction - showing dialog");
                    // 设置对话框内容
                    dialogSys.setDialog(
                        "What would you want to eat?",
                        interactObj.options.empty() ? std::vector<std::string>{"Chicken Steak", "Pasta", "Beef Noodles"} : interactObj.options,
                        [](const std::string& dish) {
                            Logger::info("Selected dish: " + dish);
                        }
                    );
                    renderer.setModalActive(true);
                } else {
                    Logger::error("Dialog system not initialized - cannot show interaction");
                }
            } else {
                Logger::debug("E key pressed but no valid interaction found");
            }
        }

        // 在此处添加游戏触发检测代码
        static bool gameTriggerLocked = false;   // ✅ 防止一帧触发 60 次

        GameTriggerArea detectedTrigger;
        if (detectGameTrigger(character, tmjMap.get(), detectedTrigger)) {
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
            }
        } 
        else {
            // ✅ 角色离开触发区后自动解锁（允许下次再玩）
            gameTriggerLocked = false;
        }

        // ========== 修复5：角色更新（只更一次，避免重复） ==========
        if (!waitingForEntranceConfirmation && !dialogSys.isActive()) {
            sf::Vector2f moveInput = inputManager.getMoveInput();
            character.update(deltaTime, moveInput, 
                            tmjMap->getWorldPixelWidth(), 
                            tmjMap->getWorldPixelHeight(),
                            tmjMap.get());
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
                    mapLoader.setSpawnOverride(fromKey, character.getPosition().x, character.getPosition().y);
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

        // ========== 相机更新（原有） ==========
        if (!waitingForEntranceConfirmation) {
            renderer.updateCamera(character.getPosition(),
                                  tmjMap->getWorldPixelWidth(),
                                  tmjMap->getWorldPixelHeight());
        } else {
            renderer.updateCamera(view.getCenter(),
                                  tmjMap->getWorldPixelWidth(),
                                  tmjMap->getWorldPixelHeight());
        }

        // ========== 渲染逻辑（原有+修复） ==========
        renderer.clear();
        mapLoader.render(&renderer);
        renderer.renderTextObjects(tmjMap->getTextObjects());
        renderer.renderEntranceAreas(tmjMap->getEntranceAreas());
        renderer.renderGameTriggerAreas(tmjMap->getGameTriggers());  // 新增：渲染游戏触发区域
        renderer.renderChefs(tmjMap->getChefs());
        renderer.drawSprite(character.getSprite());
        renderer.drawMapButton();

        // 绘制入口确认提示
        if (waitingForEntranceConfirmation) {
            std::string prompt = "Do you want to enter " + pendingEntrance.name + "?  Enter=Yes  Esc=No";
            renderer.renderModalPrompt(prompt, modalFont, configManager.getRenderConfig().text.fontSize);
        }

        // 绘制对话框
        if (dialogSys.isActive()) {
            dialogSys.render(renderer.getWindow());
            if (!dialogSys.isActive()) {
                renderer.setModalActive(false);
            }
        }

        renderer.present();
    }
}