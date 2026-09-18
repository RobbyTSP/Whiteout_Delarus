#include "core/Window.hpp"
#include "core/Camera.hpp"
#include "core/Timer.hpp"
#include "renderer/Renderer.hpp"
#include "game/TerrainCollider.hpp"
#include "game/Player.hpp"
#include "game/WeatherSystem.hpp"
#include "game/SnowpackSimulation.hpp"
#include "audio/AlpineAudioEngine.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "=========================================================\n";
    std::cout << " WHITEOUT DELARUS: 1:1 HIMALAYA ENGINE - STEP 24 (1:1 PART XXII)\n";
    std::cout << " Dynamic Snow Creep, Slab Fractures & Weak-Layer Avalanche Mechanics\n";
    std::cout << " Snowpack: Stratified Cohesive Slab, Depth Hoar Weak-Layer & Bed Surface\n";
    std::cout << " Mechanics: Surcharge Stress & Stability S, Downhill Creep, Leeward Wind Drift\n";
    std::cout << " Crown Fracture: Propagating Anrisskante Step, Whumpf Acoustics & Slab Release\n";
    std::cout << " Acoustics: 35km DEM Acoustic Raytracing, Nuptse Echoes & Aeolian Ridge Wind\n";
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
    std::cout << "   - 8: Fast Travel -> Mount Everest Summit (8,848m) [3DGS Gebetsfahnen & Stativ]\n";
    std::cout << "   - T: Cycle Time of Day (Dawn Alpenglühen -> Noon -> Sunset -> Night)\n";
    std::cout << "   - B: Toggle Blizzard / Whiteout Mode (30m Visibility & Spindrift)\n";
    std::cout << "   - L: Toggle Live Open-Meteo Weather Synchronization\n";
    std::cout << "   - K: Trigger Slab Fracture & Avalanche (Weak-Layer Collapse & Crown Tear)\n";
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

        // Live Weather & Atmosphere System (Open-Meteo + Alpenglühen + Wolkenmeer)
        whiteout::game::WeatherSystem weatherSystem(DATA_DIR "/weather/everest_current.json");

        // First-person player character controller
        whiteout::game::Player player(camera, collider, &weatherSystem);

        // Wave-Based Alpine Audio Raytracing & Procedural Synthesis (Step 23)
        whiteout::audio::AlpineAudioEngine audioEngine;
        audioEngine.init(&collider);

        // Step 24: Snowpack Stratification & Avalanche Mechanics Callbacks
        auto& snowpack = player.getSnowpack();
        snowpack.setWhumpfCallback([&audioEngine](const glm::vec3& pos, float intensity) {
            audioEngine.triggerWhumpf(pos, intensity);
        });
        snowpack.setCrownSnapCallback([&audioEngine](const glm::vec3& pos, float crackLength) {
            audioEngine.triggerCrownSnap(pos, crackLength);
        });
        snowpack.setAvalancheReleaseCallback([&renderer](const glm::vec3& pos, const std::vector<glm::vec3>& crownPath) {
            (void)crownPath;
            renderer.triggerAvalanche(pos);
        });

        whiteout::core::Timer timer;

        // Check for CLI arguments: --preset <N>, --screenshot <path>, --cam <x> <y> <z> <yaw> <pitch>, --time <N>, --blizzard
        std::string screenshotPath = "";
        std::string recordAudioPath = "";
        float recordAudioDuration = 5.0f;
        int initialPreset = 1;
        bool hasCustomCam = false;
        glm::vec3 customCamPos(0.0f);
        float customYaw = 0.0f, customPitch = 0.0f;
        bool triggerAvalancheOnStart = false;
        bool hasGogglesOverride = false;
        bool overrideGoggles = true;
        float overrideFog = -1.0f;
        float overrideFrost = -1.0f;
        float overrideHypoxia = -1.0f;

        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--screenshot" || std::string(argv[i]) == "--headless-screenshot") {
                screenshotPath = (i + 1 < argc) ? argv[i + 1] : "everest_step6.png";
            } else if (std::string(argv[i]) == "--record-audio" && i + 1 < argc) {
                recordAudioPath = argv[++i];
            } else if (std::string(argv[i]) == "--record-duration" && i + 1 < argc) {
                recordAudioDuration = static_cast<float>(std::atof(argv[++i]));
            } else if (std::string(argv[i]) == "--preset" && i + 1 < argc) {
                initialPreset = std::atoi(argv[i + 1]);
            } else if (std::string(argv[i]) == "--avalanche") {
                triggerAvalancheOnStart = true;
            } else if (std::string(argv[i]) == "--no-goggles") {
                hasGogglesOverride = true;
                overrideGoggles = false;
            } else if (std::string(argv[i]) == "--goggles") {
                hasGogglesOverride = true;
                overrideGoggles = true;
            } else if (std::string(argv[i]) == "--fog" && i + 1 < argc) {
                overrideFog = static_cast<float>(std::atof(argv[++i]));
            } else if (std::string(argv[i]) == "--frost" && i + 1 < argc) {
                overrideFrost = static_cast<float>(std::atof(argv[++i]));
            } else if (std::string(argv[i]) == "--hypoxia" && i + 1 < argc) {
                overrideHypoxia = static_cast<float>(std::atof(argv[++i]));
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
            } else if (std::string(argv[i]) == "--hotspot" && i + 1 < argc) {
                std::string hs = argv[++i];
                if (hs == "summit" || hs == "everest") {
                    customCamPos = glm::vec3(-8463.5f, 8756.2f, -8053.5f);
                    customYaw = -77.0f;
                    customPitch = -12.0f;
                    hasCustomCam = true;
                    initialPreset = 8;
                    player.teleportToPreset(8);
                } else if (hs == "hillary" || hs == "step") {
                    customCamPos = glm::vec3(-8500.0f, 8754.0f, -8002.0f);
                    customYaw = 90.0f;
                    customPitch = 16.0f;
                    hasCustomCam = true;
                    initialPreset = 2;
                    player.teleportToPreset(2);
                } else if (hs == "southcol" || hs == "camp4" || hs == "col") {
                    customCamPos = glm::vec3(-7743.0f, 8386.5f, -4991.5f);
                    customYaw = -49.4f;
                    customPitch = -18.0f;
                    hasCustomCam = true;
                    initialPreset = 3;
                    player.teleportToPreset(3);
                }
            } else if (std::string(argv[i]) == "--cam" && i + 5 < argc) {
                customCamPos.x = static_cast<float>(std::atof(argv[i + 1]));
                customCamPos.y = static_cast<float>(std::atof(argv[i + 2]));
                customCamPos.z = static_cast<float>(std::atof(argv[i + 3]));
                customYaw = static_cast<float>(std::atof(argv[i + 4]));
                customPitch = static_cast<float>(std::atof(argv[i + 5]));
                hasCustomCam = true;
            }
        }

        if (hasGogglesOverride) {
            player.setGogglesEquipped(overrideGoggles);
        }
        if (overrideFog >= 0.0f) {
            player.setGogglesFog(overrideFog);
        }
        if (overrideFrost >= 0.0f) {
            player.setGogglesFrost(overrideFrost);
        }
        if (overrideHypoxia >= 0.0f) {
            player.setHypoxiaFactor(overrideHypoxia);
        }

        if (!hasCustomCam && initialPreset >= 1 && initialPreset <= 8) {
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

        if (triggerAvalancheOnStart) {
            player.triggerSlabFracture(0.85f);
            renderer.triggerAvalanche(player.getPosition());
            audioEngine.triggerAvalanche();
        }

        if (!recordAudioPath.empty() && screenshotPath.empty()) {
            std::cout << "[Engine] Headless audio recording active: Rendering WAV to " << recordAudioPath
                      << " (Preset " << initialPreset << " | " << weatherSystem.getWeatherTelemetry() << ")" << std::endl;
            for (int f = 0; f < 16; f++) {
                timer.tick();
                weatherSystem.update(0.016f);
                whiteout::core::WindowEventState idleInput{};
                player.update(0.016f, idleInput);
                audioEngine.update(0.016f, camera, player, weatherSystem);
            }
            if (triggerAvalancheOnStart) {
                audioEngine.triggerAvalanche();
                audioEngine.triggerWhumpf(player.getPosition(), 1.8f);
            }
            audioEngine.renderToWav(recordAudioPath, recordAudioDuration, camera, player, weatherSystem);
            std::cout << "[Engine] Audio rendering successfully completed. Exiting." << std::endl;
            return 0;
        }

        if (!screenshotPath.empty()) {
            std::cout << "[Engine] Screenshot mode active: Rendering frame to " << screenshotPath
                      << " (Preset " << initialPreset << " | " << weatherSystem.getWeatherTelemetry() << ")" << std::endl;

            // Step 20: Queue demonstration crampon footsteps in front of camera
            glm::vec3 camPos = camera.getPosition();
            glm::vec3 basePos = hasCustomCam ? camPos : player.getPosition();
            glm::vec3 camFwd = camera.getForward();
            glm::vec3 fwdH = glm::vec3(camFwd.x, 0.0f, camFwd.z);
            if (glm::length(fwdH) > 0.001f) {
                fwdH = glm::normalize(fwdH);
            } else {
                fwdH = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            glm::vec3 rgtH = glm::normalize(glm::cross(fwdH, glm::vec3(0.0f, 1.0f, 0.0f)));

            for (int s = 0; s < 4; s++) {
                float dist = 0.9f + float(s) * 0.70f;
                float side = (s % 2 == 0) ? -0.18f : 0.18f;
                glm::vec3 stepPos = basePos + fwdH * dist + rgtH * side;
                glm::vec4 posRadius(stepPos.x, stepPos.y, stepPos.z, 0.30f);
                glm::vec4 dirDepth(fwdH.x, fwdH.z, 0.12f, 0.92f);
                renderer.queueFootstep(posRadius, dirDepth);
            }

            int warmupFrames = triggerAvalancheOnStart ? 45 : 16;
            for (int f = 0; f < warmupFrames; f++) {
                timer.tick();
                weatherSystem.update(timer.deltaTime());
                whiteout::core::WindowEventState idleInput{};
                player.update(timer.deltaTime(), idleInput);
                if (hasGogglesOverride) player.setGogglesEquipped(overrideGoggles);
                if (overrideFog >= 0.0f) player.setGogglesFog(overrideFog);
                if (overrideFrost >= 0.0f) player.setGogglesFrost(overrideFrost);
                if (overrideHypoxia >= 0.0f) player.setHypoxiaFactor(overrideHypoxia);

                if (hasCustomCam) {
                    camera.setPosition(customCamPos);
                    glm::vec3 lookDir;
                    lookDir.x = std::cos(glm::radians(customYaw)) * std::cos(glm::radians(customPitch));
                    lookDir.y = std::sin(glm::radians(customPitch));
                    lookDir.z = std::sin(glm::radians(customYaw)) * std::cos(glm::radians(customPitch));
                    camera.setLookAt(customCamPos + lookDir);
                }

                audioEngine.update(timer.deltaTime(), camera, player, weatherSystem);

                glm::vec4 crownOrigin(0.0f);
                glm::vec4 crownParams(0.0f);
                const auto& crown = player.getSnowpack().getActiveCrownFracture();
                if (crown.active) {
                    crownOrigin = glm::vec4(crown.originWorldPos.x, crown.originWorldPos.z, crown.currentRadiusMeters, 1.0f);
                    float angle = std::atan2(crown.propagationDir.z, crown.propagationDir.x);
                    crownParams = glm::vec4(
                        crown.slabStepHeightMeters,
                        angle,
                        player.getSnowpack().getDynamics().creepVelocityMmPerHour,
                        1.0f
                    );
                }

                renderer.renderFrame(
                    camera,
                    timer.totalTime(),
                    weatherSystem.getSunDirection(),
                    weatherSystem.getSunColor(),
                    weatherSystem.getCloudDensity(),
                    weatherSystem.getCloudBase(),
                    weatherSystem.getBlizzardFactor(),
                    weatherSystem.getWindSpeed(),
                    player.getCryoOpticsState(),
                    crownOrigin,
                    crownParams
                );
            }
            renderer.saveScreenshot(screenshotPath);
            std::cout << "[Engine] Screenshot successfully captured." << std::endl;

            if (!recordAudioPath.empty()) {
                if (triggerAvalancheOnStart) {
                    audioEngine.triggerAvalanche();
                    audioEngine.triggerWhumpf(player.getPosition(), 1.8f);
                }
                audioEngine.renderToWav(recordAudioPath, recordAudioDuration, camera, player, weatherSystem);
            }

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
            if (input.triggerAvalanche) {
                player.triggerSlabFracture(0.85f);
                renderer.triggerAvalanche(player.getPosition());
                audioEngine.triggerAvalanche();
            }

            // Update weather simulation
            weatherSystem.update(timer.deltaTime());

            // Update player physics, collision, ground snapping, and camera
            player.update(timer.deltaTime(), input);

            // Forward player footsteps to renderer for snow deformation
            const auto& recentSteps = player.getRecentFootsteps();
            for (const auto& step : recentSteps) {
                renderer.queueFootstep(step.posRadius, step.dirDepth);
            }
            player.clearRecentFootsteps();

            // Forward player footsteps to audio engine for material-specific crampon acoustics
            const auto& recentAudioSteps = player.getRecentAudioSteps();
            for (const auto& step : recentAudioSteps) {
                audioEngine.triggerFootstep(step.surfaceType, step.intensity, step.isLeftFoot);
            }
            player.clearRecentAudioSteps();

            // Update wave-based alpine acoustics & raytracing
            audioEngine.update(timer.deltaTime(), camera, player, weatherSystem);

            // Compute Step 24 Crown Fracture & Slab Failure Parameters
            glm::vec4 crownOrigin(0.0f);
            glm::vec4 crownParams(0.0f);
            const auto& crown = player.getSnowpack().getActiveCrownFracture();
            if (crown.active) {
                crownOrigin = glm::vec4(crown.originWorldPos.x, crown.originWorldPos.z, crown.currentRadiusMeters, 1.0f);
                float angle = std::atan2(crown.propagationDir.z, crown.propagationDir.x);
                crownParams = glm::vec4(
                    crown.slabStepHeightMeters,
                    angle,
                    player.getSnowpack().getDynamics().creepVelocityMmPerHour,
                    1.0f
                );
            }

            // Render frame using Vulkan 1.4 Dynamic Rendering & Slang shader with full atmosphere
            renderer.renderFrame(
                camera,
                timer.totalTime(),
                weatherSystem.getSunDirection(),
                weatherSystem.getSunColor(),
                weatherSystem.getCloudDensity(),
                weatherSystem.getCloudBase(),
                weatherSystem.getBlizzardFactor(),
                weatherSystem.getWindSpeed(),
                player.getCryoOpticsState(),
                crownOrigin,
                crownParams
            );

            // Realtime HUD & Telemetry in window title
            titleUpdateTimer += timer.deltaTime();
            if (titleUpdateTimer >= 0.15f) {
                titleUpdateTimer = 0.0f;
                std::stringstream title;
                title << "Whiteout Delarus | "
                      << std::fixed << std::setprecision(0) << timer.currentFps() << " FPS | "
                      << player.getTelemetryString() << " | "
                      << weatherSystem.getWeatherTelemetry() << " | "
                      << audioEngine.getAudioTelemetry();

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
