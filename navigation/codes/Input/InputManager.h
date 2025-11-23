// InputManager.h
#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

/**
 * @file InputManager.h
 * @brief Singleton that samples keyboard state and translates it into game input.
 *
 * The InputManager polls SFML keyboard state each frame and stores the current
 * and previous states of all keys. It provides helpers to query movement input
 * vectors and detect key-press events (just-pressed).
 */
class InputManager {
public:
    static InputManager& getInstance();
    
    /**
     * @brief Update input state for the current frame.
     */
    void update();
    
    /**
     * @brief Get movement input vector (e.g. WASD / arrow keys).
     */
    sf::Vector2f getMoveInput() const;
    
    /**
     * @brief Check whether a key is currently pressed.
     */
    bool isKeyPressed(sf::Keyboard::Key key) const;
    
    /**
     * @brief Check whether a key was just pressed this frame (transition up->down).
     */
    bool isKeyJustPressed(sf::Keyboard::Key key) const;

private:
    InputManager() = default;
    
    // Key state for the current frame
    std::vector<bool> currentKeys;
    // Key state for the previous frame
    std::vector<bool> previousKeys;
};