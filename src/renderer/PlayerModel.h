#pragma once

#include "../VulkanWrappers.h"
#include "core/DeviceContext.h"
#include <glm/glm.hpp>
#include <vector>

struct Vertex;

class PlayerModel
{
public:
    PlayerModel(const DeviceContext &dc, VkCommandPool pool);
    ~PlayerModel() = default;

    PlayerModel(const PlayerModel &) = delete;
    PlayerModel &operator=(const PlayerModel &) = delete;

    void draw(VkCommandBuffer cmd) const;

    VkBuffer getVertexBuffer() const { return m_vertexBuffer.get(); }
    VkBuffer getIndexBuffer() const { return m_indexBuffer.get(); }
    uint32_t getIndexCount() const { return m_indexCount; }

private:
    void createModel();
    void addBox(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices,
                const glm::vec3 &offset, const glm::vec3 &dims,
                const glm::vec2 &uv_top, const glm::vec2 &uv_bottom,
                const glm::vec2 &uv_left, const glm::vec2 &uv_right,
                const glm::vec2 &uv_front, const glm::vec2 &uv_back,
                const glm::vec2 &tex_dims);

    const DeviceContext &m_dc;
    VmaBuffer m_vertexBuffer;
    VmaBuffer m_indexBuffer;
    uint32_t m_indexCount = 0;
};