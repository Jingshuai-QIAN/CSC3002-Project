#pragma once

#include <SFML/Graphics.hpp>
#include <memory>

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
void runApp(
    Renderer& renderer,
    MapLoader& mapLoader,
    std::shared_ptr<TMJMap>& tmjMap,
    Character& character,
    sf::View& view,
    ConfigManager& configManager
);
