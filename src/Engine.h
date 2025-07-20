#pragma once

#include "Window.h"
#include "VulkanRenderer.h"
#include <memory>

class Engine {
public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    Engine();
    ~Engine();

    // Verhindert Kopieren
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void run();

private:
    Window m_Window{WIDTH, HEIGHT, "Minecraft Vibe Engine"};
    VulkanRenderer m_Renderer{m_Window};
};