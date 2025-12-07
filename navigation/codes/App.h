#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include "MapLoader/MapObjects.h" // 确保 ShopTrigger 类型可用

class Renderer;
class MapLoader;
class TMJMap;
class Character;
class ConfigManager;

/*
 * File: App.h
 * Description: Declaration for the application run loop module.
 *
 * The runApp function encapsulates the main game loop so that main.cpp only
 * needs to perform initialization and cleanup. It takes references to the key
 * subsystems and the currently loaded TMJMap instance.
 */

namespace sf {
    class View;
}

// What the main app loop decided to do when it finishes.
enum class AppResult {
    QuitGame,
    BackToLogin,
    ShowFinalResult // 新增：触发结算界面
};

enum class Grade {
    A, B, C, D, F
};

struct FinalResult {
    int totalPoints;
    Grade grade;
    int starCount; // 1-5
};

enum class FinalResultAction {
    Exit,    // 退出游戏
};

// Run the main game loop (one "day" on campus)
AppResult runApp(
    Renderer& renderer,
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    Character& character,
    sf::View& view,
    ConfigManager& configManager
);

// 在 App.h 中添加 showShopDialog 的声明
void showShopDialog(const ShopTrigger& shop);
