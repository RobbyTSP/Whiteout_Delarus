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

WindowEventState Window::pollEvents() {
    SDL_Event event;
    m_lastEvents.mouseDeltaX = 0.0f;
    m_lastEvents.mouseDeltaY = 0.0f;

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
                        if (isDown) m_lastEvents.shouldClose = true;
                        break;
                    case SDLK_w: m_lastEvents.moveForward = isDown; break;
                    case SDLK_s: m_lastEvents.moveBackward = isDown; break;
                    case SDLK_a: m_lastEvents.moveLeft = isDown; break;
                    case SDLK_d: m_lastEvents.moveRight = isDown; break;
                    case SDLK_q: m_lastEvents.moveDown = isDown; break;
                    case SDLK_e: m_lastEvents.moveUp = isDown; break;
                    case SDLK_SPACE: m_lastEvents.moveUp = isDown; break;
                    case SDLK_LCTRL: m_lastEvents.moveDown = isDown; break;
                    case SDLK_LSHIFT: m_lastEvents.sprint = isDown; break;
                    default: break;
                }
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_LEFT) {
                    m_lastEvents.rightMouseDown = true;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_LEFT) {
                    m_lastEvents.rightMouseDown = false;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                break;

            case SDL_MOUSEMOTION:
                if (m_lastEvents.rightMouseDown) {
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
