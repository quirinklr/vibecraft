#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include "VulkanRenderer.h" 

class FastNoiseLite;
using BlockID = uint8_t;

class Chunk {
public:
    static constexpr int WIDTH = 32;
    static constexpr int HEIGHT = 256;
    static constexpr int DEPTH = 32;

    Chunk(glm::ivec3 position);
    ~Chunk();

    void generateTerrain(FastNoiseLite &noise);
    void generateMesh(VulkanRenderer &renderer);
    void cleanup(VkDevice device);

    bool isMeshGenerated() const { return m_IndexCount > 0; }
    const VkBuffer &getVertexBuffer() const { return m_VertexBuffer; }
    const VkBuffer &getIndexBuffer() const { return m_IndexBuffer; }
    uint32_t getIndexCount() const { return m_IndexCount; }
    const glm::mat4 &getModelMatrix() const { return m_ModelMatrix; }

private:
    BlockID getBlock(int x, int y, int z) const;

    glm::ivec3 m_Position;
    std::vector<BlockID> m_Blocks;
    glm::mat4 m_ModelMatrix;

    VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;
    uint32_t m_IndexCount = 0;
};