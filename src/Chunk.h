#pragma once

#include "BlockIds.h"
#include <glm/glm.hpp>
#include <vector>
#include <atomic>
#include <map>
#include "UploadJob.h"
#include "math/AABB.h"
#include <mutex>

class VulkanRenderer;
class FastNoiseLite;

#include "renderer/Vertex.h"

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

    void buildAndStageMesh(VmaAllocator allocator, int lodLevel);
    bool uploadMesh(VulkanRenderer &renderer, int lodLevel);

    State getState() const { return m_State.load(std::memory_order_acquire); }
    bool hasLOD(int lodLevel) const;
    int getBestAvailableLOD(int requiredLod) const;

    const ChunkMesh *getMesh(int lodLevel) const;

    const glm::mat4 &getModelMatrix() const { return m_ModelMatrix; }
    BlockID getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockID id);
    glm::ivec3 getPos() const { return m_Pos; }

    std::atomic<State> m_State;
    std::atomic<int> m_Flags{0};

    mutable std::mutex m_PendingMutex;
    mutable std::mutex m_MeshesMutex;

private:
    void buildMeshGreedy(int lodLevel, std::vector<Vertex> &outVertices, std::vector<uint32_t> &outIndices);

    glm::ivec3 m_Pos;
    glm::mat4 m_ModelMatrix;
    std::vector<BlockID> m_Blocks;

    std::map<int, ChunkMesh> m_Meshes;
    std::map<int, UploadJob> m_PendingUploads;
};