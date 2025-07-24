#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <atomic>

#include "UploadJob.h"

class VulkanRenderer;

class FastNoiseLite;

using BlockID = uint8_t;

#include "renderer/Vertex.h"

class Chunk
{
public:
    static constexpr int WIDTH = 16, HEIGHT = 256, DEPTH = 16;

    enum class State
    {
        INITIAL,
        TERRAIN_READY,
        CPU_MESH_READY,
        GPU_PENDING,
        GPU_READY
    };

    Chunk(glm::ivec3 pos);
    ~Chunk();

    void generateTerrain(FastNoiseLite &noise);
    void generateMesh(VulkanRenderer &renderer);
    void cleanup(VulkanRenderer &renderer);
    void markReady(VulkanRenderer &renderer);
    void buildMeshCpu();
    bool uploadMesh(VulkanRenderer &renderer);
    void buildMeshGreedy();

    State getState() const { return m_State.load(std::memory_order_acquire); }
    bool isReady() const { return m_State.load(std::memory_order_acquire) == State::GPU_READY; }
    uint32_t getIndexCount() const { return m_IndexCount; }
    VkBuffer getVertexBuffer() const { return m_VertexBuffer; }
    VkBuffer getIndexBuffer() const { return m_IndexBuffer; }
    const glm::mat4 &getModelMatrix() const { return m_ModelMatrix; }

private:
    glm::ivec3 m_Pos;
    glm::mat4 m_ModelMatrix;

    std::vector<BlockID> m_Blocks;
    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;
    uint32_t m_IndexCount = 0;

    VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_VertexBufferAllocation = VK_NULL_HANDLE;

    VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_IndexBufferAllocation = VK_NULL_HANDLE;

    UploadJob m_Upload;
    std::atomic<State> m_State;

    BlockID getBlock(int x, int y, int z) const;
};