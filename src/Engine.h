#pragma once
#include "Window.h"
#include "VulkanRenderer.h"
#include "Settings.h"
#include "Chunk.h"
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>
#include "math/Ivec3Less.h"
#include "generation/TerrainGenerator.h"
#include "ThreadPool.h"
#include <set>
#include <mutex>
#include <utility>
#include "Entity.h"
#include "Player.h"
#include "DebugController.h"
#include <optional>

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

    Block get_block(int x, int y, int z);
    void set_block(int x, int y, int z, BlockId id);

    Window &get_window() { return m_Window; }
    Settings &getSettings() { return m_Settings; }
    void advanceTime(int32_t ticks);

    void generateBlockOutline(const glm::ivec3 &pos, std::vector<glm::vec3> &vertices);

private:
    void processInput(float dt, bool &mouse_enabled, double &lx, double &ly);
    void updateWindowTitle(float now, float &fpsTime, int &frames, const glm::vec3 &player_pos);
    void updateChunks(const glm::vec3 &cameraPos);

    void unloadDistantChunks(const glm::ivec3 &playerChunkPos);
    void processGarbage();
    void loadVisibleChunks(const glm::ivec3 &playerChunkPos);
    void createMeshJobs(const glm::ivec3 &playerChunkPos);
    void submitMeshJobs();
    void uploadReadyMeshes();
    void createChunkContainer(const glm::ivec3 &pos);

    Settings m_Settings{};
    Window m_Window;
    VulkanRenderer m_Renderer;
    DebugController m_debugController;

    bool m_showDebugOverlay = false;
    bool m_key_Z_last_state = false;
    uint32_t m_gameTicks = 6000;
    float m_ticksPerSecond = 20.0f;
    float m_timeAccumulator = 0.0f;
    float m_physicsAccumulator = 0.0f;

    std::vector<std::unique_ptr<Entity>> m_entities;
    Player *m_player_ptr = nullptr;

    TerrainGenerator m_TerrainGen;
    std::optional<glm::ivec3> m_hoveredBlockPos;

    double m_FrameEMA = 0.004;

    mutable std::mutex m_MeshJobsMutex;
    std::set<std::pair<glm::ivec3, int>, ChunkLodRequestLess> m_MeshJobsToCreate;
    std::set<std::pair<glm::ivec3, int>, ChunkLodRequestLess> m_MeshJobsInProgress;

    mutable std::mutex m_ChunksMutex;

    std::unordered_map<glm::ivec3, std::shared_ptr<Chunk>, ivec3_hasher> m_Chunks;
    std::vector<std::shared_ptr<Chunk>> m_Garbage;
    ThreadPool m_Pool;

    std::set<glm::ivec3, ivec3_less> m_ChunksToGenerate;
    std::mutex m_ChunkGenerationQueueMtx;
};