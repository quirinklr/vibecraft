#pragma once

#include "Window.h"
#include "VulkanRenderer.h"
#include "Camera.h"
#include "Settings.h" // NEUES INCLUDE
#include <memory>

class Engine {
public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void run();

private:
    Window m_Window{WIDTH, HEIGHT, "Minecraft Vibe Engine"};
    VulkanRenderer m_Renderer{m_Window, m_Settings}; 
    Camera m_Camera{};
    
    // NEU: Settings-Objekt
    Settings m_Settings{};

    // Wir brauchen keine separaten Member mehr für die Mauslogik
    // float m_DeltaTime = 0.0f; <-- Wird zur lokalen Variable in run()
    // float m_LastFrame = 0.0f; <-- Wird zur lokalen Variable in run()
    // bool m_MouseCaptured = false; <-- Wird zur lokalen Variable in run()
};