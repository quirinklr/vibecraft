#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <atomic>
#include "UploadJob.h"

struct Vertex;
class VulkanRenderer;
class FastNoiseLite;
using BlockID = uint8_t;

class Chunk
{
public:
    static constexpr int WIDTH = 32;
    static constexpr int HEIGHT = 256;
    static constexpr int DEPTH = 32;

    enum class State
    {
        EMPTY,
        TERRAIN_READY,
        CPU_MESH_READY,
        GPU_PENDING,
        GPU_READY
    };

    explicit Chunk(glm::ivec3 pos);
    ~Chunk();

    void generateTerrain(FastNoiseLite &noise);
    void buildMeshCpu();
    bool uploadMesh(VulkanRenderer &renderer);
    void markReady(VkDevice device);

    bool isReady() const { return m_State.load(std::memory_order_relaxed) == State::GPU_READY; }
    const VkBuffer &getVertexBuffer() const { return m_VertexBuffer; }
    const VkBuffer &getIndexBuffer() const { return m_IndexBuffer; }
    uint32_t getIndexCount() const { return m_IndexCount; }
    const glm::mat4 &getModelMatrix() const { return m_ModelMatrix; }

    void generateMesh(VulkanRenderer &renderer);
    void cleanup(VulkanRenderer &renderer);

private:
    BlockID getBlock(int x, int y, int z) const;

    glm::ivec3 m_Pos{};
    glm::mat4 m_ModelMatrix{1.0f};
    std::vector<BlockID> m_Blocks;
    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;

    VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;
    uint32_t m_IndexCount = 0;

    UploadJob m_Upload;
    std::atomic<State> m_State{State::EMPTY};
};
