#pragma once

#include "Window.h"
#include "VulkanRenderer.h"
#include "Camera.h"
#include "Settings.h"
#include "Chunk.h"
#include <map>
#include <glm/glm.hpp>
#include <memory>
#include "math/Ivec3Less.h"

class Engine
{
public:
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;

    Engine();
    ~Engine();

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    void run();

private:
    Window m_Window{WIDTH, HEIGHT, "Minecraft Vibe Engine"};
    Settings m_Settings{};
    VulkanRenderer m_Renderer{m_Window, m_Settings};
    Camera m_Camera{};

    // --- Chunk‑Verwaltung ---
    static constexpr int RENDER_DISTANCE = 6; // Radius in Chunks
    std::map<glm::ivec3, std::unique_ptr<Chunk>, ivec3_less> m_Chunks;

    void generateChunk(const glm::ivec3 &pos);     // neu
    void updateChunks(const glm::vec3 &cameraPos); // neu
};