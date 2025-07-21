#include "Engine.h"

#include <iostream>
#include <glm/gtc/constants.hpp>
#include <string>
#include <sstream> // Für die String-Formatierung

Engine::Engine()
{
    // Verstecke und fange den Cursor, wenn das Fenster fokussiert ist
    glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

Engine::~Engine()
{
    // Destruktor bleibt leer
}

void Engine::run() {
    bool mouseCaptured = true;
    glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    float lastFrameTime = 0.f;
    double lastX, lastY;
    glfwGetCursorPos(m_Window.getGLFWwindow(), &lastX, &lastY);
    float yaw = glm::pi<float>();
    float pitch = 0.0f;
    
    float lastFPSTime = 0.f;
    int frameCount = 0;

    while (!m_Window.shouldClose()) {
        float currentFrameTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrameTime - lastFrameTime;

        // --- KORREKTE FPS-CAP LOGIK ---
        if (!m_Settings.vsync && m_Settings.fpsCap > 0) {
            float minFrameTime = 1.0f / m_Settings.fpsCap;
            if (deltaTime < minFrameTime) {
                continue; 
            }
        }
        // WICHTIG: deltaTime wird erst NACH dem Cap aktualisiert
        lastFrameTime = currentFrameTime;

        // FPS-Titel aktualisieren (mit Yaw/Pitch)
        frameCount++;
        if (currentFrameTime - lastFPSTime >= 1.0) {
            std::stringstream titleStream;
            titleStream.precision(1);
            titleStream << std::fixed << "Minecraft Vibe Engine | FPS: " << frameCount 
                        << " | Yaw: " << glm::degrees(yaw) 
                        << " | Pitch: " << glm::degrees(pitch);
            
            glfwSetWindowTitle(m_Window.getGLFWwindow(), titleStream.str().c_str());
            frameCount = 0;
            lastFPSTime = currentFrameTime;
        }

        // ... (Rest der Funktion: Input, Kamera, Draw Call - bleibt unverändert)
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            mouseCaptured = false;
        }
        if (!mouseCaptured && glfwGetMouseButton(m_Window.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
             glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
             mouseCaptured = true;
             glfwGetCursorPos(m_Window.getGLFWwindow(), &lastX, &lastY);
        }

        if (mouseCaptured) {
            double mouseX, mouseY;
            glfwGetCursorPos(m_Window.getGLFWwindow(), &mouseX, &mouseY);

            float deltaX = static_cast<float>(mouseX - lastX);
            float deltaY = static_cast<float>(mouseY - lastY);

            lastX = mouseX;
            lastY = mouseY;
            
            float lookSpeed = 0.1f;
            yaw   += deltaX * m_Settings.mouseSensitivityX * lookSpeed * deltaTime;
            
            if (m_Settings.invertMouseY) {
                pitch += deltaY * m_Settings.mouseSensitivityY * lookSpeed * deltaTime;
            } else {
                pitch -= deltaY * m_Settings.mouseSensitivityY * lookSpeed * deltaTime;
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

        glfwPollEvents();
        m_Renderer.drawFrame(m_Camera);
    }
}