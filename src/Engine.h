#pragma once
#include "Window.h"
#include "VulkanRenderer.h"
#include "Camera.h"
#include "Settings.h"
#include "Chunk.h"
#include <map>
#include <memory>
#include <glm/glm.hpp>
#include "math/Ivec3Less.h"
#include "generation/TerrainGenerator.h"
#include "ThreadPool.h"

class Engine
{
public:
    static constexpr int WIDTH = 1920;
    static constexpr int HEIGHT = 1080;
    Engine();
    ~Engine();
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    void run();

private:
    Settings m_Settings{};
    Window m_Window{WIDTH, HEIGHT, "Vibecraft", m_Settings};
    VulkanRenderer m_Renderer{m_Window, m_Settings};
    Camera m_Camera{};
    TerrainGenerator m_TerrainGen;
    static constexpr int RENDER_DISTANCE = 12;
    std::map<glm::ivec3, std::unique_ptr<Chunk>, ivec3_less> m_Chunks;
    std::vector<std::unique_ptr<Chunk>> m_Garbage;
    ThreadPool m_Pool;
    void generateChunk(const glm::ivec3 &pos);
    void updateChunks(const glm::vec3 &cameraPos);
};
