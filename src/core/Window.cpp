#include "Window.hpp"
#include <SDL2/SDL_vulkan.h>
#include <stdexcept>
#include <iostream>

namespace whiteout::core {

Window::Window(const std::string& title, int width, int height)
    : m_width(width), m_height(height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        throw std::runtime_error(std::string("Failed to initialize SDL2: ") + SDL_GetError());
    }

    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_width,
        m_height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );

    if (!m_window) {
        throw std::runtime_error(std::string("Failed to create SDL2 Vulkan Window: ") + SDL_GetError());
    }
}

Window::~Window() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

std::vector<const char*> Window::getRequiredInstanceExtensions() const {
    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(m_window, &count, nullptr)) {
        throw std::runtime_error(std::string("Failed to get Vulkan instance extension count: ") + SDL_GetError());
    }

    std::vector<const char*> extensions(count);
    if (!SDL_Vulkan_GetInstanceExtensions(m_window, &count, extensions.data())) {
        throw std::runtime_error(std::string("Failed to get Vulkan instance extensions: ") + SDL_GetError());
    }

    return extensions;
}

bool Window::createVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) const {
    if (!SDL_Vulkan_CreateSurface(m_window, instance, surface)) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
        return false;
    }
    return true;
}

void Window::setMouseCapture(bool capture) {
    m_mouseCaptured = capture;
    SDL_SetRelativeMouseMode(capture ? SDL_TRUE : SDL_FALSE);
}

WindowEventState Window::pollEvents() {
    SDL_Event event;
    m_lastEvents.mouseDeltaX = 0.0f;
    m_lastEvents.mouseDeltaY = 0.0f;
    m_lastEvents.toggleMode = false;
    m_lastEvents.teleportPreset = 0;
    m_lastEvents.toggleTimeOfDay = false;
    m_lastEvents.toggleBlizzard = false;
    m_lastEvents.toggleLiveWeather = false;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                m_lastEvents.shouldClose = true;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                    event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    m_width = event.window.data1;
                    m_height = event.window.data2;
                    m_lastEvents.resized = true;
                    m_lastEvents.newWidth = m_width;
                    m_lastEvents.newHeight = m_height;
                }
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                bool isDown = (event.type == SDL_KEYDOWN);
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        if (isDown) {
                            if (m_mouseCaptured) {
                                setMouseCapture(false);
                            } else {
                                m_lastEvents.shouldClose = true;
                            }
                        }
                        break;
                    case SDLK_w: m_lastEvents.moveForward = isDown; break;
                    case SDLK_s: m_lastEvents.moveBackward = isDown; break;
                    case SDLK_a: m_lastEvents.moveLeft = isDown; break;
                    case SDLK_d: m_lastEvents.moveRight = isDown; break;
                    case SDLK_q: m_lastEvents.moveDown = isDown; break;
                    case SDLK_e: m_lastEvents.moveUp = isDown; break;
                    case SDLK_SPACE:
                        m_lastEvents.moveUp = isDown;
                        m_lastEvents.jump = isDown;
                        break;
                    case SDLK_c: m_lastEvents.crouch = isDown; break;
                    case SDLK_LCTRL:
                        m_lastEvents.moveDown = isDown;
                        m_lastEvents.crouch = isDown;
                        break;
                    case SDLK_LSHIFT: m_lastEvents.sprint = isDown; break;
                    case SDLK_v:
                    case SDLK_TAB:
                        if (isDown) m_lastEvents.toggleMode = true;
                        break;
                    case SDLK_1: if (isDown) m_lastEvents.teleportPreset = 1; break;
                    case SDLK_2: if (isDown) m_lastEvents.teleportPreset = 2; break;
                    case SDLK_3: if (isDown) m_lastEvents.teleportPreset = 3; break;
                    case SDLK_4: if (isDown) m_lastEvents.teleportPreset = 4; break;
                    case SDLK_5: if (isDown) m_lastEvents.teleportPreset = 5; break;
                    case SDLK_t: if (isDown) m_lastEvents.toggleTimeOfDay = true; break;
                    case SDLK_b: if (isDown) m_lastEvents.toggleBlizzard = true; break;
                    case SDLK_l: if (isDown) m_lastEvents.toggleLiveWeather = true; break;
                    default: break;
                }
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    m_lastEvents.rightMouseDown = true;
                    setMouseCapture(true);
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    m_lastEvents.leftMouseDown = true;
                    setMouseCapture(true);
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    m_lastEvents.rightMouseDown = false;
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    m_lastEvents.leftMouseDown = false;
                }
                break;

            case SDL_MOUSEMOTION:
                if (m_mouseCaptured || m_lastEvents.rightMouseDown) {
                    m_lastEvents.mouseDeltaX += static_cast<float>(event.motion.xrel);
                    m_lastEvents.mouseDeltaY += static_cast<float>(event.motion.yrel);
                }
                break;

            default:
                break;
        }
    }

    return m_lastEvents;
}

} // namespace whiteout::core
