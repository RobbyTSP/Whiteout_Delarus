#include "core/Window.hpp"
#include "core/Camera.hpp"
#include "core/Timer.hpp"
#include "renderer/Renderer.hpp"
#include "game/TerrainCollider.hpp"
#include "game/Player.hpp"
#include "game/WeatherSystem.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "=========================================================\n";
    std::cout << " WHITEOUT DELARUS: 1:1 HIMALAYA ENGINE - STEP 7 (1:1 PART III)\n";
    std::cout << " Photorealism: Alpine Ridge Sculpting, Couloir Fluting & Talus Cones\n";
    std::cout << " Shader: Triplanar PBR, Slope-Masking, Anti-Tiling, Horizon AO & ACES\n";
    std::cout << " Rendering API: Vulkan 1.3+ / 1.4 (Dynamic Rendering)\n";
    std::cout << " Shading Language: Slang (SPIR-V)\n";
    std::cout << " 1:1 Scale: 1,048,576 Vertices / 2,093,058 Triangles (1024x1024 DEM)\n";
    std::cout << " Atmosphere: Volumetric Wolkenmeer, Alpenglühen, Aerial Rayleigh Haze\n";
    std::cout << " Weather: Live Open-Meteo Sync, Spindrift & Whiteout Simulation\n";
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
    std::cout << "   - 5: Fast Travel -> Khumbu Icefall Séracs & Crevasses (5,867m)\n";
    std::cout << "   - T: Cycle Time of Day (Dawn Alpenglühen -> Noon -> Sunset -> Night)\n";
    std::cout << "   - B: Toggle Blizzard / Whiteout Mode (30m Visibility & Spindrift)\n";
    std::cout << "   - L: Toggle Live Open-Meteo Weather Synchronization\n";
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

        // Live Weather & Atmosphere System (Open-Meteo + Alpenglühen + Wolkenmeer)
        whiteout::game::WeatherSystem weatherSystem(DATA_DIR "/weather/everest_current.json");

        whiteout::core::Timer timer;

        // Check for CLI arguments: --preset <N>, --screenshot <path>, --cam <x> <y> <z> <yaw> <pitch>, --time <N>, --blizzard
        std::string screenshotPath = "";
        int initialPreset = 1;
        bool hasCustomCam = false;
        glm::vec3 customCamPos(0.0f);
        float customYaw = 0.0f, customPitch = 0.0f;

        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--screenshot" || std::string(argv[i]) == "--headless-screenshot") {
                screenshotPath = (i + 1 < argc) ? argv[i + 1] : "everest_step6.png";
            } else if (std::string(argv[i]) == "--preset" && i + 1 < argc) {
                initialPreset = std::atoi(argv[i + 1]);
            } else if (std::string(argv[i]) == "--time" && i + 1 < argc) {
                float t = static_cast<float>(std::atof(argv[i + 1]));
                if (t >= 0.0f && t <= 4.0f && std::floor(t) == t) {
                    weatherSystem.setTimeOfDayPreset(static_cast<whiteout::game::TimeOfDayPreset>(static_cast<int>(t)));
                } else {
                    if (t >= 10.5f && t <= 15.0f) weatherSystem.setTimeOfDayPreset(whiteout::game::TimeOfDayPreset::CrispNoon);
                    else if (t >= 4.5f && t <= 8.5f) weatherSystem.setTimeOfDayPreset(whiteout::game::TimeOfDayPreset::DawnAlpengluehen);
                    else if (t >= 17.0f && t <= 20.5f) weatherSystem.setTimeOfDayPreset(whiteout::game::TimeOfDayPreset::SunsetAlpengluehen);
                    else weatherSystem.setTimeOfDayPreset(whiteout::game::TimeOfDayPreset::MoonlitNight);
                }
            } else if (std::string(argv[i]) == "--blizzard") {
                weatherSystem.toggleBlizzard();
            } else if (std::string(argv[i]) == "--cam" && i + 5 < argc) {
                customCamPos.x = static_cast<float>(std::atof(argv[i + 1]));
                customCamPos.y = static_cast<float>(std::atof(argv[i + 2]));
                customCamPos.z = static_cast<float>(std::atof(argv[i + 3]));
                customYaw = static_cast<float>(std::atof(argv[i + 4]));
                customPitch = static_cast<float>(std::atof(argv[i + 5]));
                hasCustomCam = true;
            }
        }

        if (initialPreset >= 1 && initialPreset <= 5) {
            player.teleportToPreset(initialPreset);
        }

        if (hasCustomCam) {
            camera.setPosition(customCamPos);
            glm::vec3 lookDir;
            lookDir.x = std::cos(glm::radians(customYaw)) * std::cos(glm::radians(customPitch));
            lookDir.y = std::sin(glm::radians(customPitch));
            lookDir.z = std::sin(glm::radians(customYaw)) * std::cos(glm::radians(customPitch));
            camera.setLookAt(customCamPos + lookDir);
        }

        if (!screenshotPath.empty()) {
            std::cout << "[Engine] Screenshot mode active: Rendering frame to " << screenshotPath
                      << " (Preset " << initialPreset << " | " << weatherSystem.getWeatherTelemetry() << ")" << std::endl;
            for (int f = 0; f < 8; f++) {
                timer.tick();
                weatherSystem.update(timer.deltaTime());
                renderer.renderFrame(
                    camera,
                    timer.totalTime(),
                    weatherSystem.getSunDirection(),
                    weatherSystem.getSunColor(),
                    weatherSystem.getCloudDensity(),
                    weatherSystem.getCloudBase(),
                    weatherSystem.getBlizzardFactor(),
                    weatherSystem.getWindSpeed()
                );
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

            // Weather & atmosphere inputs
            if (input.toggleTimeOfDay) {
                weatherSystem.cycleTimeOfDay();
            }
            if (input.toggleBlizzard) {
                weatherSystem.toggleBlizzard();
            }
            if (input.toggleLiveWeather) {
                weatherSystem.toggleLiveWeather();
            }

            // Update weather simulation
            weatherSystem.update(timer.deltaTime());

            // Update player physics, collision, ground snapping, and camera
            player.update(timer.deltaTime(), input);

            // Render frame using Vulkan 1.4 Dynamic Rendering & Slang shader with full atmosphere
            renderer.renderFrame(
                camera,
                timer.totalTime(),
                weatherSystem.getSunDirection(),
                weatherSystem.getSunColor(),
                weatherSystem.getCloudDensity(),
                weatherSystem.getCloudBase(),
                weatherSystem.getBlizzardFactor(),
                weatherSystem.getWindSpeed()
            );

            // Realtime HUD & Telemetry in window title
            titleUpdateTimer += timer.deltaTime();
            if (titleUpdateTimer >= 0.15f) {
                titleUpdateTimer = 0.0f;
                std::stringstream title;
                title << "Whiteout Delarus | "
                      << std::fixed << std::setprecision(0) << timer.currentFps() << " FPS | "
                      << player.getTelemetryString() << " | "
                      << weatherSystem.getWeatherTelemetry();

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
