// codes/Login/LoginScreen.h
#pragma once

class Renderer;

/**
 * Show a home / login screen on the same window as the game.
 *
 * Flow:
 *  - Uses the existing sf::RenderWindow from Renderer (no new window).
 *  - Background: stretched brown panel from the UI spritesheet.
 *  - Two buttons: Enter / Exit, size relative to window size.
 *  - Click Enter -> show introduction screen, then Enter again -> start game.
 *  - Click Exit or close window -> exit app before game starts.
 *
 * Return:
 *  - true  = player wants to start the game
 *  - false = player chose Exit or closed the window
 */
bool runLoginScreen(Renderer& renderer);

