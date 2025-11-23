// CharacterConfig.h
#pragma once

#include <string>  // Standard library includes
#include <SFML/Graphics.hpp>  // External library includes

/**
 * @file CharacterConfig.h
 * @brief Configuration structures and manager for the player character.
 *
 * This file provides:
 * - CharacterConfig: a POD storing animation, movement, collision and asset settings.
 * - CharacterConfigManager: a singleton that loads/saves CharacterConfig from JSON.
 *
 * Dependencies:
 * - nlohmann/json for serialization in the implementation file.
 * - SFML for any types used by other modules (kept for consistency).
 *
 * Notes:
 * - The manager uses a default base path of "./config/" and default filenames.
 * - Caller should handle errors returned by load/save methods.
 */

/**
 * @struct CharacterConfig
 * @brief Runtime configuration for the player character.
 *
 * Fields are intentionally public to keep the structure simple and POD-like.
 * Default values are chosen to match the sprite sheet used in the project.
 */
struct CharacterConfig {
    // Path to the character texture (relative to the executable).
    std::string texturePath = "tiles/F_01.png";

    // Animation frame dimensions in pixels.
    int frameWidth = 16;
    int frameHeight = 16;
    // Number of columns in the sprite sheet that represent directions.
    int directionColumns = 4;
    // Number of animation rows (frames) per direction.
    int frameRows = 3;
    // Vertical spacing between rows in the sprite sheet (pixels).
    int rowSpacing = 1;

    // Mapping from internal Direction enum to texture column indices.
    // Order corresponds to {Down, Left, Right, Up} by convention.
    int directionMapping[4] = {0, 3, 1, 2};

    // Movement and animation timing parameters.
    float moveSpeed = 75.0f;         // pixels per second
    float animationInterval = 0.15f; // seconds per frame
    float scale = 1.0f;              // global scale factor for the sprite

    // Optional collision offset to shrink/expand collision region.
    float collisionOffsetX = 0.0f;
    float collisionOffsetY = 0.0f;
};

/**
 * @class CharacterConfigManager
 * @brief Singleton responsible for loading and saving CharacterConfig.
 *
 * The manager provides a single shared configuration instance that other
 * systems (such as Character) query during initialization.
 *
 * Thread-safety: This class is not thread-safe and is intended for use on the
 * main thread only.
 */
class CharacterConfigManager {
public:
    /**
     * @brief Returns the singleton instance.
     * @return Reference to the global CharacterConfigManager.
     */
    static CharacterConfigManager& getInstance();

    /**
     * @brief Loads configuration from a JSON file.
     * @param configPath Filename under the manager base path (default).
     * @return true if load succeeded, false on error (defaults remain).
     */
    bool loadConfig(const std::string& configPath = "character_config.json");

    /**
     * @brief Saves current configuration to a JSON file.
     * @param configPath Filename under the manager base path (default).
     * @return true if save succeeded, false on error.
     */
    bool saveConfig(const std::string& configPath = "character_config.json");

    /**
     * @brief Returns reference to the loaded configuration.
     */
    const CharacterConfig& getConfig() const { return config; }

    /**
     * @brief Replaces the current configuration with a new one.
     */
    void updateConfig(const CharacterConfig& newConfig);

private:
    // Private constructor for singleton pattern.
    CharacterConfigManager() = default;

    // Stored configuration instance.
    CharacterConfig config;
    // Base directory used by load/save operations.
    std::string basePath = "./config/";
};