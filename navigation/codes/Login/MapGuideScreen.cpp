#include "MapGuideScreen.h"
#include "Renderer/Renderer.h"

#include <SFML/Graphics.hpp>
#include <optional>
#include <iostream>
#include <vector>
#include <string>

bool runMapGuideScreen(Renderer& renderer)
{
    sf::RenderWindow& window = renderer.getWindow();
    window.setView(window.getDefaultView());

    const auto winSize = window.getSize();
    const float winW = static_cast<float>(winSize.x);
    const float winH = static_cast<float>(winSize.y);

    // 1. Load map screenshot
    sf::Texture mapTexture;
    if (!mapTexture.loadFromFile("assets/ui_map_guide.png")) {
        std::cerr << "[MapGuide] Failed to load assets/ui_map_guide.png\n";
        // Skip guide but still allow entering game
        return true;
    }

    sf::Sprite mapSprite(mapTexture);
    {
        const float texW = static_cast<float>(mapTexture.getSize().x);
        const float texH = static_cast<float>(mapTexture.getSize().y);
        const float scaleX = winW / texW;
        const float scaleY = winH / texH;
        mapSprite.setScale(sf::Vector2f{scaleX, scaleY});
        mapSprite.setPosition(sf::Vector2f{0.f, 0.f});
    }

    // 2. Dialog background (use panelInset_brown.png)
    sf::Texture dialogTexture;
    if (!dialogTexture.loadFromFile("assets/panelInset_brown.png")) {
        std::cerr << "[MapGuide] Failed to load assets/panelInset_brown.png\n";
        return true;
    }

    sf::Sprite dialogSprite(dialogTexture);

    // Base size from the texture itself
    const auto dlgSize   = dialogTexture.getSize();
    const float baseDlgW = static_cast<float>(dlgSize.x);
    const float baseDlgH = static_cast<float>(dlgSize.y);

    // Scale dialog to take about 55% of the window width
    const float dialogTargetW = winW * 0.55f;
    const float dialogScale   = dialogTargetW / baseDlgW;
    dialogSprite.setScale(sf::Vector2f{dialogScale, dialogScale});

    const float dialogPixW = baseDlgW * dialogScale;
    const float dialogPixH = baseDlgH * dialogScale;

    // Place the dialog near the bottom center
    dialogSprite.setPosition(sf::Vector2f{
        (winW - dialogPixW) * 0.5f,
        winH * 0.60f
    });


    // 3. Font
    sf::Font font;
    if (!font.openFromFile("fonts/arial.ttf")) {
        std::cerr << "[MapGuide] Failed to load font: fonts/arial.ttf\n";
        return true;
    }

    const unsigned int textSize = 24;

        
    // 4. UI hint data (normalized coords + text)
    struct UiHint {
        sf::FloatRect normRect;  // (x,y,w,h) in 0..1 relative to window
        std::string   text;
    };

    std::vector<UiHint> hints;

    //  Player character
    hints.push_back(UiHint{
        // mark out the character
        sf::FloatRect{ sf::Vector2f{0.48f, 0.16f}, sf::Vector2f{0.04f, 0.04f} },
        "Player\n\nThis is your character. Use WASD or the arrow keys\n"
        "to move around the campus. You can walk on roads and\n"
        "open ground,interact NPCs and enter buildings.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    // Time & Energy bar
    hints.push_back(UiHint{
        sf::FloatRect{ sf::Vector2f{0.01f, 0.03f}, sf::Vector2f{0.22f, 0.08f} },
        "Time & Energy Bar\n\nThe top-left panel shows the current in-game time\n"
        "and your energy bar. Energy decreases as you walk\n"
        "around and complete tasks. You can restore energy\n"
        "by eating, resting, or doing relaxing activities.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    // Points & Tasks
    hints.push_back(UiHint{
        sf::FloatRect{ sf::Vector2f{0.01f, 0.12f}, sf::Vector2f{0.22f, 0.26f} },
        "Points & Tasks\n\n\"Points\" shows your current score for the game.\n"
        "The list below are your tasks, such as eating at\n"
        "the canteen or talking to a professor. Completing\n"
        "tasks increases your points and helps you finish\n"
        "the game week successfully.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    //  Schedule button
    hints.push_back(UiHint{
        sf::FloatRect{ sf::Vector2f{0.71f, 0.01f}, sf::Vector2f{0.12f, 0.08f} },
        "Schedule Button\n\nClick \"Schedule\" to open your timetable.\n"
        "Click again to close the timetable.\n"
        "It shows where you need to go and at what time.\n"
        "You can use it to plan which task to do next.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    // Map button
    hints.push_back(UiHint{
        sf::FloatRect{ sf::Vector2f{0.85f, 0.01f}, sf::Vector2f{0.09f, 0.08f} },
        "Map Button\n\nClick \"Map\" to open the full campus map.\n"
        "Press Esc to close the map.\n"
        "You can zoom in/out and see how different areas\n"
        "are connected, so it is easier to find your way.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    // Building Entrances
    hints.push_back(UiHint{
        // take a section that contains the blue entrance frame
        sf::FloatRect{ sf::Vector2f{0.44f, 0.11f}, sf::Vector2f{0.08f, 0.12f} },
        "Building Entrances\n\nBlue squares mark the entrances of buildings.\n"
        "Walk your character into a blue square and press Enter\n"
        "to enter that building and see events inside.\n"
        "Press Esc to stop entering the building.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    //Buildings, roads & open ground
    hints.push_back(UiHint{
        sf::FloatRect{ sf::Vector2f{0.41f, 0.32f}, sf::Vector2f{0.24f, 0.25f} },
        "Campus Buildings & Roads\n\nGrey areas with building names like \"Shaw College\"\n"
        "are the main campus buildings. Dark grey is the road,\n"
        "and the light beige tiles are walkable ground.\n"
        "You can explore these areas to look for task locations.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    //  Plants / Trees
    hints.push_back(UiHint{
        sf::FloatRect{ sf::Vector2f{0.80f, 0.13f}, sf::Vector2f{0.18f, 0.12f} },
        "Plants & Trees\n\nThe colourful dots represent plants and trees.\n"
        "They are decoration and cannot be walked on. You\n"
        "need to move along the roads and open ground instead\n"
        "of cutting through the plant areas.\n\n"
        "[This is the last hint, press Enter to start the game...]\n"
    });

    std::size_t current = 0;



    // 5. Main loop: A/Left prev, D/Right next, Enter to finish
    while (window.isOpen()) {
        // Events
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                return false;
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Escape) {
                    // Allow Esc to skip guide but still start game
                    return true;
                }

                if (keyPressed->code == sf::Keyboard::Key::Enter) {
                    // Enter always moves forward; on last hint it finishes
                    if (current + 1 < hints.size()) {
                        ++current;
                    } else {
                        return true; // finished all hints, enter the game
                    }
                }
            }

        }

        // Draw
        window.clear(sf::Color::Black);
        window.draw(mapSprite);

        if (!hints.empty()) {
            const UiHint& h = hints[current];

            // Red rectangle highlight
            sf::RectangleShape highlight;
            highlight.setPosition(sf::Vector2f{
                h.normRect.position.x * winW,
                h.normRect.position.y * winH
            });
            highlight.setSize(sf::Vector2f{
                h.normRect.size.x * winW,
                h.normRect.size.y * winH
            });
            highlight.setFillColor(sf::Color(0, 0, 0, 0));
            highlight.setOutlineColor(sf::Color::Red);
            highlight.setOutlineThickness(3.f);
            window.draw(highlight);

            // Dialog panel + text
            window.draw(dialogSprite);

            sf::Text text(font);
            text.setString(h.text);
            text.setCharacterSize(textSize);
            text.setFillColor(sf::Color::White);

            sf::FloatRect tb = text.getLocalBounds();

            // Use top-left as origin (not center)
            text.setOrigin(sf::Vector2f{
                tb.position.x,
                tb.position.y
            });

            const sf::Vector2f dialogPos = dialogSprite.getPosition();

            // Padding inside the dialog panel
            const float padX = dialogPixW * 0.06f;  // ~6% of panel width
            const float padY = dialogPixH * 0.10f;  // ~10% of panel height

            text.setPosition(sf::Vector2f{
                dialogPos.x + padX,
                dialogPos.y + padY
            });

            window.draw(text);
        }

        // Do not draw HUD buttons on the tutorial/map-guide overlay window.

        window.display();
    }

    return false;
}
