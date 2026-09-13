#include "core/Window.hpp"
#include "core/Camera.hpp"
#include "core/Timer.hpp"
#include "renderer/Renderer.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "=========================================================\n";
    std::cout << " WHITEOUT DELARUS: 1:1 SCALE HIMALAYA ENGINE (STEP 2)\n";
    std::cout << " Rendering API: Vulkan 1.3+ / 1.4 (Dynamic Rendering)\n";
    std::cout << " Shading Language: Slang (SPIR-V)\n";
    std::cout << " Target: Mount Everest Massif & Sagarmatha\n";
    std::cout << " Controls:\n";
    std::cout << "   - Right Mouse + Drag: Look around\n";
    std::cout << "   - W / A / S / D: Fly forward / left / back / right\n";
    std::cout << "   - Q / E (or Ctrl / Space): Descend / Ascend\n";
    std::cout << "   - Left Shift: Turbo flight sprint\n";
    std::cout << "   - ESC: Exit\n";
    std::cout << "=========================================================\n" << std::endl;

    try {
        const int initialWidth = 1600;
        const int initialHeight = 900;
        whiteout::core::Window window(
            "Whiteout Delarus - 1:1 Mount Everest (C++ / Vulkan / Slang)",
            initialWidth,
            initialHeight
        );

        // Position camera high above the South ridge overlooking Mount Everest (elevation ~9,200m)
        whiteout::core::Camera camera(glm::vec3(0.0f, 9200.0f, 22000.0f), 65.0f);
        camera.setAspectRatio(window.getAspectRatio());
        camera.setLookAt(glm::vec3(0.0f, 6500.0f, 0.0f));

        whiteout::renderer::Renderer renderer(window);
        whiteout::core::Timer timer;

        // Check for --screenshot CLI argument
        std::string screenshotPath = "";
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--screenshot") {
                screenshotPath = (i + 1 < argc) ? argv[i + 1] : "everest_vulkan_slang.png";
                break;
            }
        }

        if (!screenshotPath.empty()) {
            std::cout << "[Engine] Screenshot mode active: Rendering frame to " << screenshotPath << std::endl;
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

            camera.update(timer.deltaTime(), input);
            renderer.renderFrame(camera, timer.totalTime());

            // Update title with FPS, Altitude, and Coordinates every 0.25s
            titleUpdateTimer += timer.deltaTime();
            if (titleUpdateTimer >= 0.25f) {
                titleUpdateTimer = 0.0f;
                glm::vec3 pos = camera.getPosition();
                std::stringstream title;
                title << "Whiteout Delarus [Vulkan/Slang] | "
                      << std::fixed << std::setprecision(1) << timer.currentFps() << " FPS | "
                      << "Altitude: " << static_cast<int>(pos.y) << " m | "
                      << "Pos: (" << static_cast<int>(pos.x) << ", " << static_cast<int>(pos.z) << ") | GPU: "
                      << renderer.getContext().getGpuName();

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
