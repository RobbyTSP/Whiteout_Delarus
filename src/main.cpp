#include "core/Window.hpp"
#include "core/Camera.hpp"
#include "core/Timer.hpp"
#include "renderer/Renderer.hpp"
#include "game/TerrainCollider.hpp"
#include "game/Player.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "=========================================================\n";
    std::cout << " WHITEOUT DELARUS: 1:1 HIMALAYA FIRST-PERSON GAME (STEP 3)\n";
    std::cout << " Rendering API: Vulkan 1.3+ / 1.4 (Dynamic Rendering)\n";
    std::cout << " Shading Language: Slang (SPIR-V)\n";
    std::cout << " Physics: 1:1 Bilinear Terrain Collision & Ground Physics\n";
    std::cout << " Controls:\n";
    std::cout << "   - Mouse Move: Look around (Click window to capture mouse)\n";
    std::cout << "   - W / A / S / D: Walk forward / left / back / right\n";
    std::cout << "   - Left Shift: Sprint\n";
    std::cout << "   - Space: Jump / Hopping over rocks & crevasses\n";
    std::cout << "   - C / Ctrl: Crouch / Lower profile\n";
    std::cout << "   - Tab / V: Switch between First-Person Walk <-> Drone Flight\n";
    std::cout << "   - 1: Fast Travel -> Everest Base Camp South (5,303m)\n";
    std::cout << "   - 2: Fast Travel -> Mount Everest Summit Ridge (8,729m)\n";
    std::cout << "   - 3: Fast Travel -> Lhotse Face / South Col (8,410m)\n";
    std::cout << "   - 4: Fast Travel -> Ama Dablam Valley (4,653m)\n";
    std::cout << "   - ESC: Release mouse capture / Exit\n";
    std::cout << "=========================================================\n" << std::endl;

    try {
        const int initialWidth = 1600;
        const int initialHeight = 900;
        whiteout::core::Window window(
            "Whiteout Delarus - 1:1 Mount Everest (First-Person)",
            initialWidth,
            initialHeight
        );

        // Capture mouse on start for true FPS feel
        window.setMouseCapture(true);

        whiteout::core::Camera camera(glm::vec3(-15645.0f, 5306.0f, -9751.0f), 70.0f);
        camera.setAspectRatio(window.getAspectRatio());

        whiteout::renderer::Renderer renderer(window);

        // Load 1:1 DEM Terrain Collider for player physics & ground collision
        whiteout::game::TerrainCollider collider(34610.0f, 34520.0f);
        collider.loadDem(DATA_DIR "/processed/everest_dem_float32.bin", 1024, 1024);

        // First-person player character controller
        whiteout::game::Player player(camera, collider);

        whiteout::core::Timer timer;

        // Check for --screenshot CLI argument
        std::string screenshotPath = "";
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--screenshot") {
                screenshotPath = (i + 1 < argc) ? argv[i + 1] : "everest_first_person.png";
                break;
            }
        }

        if (!screenshotPath.empty()) {
            std::cout << "[Engine] Screenshot mode active: Rendering first-person frame to " << screenshotPath << std::endl;
            for (int f = 0; f < 5; f++) {
                timer.tick();
                renderer.renderFrame(camera, timer.totalTime());
            }
            renderer.saveScreenshot(screenshotPath);
            std::cout << "[Engine] Screenshot successfully captured. Exiting." << std::endl;
            return 0;
        }

        float titleUpdateTimer = 0.0f;

        while (true) {
            timer.tick();

            whiteout::core::WindowEventState input = window.pollEvents();
            if (input.shouldClose) {
                break;
            }

            if (input.resized) {
                camera.setAspectRatio(window.getAspectRatio());
                renderer.onResize();
                window.resetResizeFlag();
            }

            // Update player physics, collision, ground snapping, and camera
            player.update(timer.deltaTime(), input);

            // Render frame using Vulkan 1.3+ Dynamic Rendering & Slang shader
            renderer.renderFrame(camera, timer.totalTime());

            // Realtime HUD & Telemetry in window title
            titleUpdateTimer += timer.deltaTime();
            if (titleUpdateTimer >= 0.15f) {
                titleUpdateTimer = 0.0f;
                std::stringstream title;
                title << "Whiteout Delarus | "
                      << std::fixed << std::setprecision(0) << timer.currentFps() << " FPS | "
                      << player.getTelemetryString();

                SDL_SetWindowTitle(window.getNativeHandle(), title.str().c_str());
            }
        }

        std::cout << "\n[Engine] Clean shutdown completed." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n[Fatal Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
