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
#include <set>
#include <mutex>
#include <utility>

struct ChunkLodRequestLess
{
    bool operator()(const std::pair<glm::ivec3, int> &a, const std::pair<glm::ivec3, int> &b) const
    {
        if (a.second != b.second)
            return a.second < b.second;
        if (a.first.x != b.first.x)
            return a.first.x < b.first.x;
        if (a.first.y != b.first.y)
            return a.first.y < b.first.y;
        return a.first.z < b.first.z;
    }
};

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
    void processInput(float dt, bool &mouse, double &lx, double &ly, float &yaw, float &pitch, glm::vec3 &cam, float &baseSpeed);
    void updateCamera(const glm::vec3 &cam, float yaw, float pitch);
    void updateWindowTitle(float now, float &fpsTime, int &frames, float yaw, float pitch, float baseSpeed);

    void updateChunks(const glm::vec3 &cameraPos);
    void unloadDistantChunks(const glm::ivec3 &playerChunkPos);
    void processGarbage();
    void loadVisibleChunks(const glm::ivec3 &playerChunkPos);
    void createMeshJobs(const glm::ivec3 &playerChunkPos);
    void submitMeshJobs();
    void uploadReadyMeshes();
    void createChunkContainer(const glm::ivec3 &pos);

    Settings m_Settings{};
    Window m_Window{WIDTH, HEIGHT, "Vibecraft", m_Settings};
    VulkanRenderer m_Renderer{m_Window, m_Settings};
    Camera m_Camera{};
    TerrainGenerator m_TerrainGen;
    double m_FrameEMA = 0.004;

    mutable std::mutex m_MeshJobsMutex;
    std::set<std::pair<glm::ivec3, int>, ChunkLodRequestLess> m_MeshJobsToCreate;
    std::set<std::pair<glm::ivec3, int>, ChunkLodRequestLess> m_MeshJobsInProgress;

    std::map<glm::ivec3, std::shared_ptr<Chunk>, ivec3_less> m_Chunks;
    std::vector<std::shared_ptr<Chunk>> m_Garbage;
    ThreadPool m_Pool;

    std::set<glm::ivec3, ivec3_less> m_ChunksToGenerate;
    std::mutex m_ChunkGenerationQueueMtx;
};
