#pragma once

#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

namespace whiteout::core {

struct WindowEventState {
    bool shouldClose = false;
    bool resized = false;
    int newWidth = 0;
    int newHeight = 0;

    // Movement & Action keys
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool moveUp = false;
    bool moveDown = false;
    bool sprint = false;
    bool jump = false;
    bool crouch = false;
    bool toggleMode = false;
    int teleportPreset = 0; // 1 = Base Camp, 2 = Summit, 3 = Lhotse, 4 = Ama Dablam
    bool toggleTimeOfDay = false;
    bool toggleBlizzard = false;
    bool toggleLiveWeather = false;
    bool triggerAvalanche = false;
    bool triggerCrevasseCollapse = false;
    bool toggleGoggles = false;

    // Mouse state
    bool rightMouseDown = false;
    bool leftMouseDown = false;
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;
};

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] SDL_Window* getNativeHandle() const { return m_window; }
    [[nodiscard]] int getWidth() const { return m_width; }
    [[nodiscard]] int getHeight() const { return m_height; }
    [[nodiscard]] float getAspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }

    void setMouseCapture(bool capture);
    [[nodiscard]] bool isMouseCaptured() const { return m_mouseCaptured; }

    [[nodiscard]] std::vector<const char*> getRequiredInstanceExtensions() const;
    bool createVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) const;

    WindowEventState pollEvents();
    void resetResizeFlag() { m_lastEvents.resized = false; }

private:
    SDL_Window* m_window = nullptr;
    int m_width = 1280;
    int m_height = 720;
    bool m_mouseCaptured = false;
    WindowEventState m_lastEvents;
};

} // namespace whiteout::core
