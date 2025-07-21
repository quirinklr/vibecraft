#include "Engine.h"

#include <iostream>
#include <glm/gtc/constants.hpp>

Engine::Engine()
{
    // Konstruktor bleibt leer, da Initialisierung in den Membern erfolgt
}

Engine::~Engine()
{
    // Destruktor bleibt leer, da Aufräumen in den Member-Destruktoren erfolgt
}

void Engine::run()
{
    // Initialisiere Maus-Position
    double lastX = 0.0, lastY = 0.0;
    glfwGetCursorPos(m_Window.getGLFWwindow(), &lastX, &lastY);

    float yaw = glm::pi<float>(); // Blickrichtung initial nach -Z
    float pitch = 0.0f;

    while (!m_Window.shouldClose())
    {
        glfwPollEvents();

        // Maus-Input für Kamera-Rotation
        double mouseX, mouseY;
        glfwGetCursorPos(m_Window.getGLFWwindow(), &mouseX, &mouseY);
        float deltaX = static_cast<float>(mouseX - lastX);
        float deltaY = static_cast<float>(mouseY - lastY);
        lastX = mouseX;
        lastY = mouseY;

        // Empfindlichkeit anpassen
        const float lookSpeed = 0.005f;
        yaw -= deltaX * lookSpeed;
        pitch -= deltaY * lookSpeed;
        pitch = glm::clamp(pitch, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);

        // Blickrichtung aus Yaw/Pitch berechnen
        glm::vec3 direction;
        direction.x = cos(yaw) * cos(pitch);
        direction.y = sin(pitch);
        direction.z = sin(yaw) * cos(pitch);
        glm::vec3 cameraFront = glm::normalize(direction);

        // Kamera-Position und Projektion setzen
        glm::vec3 cameraPos = {2.f, 2.f, 2.f};
        m_Camera.setViewDirection(cameraPos, cameraFront);

        auto extent = m_Window.getExtent();
        m_Camera.setPerspectiveProjection(glm::radians(45.f), (float)extent.width / (float)extent.height, 0.1f, 10.f);

        // Kamera an den Renderer übergeben und zeichnen
        m_Renderer.drawFrame(m_Camera);
    }
}