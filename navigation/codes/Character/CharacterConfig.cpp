// CharacterConfig.cpp
#include <fstream>
#include <nlohmann/json.hpp>
#include "CharacterConfig.h"
#include "Utils/Logger.h"

/**
 * @file CharacterConfig.cpp
 * @brief Implements CharacterConfigManager for loading and saving character settings.
 *
 * This file contains the concrete implementation of the CharacterConfigManager
 * singleton. The manager reads/writes CharacterConfig from JSON files under a
 * configurable base path. It provides:
 * - loadConfig(): parse JSON into CharacterConfig with safe defaults on failure.
 * - saveConfig(): serialize CharacterConfig to JSON on disk.
 * - updateConfig(): replace in-memory configuration.
 *
 * Dependencies:
 * - nlohmann::json for JSON parsing and serialization.
 * - Utils::Logger for logging status and errors.
 *
 * Notes:
 * - On parse or IO errors the manager logs and returns failure, leaving defaults intact.
 */

// Alias for json library used for serialization.
using json = nlohmann::json;

/**
 * Returns the singleton instance of CharacterConfigManager.
 *
 * This function implements the Meyers singleton pattern. The instance is
 * created on first use and is destroyed automatically at program exit.
 */
CharacterConfigManager& CharacterConfigManager::getInstance() {
    static CharacterConfigManager instance;
    return instance;
}

/**
 * Loads the character configuration from JSON file located under basePath.
 *
 * @param configPath Filename relative to the manager basePath.
 * @return true if the file was found and parsed successfully, false otherwise.
 *
 * Note: On failure the manager keeps the default values already present in
 * its internal CharacterConfig.
 */
bool CharacterConfigManager::loadConfig(const std::string& configPath) {
    std::string fullPath = basePath + configPath;
    std::ifstream file(fullPath);

    if (!file.is_open()) {
        Logger::warn("Character config file not found: " + fullPath + ", using defaults");
        return false;
    }

    try {
        json j;
        file >> j;

        // Texture and animation parameters
        config.texturePath = j.value("texturePath", "tiles/F_01.png");
        config.frameWidth = j.value("frameWidth", 16);
        config.frameHeight = j.value("frameHeight", 16);
        config.directionColumns = j.value("directionColumns", 4);
        config.frameRows = j.value("frameRows", 3);
        config.rowSpacing = j.value("rowSpacing", 1);

        // Direction mapping (optional array)
        if (j.contains("directionMapping") && j["directionMapping"].is_array()) {
            auto mapping = j["directionMapping"];
            for (size_t i = 0; i < 4 && i < mapping.size(); ++i) {
                config.directionMapping[i] = mapping[i];
            }
        }

        // Movement and animation timing
        config.moveSpeed = j.value("moveSpeed", 75.0f);
        config.animationInterval = j.value("animationInterval", 0.15f);
        config.scale = j.value("scale", 1.0f);

        // Collision offsets
        config.collisionOffsetX = j.value("collisionOffsetX", 0.0f);
        config.collisionOffsetY = j.value("collisionOffsetY", 0.0f);

        Logger::info("Character config loaded successfully");
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to parse character config: " + std::string(e.what()));
        return false;
    }
}

/**
 * Serializes and writes the current CharacterConfig to disk.
 *
 * @param configPath Filename relative to the manager basePath.
 * @return true on success, false on any IO or serialization error.
 */
bool CharacterConfigManager::saveConfig(const std::string& configPath) {
    std::string fullPath = basePath + configPath;
    std::ofstream file(fullPath);

    if (!file.is_open()) {
        Logger::error("Cannot open character config file for writing: " + fullPath);
        return false;
    }

    try {
        json j;

        // Texture and animation parameters
        j["texturePath"] = config.texturePath;
        j["frameWidth"] = config.frameWidth;
        j["frameHeight"] = config.frameHeight;
        j["directionColumns"] = config.directionColumns;
        j["frameRows"] = config.frameRows;
        j["rowSpacing"] = config.rowSpacing;

        // Direction mapping
        j["directionMapping"] = json::array();
        for (int i = 0; i < 4; ++i) {
            j["directionMapping"].push_back(config.directionMapping[i]);
        }

        // Movement and animation parameters
        j["moveSpeed"] = config.moveSpeed;
        j["animationInterval"] = config.animationInterval;
        j["scale"] = config.scale;

        // Collision parameters
        j["collisionOffsetX"] = config.collisionOffsetX;
        j["collisionOffsetY"] = config.collisionOffsetY;

        file << j.dump(4);
        Logger::info("Character config saved successfully");
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to save character config: " + std::string(e.what()));
        return false;
    }
}

/**
 * Replaces internal configuration with a provided instance.
 *
 * This function performs a simple assignment; no validation is performed.
 */
void CharacterConfigManager::updateConfig(const CharacterConfig& newConfig) {
    config = newConfig;
}