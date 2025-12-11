// Character.cpp
#include "Character.h"
#include "Utils/Logger.h"
#include "MapLoader/TMJMap.h"
#include <cmath>
#include <algorithm>

/*
 * File: Character.cpp
 * Description: Implementation of the Character class.
 *
 * This file implements the player's sprite loading, animation control and
 * movement logic. Movement includes basic collision handling by querying
 * TMJMap for NotWalkable regions.
 *
 * Dependencies:
 *   - Character.h for the class declaration.
 *   - TMJMap.h for collision queries (pointer only).
 *   - Utils/Logger for logging.
 *
 * Notes:
 *   - The class owns its texture and sprite via unique_ptr to manage lifetime.
 */

/**
 * Constructs a Character and initializes its configuration reference.
 * The actual sprite/texture are created in initialize().
 */
Character::Character()
    : config(CharacterConfigManager::getInstance().getConfig()) {
    // Sprite and texture are created in initialize().
}

/**
 * Destructor cleans up owned resources.
 */
Character::~Character() {
    cleanup();
}

/**
 * Initializes the character by loading its texture and creating the sprite.
 * Also computes collision extents derived from configuration values.
 *
 * @return true on successful initialization, false otherwise.
 */
bool Character::initialize() {
    if (!loadTexture()) {
        Logger::error("Failed to load character texture");
        return false;
    }

    if (!createSprite()) {
        Logger::error("Failed to create character sprite");
        return false;
    }

    // initialize direction
    currentDirection = Direction::Down;

    // Compute scale and apply to sprite.
    float targetScale = static_cast<float>(config.frameWidth) > 0 ?
        config.scale * config.frameWidth / config.frameWidth : config.scale;
    sprite->setScale(sf::Vector2f(targetScale, targetScale));

    // Set sprite origin to center for intuitive positioning.
    sprite->setOrigin(sf::Vector2f(
        config.frameWidth * 0.5f,
        config.frameHeight * 0.5f
    ));

    // Compute collision half extents using configured offsets.
    collisionHalfWidth = config.frameWidth * 0.5f * targetScale + config.collisionOffsetX;
    collisionHalfHeight = config.frameHeight * 0.5f * targetScale + config.collisionOffsetY;

    // Initialize the sprite texture rectangle for the default frame.
    setAnimationFrame(
        currentFrameRow,
        config.directionMapping[static_cast<int>(currentDirection)]
    );

    Logger::info("Character initialized successfully");
    return true;
}

/**
 * Loads the character texture from disk into a unique_ptr.
 *
 * @return true on success.
 */
bool Character::loadTexture() {
    texture = std::make_unique<sf::Texture>();
    if (!texture->loadFromFile(config.texturePath)) {
        Logger::error("Failed to load texture from: " + config.texturePath);
        return false;
    }
    texture->setSmooth(false);
    return true;
}

/**
 * Creates the SFML sprite using the previously loaded texture.
 *
 * @return true on success.
 */
bool Character::createSprite() {
    if (!texture) {
        Logger::error("Cannot create sprite: texture not loaded");
        return false;
    }

    sprite = std::make_unique<sf::Sprite>(*texture);
    return true;
}

/**
 * Releases owned resources.
 */
void Character::cleanup() {
    sprite.reset();
    texture.reset();
}

/**
 * Update the character for a single frame.
 *
 * @param deltaTime Time elapsed since last frame (seconds).
 * @param moveInput Movement input vector, typically from input manager.
 * @param mapWidth Map width in pixels used to clamp movement.
 * @param mapHeight Map height in pixels used to clamp movement.
 * @param map Optional pointer to TMJMap for NotWalkable collision queries.
 */
void Character::update(
    float deltaTime,
    const sf::Vector2f& moveInput,
    int mapWidth,
    int mapHeight,
    const TMJMap* map
) {
    if (!sprite) return;

    // update resting status
    if (isResting) {
        restTimer += deltaTime;
        if (restTimer >= REST_DURATION) {
            stopResting(); // end resting
        }
    }

    handleMovement(deltaTime, moveInput, mapWidth, mapHeight, map);
    updateAnimation(deltaTime);
}

/**
 * Perform movement and basic collision handling.
 *
 * The function performs:
 * 1. Input normalization for diagonal movement.
 * 2. Direction update (only when a single axis is pressed).
 * 3. Attempts full movement; if blocked, tries X-only then Y-only movement.
 *
 * @param map Optional TMJMap used for feet-block checks; if null, no map collision is checked.
 */
