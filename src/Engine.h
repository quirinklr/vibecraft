#pragma once

#include "Window.h"
#include "VulkanRenderer.h"
#include "Camera.h"
#include "Settings.h"
#include "GameObject.h" // -> HINZUFÜGEN
#include <memory>
#include <vector>        // -> HINZUFÜGEN

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
    Settings m_Settings{};
    VulkanRenderer m_Renderer{m_Window, m_Settings}; 
    Camera m_Camera{};

    // -> DIESE ZEILE HINZUFÜGEN
    std::vector<GameObject> m_GameObjects; 
};