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

    // ----------------------------------------------------
    // 1. Load map screenshot (your UI screenshot)
    // ----------------------------------------------------
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

    // ----------------------------------------------------
    // 2. Dialog background (use panelInset_brown.png)
    // ----------------------------------------------------
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


    // ----------------------------------------------------
    // 3. Font
    // ----------------------------------------------------
    sf::Font font;
    if (!font.openFromFile("fonts/arial.ttf")) {
        std::cerr << "[MapGuide] Failed to load font: fonts/arial.ttf\n";
        return true;
    }

    const unsigned int textSize = 24;

    // ----------------------------------------------------
    // 4. UI hint data (normalized coords + text)
    // ----------------------------------------------------
    struct UiHint {
        sf::FloatRect normRect;  // (x,y,w,h) in 0..1 relative to window
        std::string   text;
    };

    std::vector<UiHint> hints;

    // Hint 1: Map button (top-right)
    hints.push_back(UiHint{
        sf::FloatRect{ sf::Vector2f{0.84f, 0.01f}, sf::Vector2f{0.08f, 0.07f} },
        "Map Button\n\nClick here to open the full campus map.\n"
        "You can zoom and see the whole layout.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    // Hint 2: Time + energy / experience bars (top-left)
    hints.push_back(UiHint{
        sf::FloatRect{ sf::Vector2f{0.01f, 0.03f}, sf::Vector2f{0.18f, 0.10f} },
        "Time & Bars\n\nTop-left shows the current time of day.\n"
        "The upper bar is your energy. You can restore it by\n"
        "doing certain activities (eating, resting, etc.).\n"
        "The lower bar is your experience. When it is full,\n"
        "you have successfully completed the game.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    // Hint 3: Task list (left side)
    hints.push_back(UiHint{
        sf::FloatRect{ sf::Vector2f{0.02f, 0.14f}, sf::Vector2f{0.15f, 0.20f} },
        "Tasks\n\nThis panel shows your current tasks.\n"
        "Finish all listed tasks to progress through the day.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    // Hint 4: Building entrances (blue squares)
    hints.push_back(UiHint{
        // Rough region that covers one of the blue entrance squares
        sf::FloatRect{ sf::Vector2f{0.44f, 0.11f}, sf::Vector2f{0.08f, 0.10f} },
        "Building Entrances\n\nThe blue squares mark the entrances of buildings.\n"
        "Walk your character into a blue square and press E\n"
        "to enter that building.\n\n"
        "[Press Enter to view the next hint...]\n"
    });

    // Hint 5: Plants / trees (not walkable)
    hints.push_back(UiHint{
        // Highlight a patch of plants on the right side
        sf::FloatRect{ sf::Vector2f{0.82f, 0.12f}, sf::Vector2f{0.15f, 0.13f} },
        "Plants & Trees\n\nThese green areas are decoration and cannot\n"
        "be walked on. You need to move along paths and\n"
        "open ground instead of cutting through plants.\n\n"
        "[This is the last hint, press Enter to start the game...]\n"
    });

    std::size_t current = 0;


    // ----------------------------------------------------
    // 5. Main loop: A/Left prev, D/Right next, Enter to finish
    // ----------------------------------------------------
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