void Character::handleMovement(
    float deltaTime,
    const sf::Vector2f& moveInput,
    int mapWidth,
    int mapHeight,
    const TMJMap* map
) {

    // if resting, forbid moving
    if (isResting) {
        moving = false;
        return;
    }

    sf::Vector2f movement = moveInput;
    moving = (movement.x != 0.0f || movement.y != 0.0f);

    // Normalize diagonal movement to keep constant speed.
    if (moving && movement.x != 0.0f && movement.y != 0.0f) {
        float length = std::sqrt(movement.x * movement.x + movement.y * movement.y);
        movement.x /= length;
        movement.y /= length;
    }


    // Dialog walking in priority of horizontal walking
    if (moving) {
        int horizontal = (moveInput.x < 0 ? 1 : 0) + (moveInput.x > 0 ? 1 : 0);
        int vertical = (moveInput.y < 0 ? 1 : 0) + (moveInput.y > 0 ? 1 : 0);
        
        if (horizontal + vertical == 1) {
            if (moveInput.x < 0) currentDirection = Direction::Left;
            else if (moveInput.x > 0) currentDirection = Direction::Right;
            else if (moveInput.y < 0) currentDirection = Direction::Up;
            else if (moveInput.y > 0) currentDirection = Direction::Down;
        }
        else {
            if (std::abs(moveInput.x) > std::abs(moveInput.y)) {
                currentDirection = (moveInput.x < 0) ? Direction::Left : Direction::Right;
            } else {
                currentDirection = (moveInput.y < 0) ? Direction::Up : Direction::Down;
            }
        }
    }
    // Apply speed and delta time.
    movement *= config.moveSpeed * deltaTime;
    sf::Vector2f desiredPos = sprite->getPosition() + movement;

    // Clamp to map boundaries using collision extents.
    desiredPos.x = std::clamp(desiredPos.x, collisionHalfWidth, static_cast<float>(mapWidth) - collisionHalfWidth);
    desiredPos.y = std::clamp(desiredPos.y, collisionHalfHeight, static_cast<float>(mapHeight) - collisionHalfHeight);

    // If no map is provided, apply the desired position directly.
    if (!map) {
        sprite->setPosition(desiredPos);
        return;
    }

    // Helper to compute feet point from sprite center.
    auto feetOf = [&](const sf::Vector2f& center) {
        float scale = sprite->getScale().x;
        float halfH = config.frameHeight * 0.5f * scale;
        return sf::Vector2f(center.x, center.y + halfH - 1.f);
    };

    // Try full movement first.
    sf::Vector2f tryPos = desiredPos;
    bool blocked = map->feetBlockedAt(feetOf(tryPos));
    if (blocked) {
        // Try X-only movement.
        sf::Vector2f tryX(sprite->getPosition().x + movement.x, sprite->getPosition().y);
        tryX.x = std::clamp(tryX.x, collisionHalfWidth, static_cast<float>(mapWidth) - collisionHalfWidth);
        if (!map->feetBlockedAt(feetOf(tryX))) {
            tryPos = tryX;
            blocked = false;
        } else {
            // Try Y-only movement.
            sf::Vector2f tryY(sprite->getPosition().x, sprite->getPosition().y + movement.y);
            tryY.y = std::clamp(tryY.y, collisionHalfHeight, static_cast<float>(mapHeight) - collisionHalfHeight);
            if (!map->feetBlockedAt(feetOf(tryY))) {
                tryPos = tryY;
                blocked = false;
            } else {
                // Both attempts blocked: remain in place.
                tryPos = sprite->getPosition();
            }
        }
    }

    sprite->setPosition(tryPos);
}

