#include "LoginScreen.h"
#include "Renderer/Renderer.h"
#include "MapGuideScreen.h"


#include <SFML/Graphics.hpp>
#include <optional>
#include <iostream>

// Show Home -> Intro -> Controls on the same window.
// Return true  -> go into the actual game
// Return false -> quit before starting game
bool runLoginScreen(Renderer& renderer)
{
    // Get window from Renderer (SFML 3 RenderWindow)
    sf::RenderWindow& window = renderer.getWindow();
    window.setView(window.getDefaultView());

    const auto winSize = window.getSize();
    const float winW = static_cast<float>(winSize.x);
    const float winH = static_cast<float>(winSize.y);

    // ----------------------------------------------------
    // 1. Load UI spritesheet (panels, buttons)
    // ----------------------------------------------------
    sf::Texture uiTexture;
    if (!uiTexture.loadFromFile("assets/uipack_rpg_sheet.png")) {
        std::cerr << "[Login] Failed to load assets/uipack_rpg_sheet.png\n";
        return false;
    }

    // ----------------------------------------------------
    // 2. Load keyboard & mouse control sheet
    // ----------------------------------------------------
    sf::Texture controlTexture;
    if (!controlTexture.loadFromFile("assets/keyboard-&-mouse_sheet_default.png")) {
        std::cerr << "[Login] Failed to load assets/keyboard-&-mouse_sheet_default.png\n";
        return false;
    }

    // The keyboard-mouse sheet is a 16x16 grid of 64x64 icons
    const int ICON_TILE = 64;
    auto makeIconRect = [ICON_TILE](int cx, int cy) {
        return sf::IntRect{
            sf::Vector2i{cx * ICON_TILE, cy * ICON_TILE},
            sf::Vector2i{ICON_TILE, ICON_TILE}
        };
    };

    // ----------------------------------------------------
    // 3. Background panel from UI sheet
    // ----------------------------------------------------
    const sf::IntRect BG_PANEL_RECT{
        sf::Vector2i{0, 376},      // position in the spritesheet
        sf::Vector2i{100, 100}     // size in the spritesheet
    };

    sf::Sprite bgPanel(uiTexture, BG_PANEL_RECT);

    const float bgW = static_cast<float>(BG_PANEL_RECT.size.x);
    const float bgH = static_cast<float>(BG_PANEL_RECT.size.y);

    const float bgScaleX = winW / bgW;
    const float bgScaleY = winH / bgH;
    bgPanel.setScale(sf::Vector2f{bgScaleX, bgScaleY});
    bgPanel.setPosition(sf::Vector2f{0.f, 0.f});

    const sf::Color deepBrown(150, 100, 60);

    // ----------------------------------------------------
    // 4. Font (SFML 3 uses openFromFile)
    // ----------------------------------------------------
    sf::Font font;
    if (!font.openFromFile("fonts/arial.ttf")) {
        std::cerr << "[Login] Failed to load font: fonts/arial.ttf\n";
        return false;
    }

    // Character sizes based on window height
    const unsigned int gameTitleSize    = static_cast<unsigned int>(winH * 0.08f);
    const unsigned int pageTitleSize    = static_cast<unsigned int>(winH * 0.06f);
    const unsigned int buttonSize       = static_cast<unsigned int>(winH * 0.04f);
    const unsigned int controlsTextSize = static_cast<unsigned int>(winH * 0.035f);

    const float gameTitleY = winH * 0.18f;
    const float homeTitleY = winH * 0.40f;

    // ----------------------------------------------------
    // 5. Game title
    // ----------------------------------------------------
    sf::Text gameTitle(font);
    gameTitle.setString("Daily Life in CUHKSZ");
    gameTitle.setCharacterSize(gameTitleSize);
    gameTitle.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = gameTitle.getLocalBounds();
        gameTitle.setOrigin(sf::Vector2f{
            b.position.x + b.size.x / 2.f,
            b.position.y + b.size.y / 2.f
        });
        gameTitle.setPosition(sf::Vector2f{winW * 0.5f, gameTitleY});
    }

    // ----------------------------------------------------
    // 6. "Home" title
    // ----------------------------------------------------
    sf::Text homeTitle(font);
    homeTitle.setString("Home");
    homeTitle.setCharacterSize(pageTitleSize);
    homeTitle.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = homeTitle.getLocalBounds();
        homeTitle.setOrigin(sf::Vector2f{
            b.position.x + b.size.x / 2.f,
            b.position.y + b.size.y / 2.f
        });
        homeTitle.setPosition(sf::Vector2f{winW * 0.5f, homeTitleY});
    }

    // ----------------------------------------------------
    // 7. Buttons on Home screen
    // ----------------------------------------------------
    const sf::IntRect BUTTON_KHAKI_RECT{
        sf::Vector2i{2, 240},
        sf::Vector2i{188, 40}
    };

    sf::Sprite enterButtonSprite(uiTexture, BUTTON_KHAKI_RECT);
    sf::Sprite exitButtonSprite(uiTexture,  BUTTON_KHAKI_RECT);

    const float baseButtonW = static_cast<float>(BUTTON_KHAKI_RECT.size.x);
    const float baseButtonH = static_cast<float>(BUTTON_KHAKI_RECT.size.y);

    const float targetButtonWidth = winW * 0.25f;
    const float buttonScaleX      = targetButtonWidth / baseButtonW;
    const float buttonScaleY      = buttonScaleX * 1.3f;

    enterButtonSprite.setScale(sf::Vector2f{buttonScaleX, buttonScaleY});
    exitButtonSprite.setScale(sf::Vector2f{buttonScaleX, buttonScaleY});

    enterButtonSprite.setOrigin(sf::Vector2f{baseButtonW / 2.f, baseButtonH / 2.f});
    exitButtonSprite.setOrigin(sf::Vector2f{baseButtonW / 2.f, baseButtonH / 2.f});

    const float centerX      = winW * 0.5f;
    const float enterButtonY = winH * 0.55f;
    const float exitButtonY  = winH * 0.70f;

    enterButtonSprite.setPosition(sf::Vector2f{centerX, enterButtonY});
    exitButtonSprite.setPosition(sf::Vector2f{centerX, exitButtonY});

    sf::Text enterText(font);
    enterText.setString("Enter");
    enterText.setCharacterSize(buttonSize);
    enterText.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = enterText.getLocalBounds();
        enterText.setOrigin(sf::Vector2f{
            b.position.x + b.size.x / 2.f,
            b.position.y + b.size.y / 2.f
        });
        enterText.setPosition(enterButtonSprite.getPosition());
    }

    sf::Text exitText(font);
    exitText.setString("Exit");
    exitText.setCharacterSize(buttonSize);
    exitText.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = exitText.getLocalBounds();
        exitText.setOrigin(sf::Vector2f{
            b.position.x + b.size.x / 2.f,
            b.position.y + b.size.y / 2.f
        });
        exitText.setPosition(exitButtonSprite.getPosition());
    }

    // ----------------------------------------------------
    // 8. Intro text page
    // ----------------------------------------------------
    sf::Text introText(font);
    introText.setString(
        "Background Introduction\n\n"
        "- You are a new student at CUHKSZ.\n"
        "- Talk to NPCs and complete tasks.\n"
        "- Explore the campus at your wish.\n\n\n"
        "[Press Enter to continue...]"
    );
    introText.setCharacterSize(buttonSize);
    introText.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = introText.getLocalBounds();
        introText.setOrigin(sf::Vector2f{b.size.x / 2.f, b.size.y / 2.f});
        introText.setPosition(sf::Vector2f{winW * 0.5f, winH * 0.5f});
    }

    // ----------------------------------------------------
    // 9. Controls page: icons and labels
    // ----------------------------------------------------

    // Title for controls page
    sf::Text controlsTitle(font);
    controlsTitle.setString("Controls");
    controlsTitle.setCharacterSize(pageTitleSize);
    controlsTitle.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = controlsTitle.getLocalBounds();
        controlsTitle.setOrigin(sf::Vector2f{
            b.position.x + b.size.x / 2.f,
            b.position.y + b.size.y / 2.f
        });
        // Slightly lower than game title
        controlsTitle.setPosition(sf::Vector2f{winW * 0.5f, winH * 0.18f});
    }

    // Shared icon scale
    const float iconBaseSize   = 64.f;
    const float iconTargetSize = winH * 0.07f;
    const float iconScale      = iconTargetSize / iconBaseSize;

    auto makeIconSprite = [&](int cx, int cy) {
        sf::Sprite s(controlTexture, makeIconRect(cx, cy));
        s.setScale(sf::Vector2f{iconScale, iconScale});
        s.setOrigin(sf::Vector2f{iconBaseSize / 2.f, iconBaseSize / 2.f});
        return s;
    };

    // WASD
    sf::Sprite iconW = makeIconSprite(7, 2);
    sf::Sprite iconA = makeIconSprite(5, 14);
    sf::Sprite iconS = makeIconSprite(9, 4);
    sf::Sprite iconD = makeIconSprite(6, 10);

    // Arrow keys (replace with the coordinates of the real arrow key icons on your sheet)
    // If these are not the correct icons, just adjust the (column, row) pairs.
    sf::Sprite iconArrowUp    = makeIconSprite(3, 13);
    sf::Sprite iconArrowLeft  = makeIconSprite(15, 14);
    sf::Sprite iconArrowDown  = makeIconSprite(13, 14);
    sf::Sprite iconArrowRight = makeIconSprite(1, 13);

    // Mouse wheel: use only the right icon
    sf::Sprite iconWheel = makeIconSprite(15, 1);

    // ESC key
    sf::Sprite iconEsc = makeIconSprite(2, 9);

    // E key
    sf::Sprite iconE = makeIconSprite(10, 10);

    // Left mouse button
    sf::Sprite iconMouseLeft = makeIconSprite(3, 1);

    // Control block layout
    const float controlsStartY   = winH * 0.30f;   // base Y for the controls block
    const float rowGap           = winH * 0.08f;   // vertical gap between rows

    const float centerXControls  = winW * 0.5f;

    const float shiftAmount      = winW * 0.10f;
    const float iconsCenterX     = centerXControls - winW * 0.12f - shiftAmount; // icons slightly left of center
    const float textX            = centerXControls + winW * 0.05f - shiftAmount; // text slightly right of center

    // Row 0: WASD (first row)
    const float rowWASD_Y = controlsStartY + rowGap * 0.f;

    iconW.setPosition(sf::Vector2f{iconsCenterX - 1.5f * iconTargetSize, rowWASD_Y});
    iconA.setPosition(sf::Vector2f{iconsCenterX - 0.5f * iconTargetSize, rowWASD_Y});
    iconS.setPosition(sf::Vector2f{iconsCenterX + 0.5f * iconTargetSize, rowWASD_Y});
    iconD.setPosition(sf::Vector2f{iconsCenterX + 1.5f * iconTargetSize, rowWASD_Y});

    // Row 1: arrow keys (second row, also in one line)
    const float rowArrow_Y = controlsStartY + rowGap * 1.f;

    // NOTE: choose the white-background arrow key icons you like by adjusting
    // the (column, row) pairs used when creating iconArrowUp/Left/Down/Right.
    // Here we just position them in a row; the sprites themselves are already created.
    iconArrowUp.setPosition   (sf::Vector2f{iconsCenterX - 1.5f * iconTargetSize, rowArrow_Y});
    iconArrowLeft.setPosition (sf::Vector2f{iconsCenterX - 0.5f * iconTargetSize, rowArrow_Y});
    iconArrowDown.setPosition (sf::Vector2f{iconsCenterX + 0.5f * iconTargetSize, rowArrow_Y});
    iconArrowRight.setPosition(sf::Vector2f{iconsCenterX + 1.5f * iconTargetSize, rowArrow_Y});

    // Movement text: vertically centered between the two rows
    const float moveTextY = 0.5f * (rowWASD_Y + rowArrow_Y);

    sf::Text textMove(font);
    textMove.setString("Move: WASD or Arrow Keys");
    textMove.setCharacterSize(controlsTextSize);
    textMove.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = textMove.getLocalBounds();
        textMove.setOrigin(sf::Vector2f{0.f, b.size.y / 2.f});
        textMove.setPosition(sf::Vector2f{textX, moveTextY});
    }

    // Row 2: zoom mini-map (mouse wheel)
    const float rowWheel_Y = controlsStartY + rowGap * 2.5f; // leave a bit extra gap under the arrows
    iconWheel.setPosition(sf::Vector2f{iconsCenterX, rowWheel_Y});

    sf::Text textZoom(font);
    textZoom.setString("Zoom mini-map: Mouse wheel");
    textZoom.setCharacterSize(controlsTextSize);
    textZoom.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = textZoom.getLocalBounds();
        textZoom.setOrigin(sf::Vector2f{0.f, b.size.y / 2.f});
        textZoom.setPosition(sf::Vector2f{textX, rowWheel_Y});
    }

    // Row 3: close mini-map (ESC)
    const float rowEsc_Y = controlsStartY + rowGap * 3.5f;
    iconEsc.setPosition(sf::Vector2f{iconsCenterX, rowEsc_Y});

    sf::Text textEsc(font);
    textEsc.setString("Close mini-map: ESC");
    textEsc.setCharacterSize(controlsTextSize);
    textEsc.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = textEsc.getLocalBounds();
        textEsc.setOrigin(sf::Vector2f{0.f, b.size.y / 2.f});
        textEsc.setPosition(sf::Vector2f{textX, rowEsc_Y});
    }

    // Row 4: interact (E)
    const float rowE_Y = controlsStartY + rowGap * 4.5f;
    iconE.setPosition(sf::Vector2f{iconsCenterX, rowE_Y});

    sf::Text textInteract(font);
    textInteract.setString("Interact: E");
    textInteract.setCharacterSize(controlsTextSize);
    textInteract.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = textInteract.getLocalBounds();
        textInteract.setOrigin(sf::Vector2f{0.f, b.size.y / 2.f});
        textInteract.setPosition(sf::Vector2f{textX, rowE_Y});
    }

    // Row 5: click (left mouse button)
    const float rowClick_Y = controlsStartY + rowGap * 5.5f;
    iconMouseLeft.setPosition(sf::Vector2f{iconsCenterX, rowClick_Y});

    sf::Text textClick(font);
    textClick.setString("Click when needed: Left Mouse Button");
    textClick.setCharacterSize(controlsTextSize);
    textClick.setFillColor(sf::Color::White);
    {
        sf::FloatRect b = textClick.getLocalBounds();
        textClick.setOrigin(sf::Vector2f{0.f, b.size.y / 2.f});
        textClick.setPosition(sf::Vector2f{textX, rowClick_Y});
    }

    // Row 6: Small hint under the last line
    sf::Text textContinue(font);
    textContinue.setString("[Press Enter to continue...]");

    // Slightly smaller than other control texts
    unsigned int continueSize =
        static_cast<unsigned int>(controlsTextSize * 0.9f);
    textContinue.setCharacterSize(continueSize);
    textContinue.setFillColor(sf::Color::White);

    // Center this line under the whole controls block
    {
        sf::FloatRect b = textContinue.getLocalBounds();
        textContinue.setOrigin(sf::Vector2f{
            b.position.x + b.size.x / 2.f,
            b.position.y + b.size.y / 2.f
        });
        // Put it a bit below the click row, centered horizontally
        textContinue.setPosition(sf::Vector2f{
            centerXControls,
            rowClick_Y + rowGap * 1.8f
        });
    }


    // ----------------------------------------------------
    // 10. Screen state machine: Home -> Intro -> Controls
    // ----------------------------------------------------
    enum class ScreenState { Home, Intro, Controls };
    ScreenState screen = ScreenState::Home;
    bool wantStartGame = false;

    // ----------------------------------------------------
    // 11. Main loop (SFML 3 event API)
    // ----------------------------------------------------
    while (window.isOpen() && !wantStartGame) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                return false;
            }

            // Mouse click for buttons on Home screen
            if (const auto* mouseButton = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouseButton->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mousePos(
                        static_cast<float>(mouseButton->position.x),
                        static_cast<float>(mouseButton->position.y)
                    );

                    if (screen == ScreenState::Home) {
                        if (enterButtonSprite.getGlobalBounds().contains(mousePos)) {
                            screen = ScreenState::Intro; // Home -> Intro
                        } else if (exitButtonSprite.getGlobalBounds().contains(mousePos)) {
                            window.close();
                            return false;
                        }
                    }
                }
            }

            // Keyboard: handle Enter on different screens
            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Enter) {
                    if (screen == ScreenState::Home) {
                        screen = ScreenState::Intro;       // first: show intro
                    } else if (screen == ScreenState::Intro) {
                        screen = ScreenState::Controls;    // second: show controls
                    } else if (screen == ScreenState::Controls) {
                        wantStartGame = true;              // third: start game
                    }
                }
            }
        }

        // ---------------- Draw current screen ----------------
        window.clear(deepBrown);
        window.draw(bgPanel);

        if (screen == ScreenState::Home) {
            window.draw(gameTitle);
            window.draw(homeTitle);
            window.draw(enterButtonSprite);
            window.draw(exitButtonSprite);
            window.draw(enterText);
            window.draw(exitText);
        }
        else if (screen == ScreenState::Intro) {
            window.draw(gameTitle);
            window.draw(introText);
        }
        else { // ScreenState::Controls
            window.draw(controlsTitle);

            // Movement
            window.draw(iconW);
            window.draw(iconA);
            window.draw(iconS);
            window.draw(iconD);
            window.draw(iconArrowUp);
            window.draw(iconArrowLeft);
            window.draw(iconArrowDown);
            window.draw(iconArrowRight);
            window.draw(textMove);

            // Mini-map zoom
            window.draw(iconWheel);
            window.draw(textZoom);

            // Close mini-map
            window.draw(iconEsc);
            window.draw(textEsc);

            // Interact
            window.draw(iconE);
            window.draw(textInteract);

            // Click
            window.draw(iconMouseLeft);
            window.draw(textClick);

            // Hint
            window.draw(textContinue);
        }

        window.display();
    }

    // If we exit the loop because the player finished all login pages
    // (Home / Intro / Controls) and the window is still open,
    // run the map UI guide BEFORE entering the actual game.
    if (wantStartGame && window.isOpen()) {
        // This shows the map guide screen (screenshot + red rectangles + text)
        return runMapGuideScreen(renderer);
    }

    // Otherwise, treat it as "quit before game"
    return false;
}
