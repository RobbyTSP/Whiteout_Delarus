#pragma once

#include <chrono>

namespace whiteout::core {

class Timer {
public:
    Timer() : m_startTime(std::chrono::high_resolution_clock::now()), m_lastTime(m_startTime) {}

    void tick() {
        auto currentTime = std::chrono::high_resolution_clock::now();
        m_deltaTime = std::chrono::duration<float>(currentTime - m_lastTime).count();
        m_totalTime = std::chrono::duration<float>(currentTime - m_startTime).count();
        m_lastTime = currentTime;

        m_frameCount++;
        m_fpsTimer += m_deltaTime;
        if (m_fpsTimer >= 1.0f) {
            m_currentFps = static_cast<float>(m_frameCount) / m_fpsTimer;
            m_frameCount = 0;
            m_fpsTimer = 0.0f;
        }
    }

    [[nodiscard]] float deltaTime() const { return m_deltaTime; }
    [[nodiscard]] float totalTime() const { return m_totalTime; }
    [[nodiscard]] float currentFps() const { return m_currentFps; }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTime;
    float m_deltaTime = 0.016f;
    float m_totalTime = 0.0f;
    float m_fpsTimer = 0.0f;
    int m_frameCount = 0;
    float m_currentFps = 60.0f;
};

} // namespace whiteout::core