void Character::updateAnimation(float deltaTime) {
    if (!sprite) return;
    
    if (moving) {
        // Moving: normal animation
        sprite->setColor(sf::Color(255, 255, 255, 255));
        animationTimer += deltaTime;
        if (animationTimer >= config.animationInterval) {
            animationTimer -= config.animationInterval;
            currentFrameRow = (currentFrameRow + 1) % config.frameRows;
            setAnimationFrame(currentFrameRow, config.directionMapping[static_cast<int>(currentDirection)]);
        }
    } 
    // Resting: Flashing
    else if (isResting) {
        // Fixed as a still frame
        currentFrameRow = 0;
        setAnimationFrame(currentFrameRow, config.directionMapping[static_cast<int>(currentDirection)]);

        // update flashing timer
        flashTimer += deltaTime;
        if (flashTimer >= flashInterval) {
            flashTimer = 0.0f;
            flashState = !flashState; // switch to flashing status
            // set opacity
            sprite->setColor(flashState ? sf::Color(255, 255, 255, 128) : sf::Color(255, 255, 255, 255));
        }
    }
    else {
        currentFrameRow = 0; 
        animationTimer = 0.0f;
        sprite->setColor(sf::Color(255, 255, 255, 255));
        // Refresh the frame immediately when character idle and maintain the current orientation.
        setAnimationFrame(currentFrameRow, config.directionMapping[static_cast<int>(currentDirection)]);
    }
    
    // Update sprite texture rectangle
    setAnimationFrame(
        currentFrameRow, 
        config.directionMapping[static_cast<int>(currentDirection)]
    );
}

void Character::setAnimationFrame(int frameRow, int directionCol) {
    if (!sprite) return;
    
    frameRow = std::clamp(frameRow, 0, config.frameRows - 1);
    directionCol = std::clamp(directionCol, 0, config.directionColumns - 1);
    
    int srcX = directionCol * config.frameWidth;
    int srcY = frameRow * (config.frameHeight + config.rowSpacing);
    
    sprite->setTextureRect(sf::IntRect(
        sf::Vector2i(srcX, srcY),
        sf::Vector2i(config.frameWidth, config.frameHeight)
    ));
}

void Character::setPosition(const sf::Vector2f& position) {
    if (sprite) {
        sprite->setPosition(position);
    }
}

/**
 * @brief Return the character's feet point in world pixel coordinates.
 */
sf::Vector2f Character::getFeetPoint() const {
    if (!sprite) return sf::Vector2f{0.f, 0.f};
    float scale = sprite->getScale().x;
    float halfH = config.frameHeight * 0.5f * scale;
    sf::Vector2f center = sprite->getPosition();
    return sf::Vector2f(center.x, center.y + halfH - 1.f);
}

sf::FloatRect Character::getBounds() const {
    if (!sprite) {
        return sf::FloatRect{sf::Vector2f(0, 0), sf::Vector2f(0, 0)};
    }
    
    sf::FloatRect bounds = sprite->getGlobalBounds();
    
    // Apply collision offsets if configured.
    if (config.collisionOffsetX != 0.0f || config.collisionOffsetY != 0.0f) {
        bounds.position.x += config.collisionOffsetX;
        bounds.position.y += config.collisionOffsetY;
        bounds.size.x -= 2 * config.collisionOffsetX;
        bounds.size.y -= 2 * config.collisionOffsetY;
        
        // Ensure sizes are not negative
        if (bounds.size.x < 0) {
            bounds.position.x += bounds.size.x * 0.5f;
            bounds.size.x = 0;
        }
        if (bounds.size.y < 0) {
            bounds.position.y += bounds.size.y * 0.5f;
            bounds.size.y = 0;
        }
    }
    
    return bounds;
}

void Character::reloadConfig() {
    config = CharacterConfigManager::getInstance().getConfig();
    
    // Reload texture
    if (!loadTexture()) {
        Logger::error("Failed to reload character texture");
        return;
    }
    
    // Recreate sprite
    if (!createSprite()) {
        Logger::error("Failed to recreate character sprite");
        return;
    }
    
    float targetScale = static_cast<float>(config.frameWidth) > 0 ? 
                       config.scale * config.frameWidth / config.frameWidth : config.scale;
    sprite->setScale(sf::Vector2f(targetScale, targetScale));
    
    collisionHalfWidth = config.frameWidth * 0.5f * targetScale + config.collisionOffsetX;
    collisionHalfHeight = config.frameHeight * 0.5f * targetScale + config.collisionOffsetY;
    
    setAnimationFrame(
        currentFrameRow, 
        config.directionMapping[static_cast<int>(currentDirection)]
    );
}

void Character::setCurrentDirection(Direction dir) {
    if (currentDirection == dir) return;
    currentDirection = dir;
    setAnimationFrame(
        currentFrameRow,
        config.directionMapping[static_cast<int>(currentDirection)]
    );
    Logger::debug("Character direction updated to: " + std::to_string(static_cast<int>(dir)));
}

void Character::stopResting() {
    isResting = false;
    restTimer = 0.0f;
    flashTimer = 0.0f;
    flashState = false;
    if (sprite) {
        sprite->setColor(sf::Color(255, 255, 255, 255)); // restore opacity
    }

}
