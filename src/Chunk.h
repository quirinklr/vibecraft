#pragma once

#include "Block.h"
#include <glm/glm.hpp>
#include <vector>
#include <atomic>
#include <map>
#include "UploadJob.h"
#include "math/AABB.h"
#include <mutex>
#include <array>
#include <renderer/resources/RingStagingArena.h>

class VulkanRenderer;
class FastNoiseLite;

#include "renderer/Vertex.h"

class Chunk;

struct ChunkMeshInput
{
    static constexpr int WIDTH = 16;
    static constexpr int HEIGHT = 256;
    static constexpr int DEPTH = 16;

    static constexpr int CACHED_WIDTH = WIDTH + 2;
    static constexpr int CACHED_HEIGHT = HEIGHT;
    static constexpr int CACHED_DEPTH = DEPTH + 2;

    std::shared_ptr<Chunk> selfChunk = nullptr;
    std::array<std::shared_ptr<Chunk>, 8> neighborChunks{};

    std::vector<Block> cachedBlocks;

public:
    ChunkMeshInput()
    {
        cachedBlocks.resize(CACHED_WIDTH * CACHED_HEIGHT * CACHED_DEPTH);
    }

    Block getBlock(int x, int y, int z) const
    {
        if (x < 0 || x >= CACHED_WIDTH || y < 0 || y >= CACHED_HEIGHT || z < 0 || z >= CACHED_DEPTH)
        {
            return {BlockId::AIR};
        }
        return cachedBlocks[y * CACHED_DEPTH * CACHED_WIDTH + z * CACHED_WIDTH + x];
    }
};

struct ChunkMesh
{
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexBufferAllocation = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation indexBufferAllocation = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
};

class Chunk
{
public:
    AABB getAABB() const;
    static constexpr int WIDTH = 16, HEIGHT = 256, DEPTH = 16;
    enum class State
    {
        INITIAL,
        TERRAIN_READY,
        MESHING,
        STAGING_READY,
        UPLOADING,
        GPU_READY
    };

    Chunk(glm::ivec3 pos);
    ~Chunk();

    void generateTerrain(FastNoiseLite &noise);
    void cleanup(VulkanRenderer &renderer);
    void markReady(VulkanRenderer &renderer);

    void buildAndStageMesh(VmaAllocator allocator, RingStagingArena &arena,
                           int lodLevel, ChunkMeshInput &meshInput);

    void buildAndStageDebugMesh(VmaAllocator allocator, RingStagingArena &arena);

    bool uploadMesh(VulkanRenderer &renderer, int lodLevel);
    bool uploadTransparentMesh(VulkanRenderer &renderer, int lodLevel);

    const std::vector<Block> &getBlocks() const { return m_Blocks; }

    State getState() const { return m_State.load(std::memory_order_acquire); }
    bool hasLOD(int lodLevel) const;
    int getBestAvailableLOD(int requiredLod) const;

    const ChunkMesh *getMesh(int lodLevel) const;
    const ChunkMesh *getTransparentMesh(int lodLevel) const;
    const ChunkMesh *getDebugMesh() const { return &m_DebugMesh; }

    const glm::mat4 &getModelMatrix() const { return m_ModelMatrix; }
    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, Block block);
    glm::ivec3 getPos() const { return m_Pos; }

    std::atomic<State> m_State;
    std::atomic<int> m_Flags{0};
    std::atomic<bool> m_is_dirty{false};

    mutable std::mutex m_PendingMutex;
    mutable std::mutex m_MeshesMutex;

private:
    void buildMeshGreedy(int lodLevel,
                         std::vector<Vertex> &outOpaqueVertices, std::vector<uint32_t> &outOpaqueIndices,
                         std::vector<Vertex> &outTransparentVertices, std::vector<uint32_t> &outTransparentIndices,
                         ChunkMeshInput &meshInput);

    glm::ivec3 m_Pos;
    glm::mat4 m_ModelMatrix;
    std::vector<Block> m_Blocks;

    std::map<int, ChunkMesh> m_Meshes;
    std::map<int, ChunkMesh> m_TransparentMeshes;
    ChunkMesh m_DebugMesh;
    std::map<int, UploadJob> m_PendingUploads;
    std::map<int, UploadJob> m_PendingTransparentUploads;
};