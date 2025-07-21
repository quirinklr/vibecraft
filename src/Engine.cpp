#include "Engine.h"

#include <iostream>
#include <glm/gtc/constants.hpp>
#include <string>
#include <sstream> // Für die String-Formatierung
#include <thread>
#include <chrono>

Engine::Engine()
{
    // Verstecke und fange den Cursor, wenn das Fenster fokussiert ist
    glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

Engine::~Engine()
{
    // Destruktor bleibt leer
}

void Engine::run()
{
    bool mouseCaptured = true;
    glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    float lastFrameTime = static_cast<float>(glfwGetTime());
    double lastX, lastY;
    glfwGetCursorPos(m_Window.getGLFWwindow(), &lastX, &lastY);
    float yaw = glm::pi<float>();
    float pitch = 0.0f;

    float lastFPSTime = lastFrameTime;
    int frameCount = 0;

    constexpr float baseMouseScale = 0.0005f;

    while (!m_Window.shouldClose())
    {
        float currentFrameTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrameTime - lastFrameTime;

        glfwPollEvents();

        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            mouseCaptured = false;
        }
        if (!mouseCaptured && glfwGetMouseButton(m_Window.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            mouseCaptured = true;
            glfwGetCursorPos(m_Window.getGLFWwindow(), &lastX, &lastY);
        }

        if (mouseCaptured)
        {
            double mouseX, mouseY;
            glfwGetCursorPos(m_Window.getGLFWwindow(), &mouseX, &mouseY);

            float deltaX = static_cast<float>(mouseX - lastX);
            float deltaY = static_cast<float>(mouseY - lastY);

            lastX = mouseX;
            lastY = mouseY;

            float yawScale = m_Settings.mouseSensitivityX * baseMouseScale;
            float pitchScale = m_Settings.mouseSensitivityY * baseMouseScale;

            yaw += deltaX * yawScale;
            if (m_Settings.invertMouseY)
            {
                pitch += deltaY * pitchScale;
            }
            else
            {
                pitch -= deltaY * pitchScale;
            }
            pitch = glm::clamp(pitch, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
        }

        glm::vec3 direction;
        direction.x = cos(yaw) * cos(pitch);
        direction.y = sin(pitch);
        direction.z = sin(yaw) * cos(pitch);
        glm::vec3 cameraFront = glm::normalize(direction);

        glm::vec3 cameraPos = {2.f, 2.f, 2.f};
        m_Camera.setViewDirection(cameraPos, cameraFront);

        auto extent = m_Window.getExtent();
        m_Camera.setPerspectiveProjection(glm::radians(45.f), (float)extent.width / (float)extent.height, 0.1f, 100.f);

        auto frameStart = std::chrono::high_resolution_clock::now();

        m_Renderer.drawFrame(m_Camera);

        frameCount++;
        if (currentFrameTime - lastFPSTime >= 1.0f)
        {
            std::stringstream titleStream;
            titleStream.precision(1);
            titleStream << std::fixed << "Minecraft Vibe Engine | FPS: " << frameCount
                        << " | Yaw: " << glm::degrees(yaw)
                        << " | Pitch: " << glm::degrees(pitch);
            glfwSetWindowTitle(m_Window.getGLFWwindow(), titleStream.str().c_str());
            frameCount = 0;
            lastFPSTime = currentFrameTime;
        }

        if (!m_Settings.vsync && m_Settings.fpsCap > 0)
        {
            auto minFrameTime = std::chrono::duration<float>(1.0f / m_Settings.fpsCap);
            auto frameEnd = std::chrono::high_resolution_clock::now();
            auto spent = frameEnd - frameStart;
            if (spent < minFrameTime)
                std::this_thread::sleep_for(minFrameTime - spent);
        }

        lastFrameTime = currentFrameTime;
    }
}
