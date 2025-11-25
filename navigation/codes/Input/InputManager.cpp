// InputManager.cpp
#include "InputManager.h"
#include <algorithm>

/*
 * File: InputManager.cpp
 * Description: Implementation of a lightweight input sampling singleton.
 *
 * The InputManager polls sf::Keyboard state each frame and stores boolean
 * vectors for current and previous frames. It provides helpers for movement
 * input (WASD/arrow) and for detecting just-pressed key events.
 */

InputManager& InputManager::getInstance() {
    static InputManager instance;
    return instance;
}

/**
 * Sample the current keyboard state and store a copy of the previous frame.
 *
 * This function should be called once per frame before querying input.
 */
void InputManager::update() {
    // Save previous frame
    previousKeys = currentKeys;

    // Update current frame by sampling all SFML keys.
    currentKeys.clear();
    for (int key = 0; key < static_cast<int>(sf::Keyboard::KeyCount); ++key) {
        currentKeys.push_back(
            sf::Keyboard::isKeyPressed(
                static_cast<sf::Keyboard::Key>(key)
            )
        );
    }
}

/**
 * Returns a normalized movement vector based on common keys.
 * Left/A -> -X, Right/D -> +X, Up/W -> -Y, Down/S -> +Y.
 */
sf::Vector2f InputManager::getMoveInput() const {
    sf::Vector2f input(0.0f, 0.0f);

    if (isKeyPressed(sf::Keyboard::Key::Left) || isKeyPressed(sf::Keyboard::Key::A)) {
        input.x -= 1.0f;
    }
    if (isKeyPressed(sf::Keyboard::Key::Right) || isKeyPressed(sf::Keyboard::Key::D)) {
        input.x += 1.0f;
    }
    if (isKeyPressed(sf::Keyboard::Key::Up) || isKeyPressed(sf::Keyboard::Key::W)) {
        input.y -= 1.0f;
    }
    if (isKeyPressed(sf::Keyboard::Key::Down) || isKeyPressed(sf::Keyboard::Key::S)) {
        input.y += 1.0f;
    }

    return input;
}

/**
 * Safely query whether a key is currently pressed.
 */
bool InputManager::isKeyPressed(sf::Keyboard::Key key) const {
    int index = static_cast<int>(key);
    if (index >= 0 && index < static_cast<int>(currentKeys.size())) {
        return currentKeys[index];
    }
    return false;
}

/**
 * Returns true if the key transitioned from up->down between the previous
 * and current frame.
 */
bool InputManager::isKeyJustPressed(sf::Keyboard::Key key) const {
    int index = static_cast<int>(key);
    if (index >= 0 && index < static_cast<int>(currentKeys.size()) &&
        index < static_cast<int>(previousKeys.size())) {
        return currentKeys[index] && !previousKeys[index];
    }
    return false;
}