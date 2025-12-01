// Character.h
#pragma once

// SFML types used for sprites and geometry.
#include <SFML/Graphics.hpp>
#include <memory>
#include "CharacterConfig.h"

/*
 * File: Character.h
 * Description: Player character class and supporting declarations.
 *
 * This header defines the Character class which encapsulates player sprite,
 * animation, movement and collision handling. The implementation relies on
 * CharacterConfig for tunable parameters.
 *
 * Notes:
 *   - Character expects a valid CharacterConfig to be available via the
 *     CharacterConfigManager when initialize() is called.
 */

// Forward declaration to avoid including TMJMap.h in this header.
class TMJMap;

/**
 * @class Character
 * @brief Manages the player's sprite, animation and movement logic.
 *
 * The Character class owns its texture and sprite and exposes an update
 * method used by the main loop. Collision checking may query a TMJMap
 * instance via a pointer passed to update().
 */
class Character {
public:
    enum class Direction { Down = 0, Left = 1, Right = 2, Up = 3 };
    
    Character();
    ~Character();
    
    /**
     * @brief Initialize the character (load texture, create sprite).
     * @return true on success, false otherwise.
     */
    bool initialize();

    /**
     * @brief Release owned resources and reset internal state.
     */
    void cleanup();
    
    /**
     * @brief Update the character state for the current frame.
     * @param deltaTime Frame time in seconds.
     * @param moveInput Movement input vector.
     * @param mapWidth Map width in pixels (for boundary checks).
     * @param mapHeight Map height in pixels (for boundary checks).
     */
    // Modified: accepts an optional TMJMap pointer for map collision queries (may be nullptr)
    void update(float deltaTime, const sf::Vector2f& moveInput, int mapWidth, int mapHeight, const TMJMap* map);
    
    /**
     * @brief Get the character's sprite.
     */
    const sf::Sprite& getSprite() const { return *sprite; }
    
    /**
     * @brief Set the character position.
     */
    void setPosition(const sf::Vector2f& position);
    
    /**
     * @brief Get the character position.
     */
    sf::Vector2f getPosition() const { return sprite->getPosition(); }
    
    /**
     * @brief Get the character's feet point (used for map collision/trigger checks).
     */
    sf::Vector2f getFeetPoint() const;

    /**
     * @brief Get collision bounds for the character.
     */
    sf::FloatRect getBounds() const;
    
    /**
     * @brief Check whether the character is currently moving.
     */
    bool isMoving() const { return moving; }
    
    /**
     * @brief Get the current facing direction.
     */
    Direction getCurrentDirection() const { return currentDirection; }
    
    /**
     * @brief Reload configuration and reinitialize resources as needed.
     */
    void reloadConfig();

    /**
     * @brief Check whether the character has been initialized.
     */
    bool isInitialized() const { return sprite != nullptr; }

    // 新增：设置朝向
    void setCurrentDirection(Direction dir);

    // 添加状态控制方法
    bool getIsResting() const { return isResting; }
    void startResting() { 
        isResting = true; 
        restTimer = 0.0f; 
        moving = false; // 强制停止移动
    }
    void stopResting();

    // 设置休息状态
    void setResting(bool resting) { 
        isResting = resting; 
        if (!resting) {
            // 结束休息时重置透明度
            sprite->setColor(sf::Color(255, 255, 255, 255)); // 白色+完全不透明
            flashTimer = 0.0f;
            flashState = false;
        }
    }

private:
    /**
     * @brief Load texture from disk into memory.
     */
    bool loadTexture();
    
    /**
     * @brief Create the SFML sprite from the loaded texture.
     */
    bool createSprite();
    
    /**
     * @brief Set the current animation frame rectangle on the sprite.
     */
    void setAnimationFrame(int frameRow, int directionCol);
    
    /**
     * @brief Update animation timer and frame selection.
     */
    void updateAnimation(float deltaTime);
    
    /**
     * @brief Handle movement, boundary clamping and simple collision response.
     * @param map Optional TMJMap pointer used for NotWalkable collision checks.
     */
    void handleMovement(float deltaTime, const sf::Vector2f& moveInput, int mapWidth, int mapHeight, const TMJMap* map);
    
private:
    std::unique_ptr<sf::Sprite> sprite;
    std::unique_ptr<sf::Texture> texture;
    CharacterConfig config;
    
    // Animation state
    int currentFrameRow = 1;
    Direction currentDirection = Direction::Down;
    bool moving = false;
    float animationTimer = 0.0f;
    
    // Calculated collision extents
    float collisionHalfWidth = 0.0f;
    float collisionHalfHeight = 0.0f;

    // 休息状态相关成员变量
    bool isResting = false;          // 是否处于休息状态
    float restTimer = 0.0f;          // 休息计时器
    const float REST_DURATION = 5.0f; // 休息持续时间（5秒）
    float flashTimer = 0.0f;       // 闪烁计时器
    bool flashState = false;       // 闪烁状态（true为半透明，false为不透明）
    float flashInterval = 0.3f;    // 闪烁间隔（秒）
};

