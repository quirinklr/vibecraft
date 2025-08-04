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

    void drawHead(VkCommandBuffer cmd) const;
    void drawBody(VkCommandBuffer cmd) const;
    void drawLeftArm(VkCommandBuffer cmd) const;
    void drawRightArm(VkCommandBuffer cmd) const;
    void drawLeftLeg(VkCommandBuffer cmd) const;
    void drawRightLeg(VkCommandBuffer cmd) const;

private:
    void createModel();
    void addBox(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices,
                const glm::vec3 &offset, const glm::vec3 &dims,
                const glm::vec2 &uv_top, const glm::vec2 &uv_bottom,
                const glm::vec2 &uv_left, const glm::vec2 &uv_right,
                const glm::vec2 &uv_front, const glm::vec2 &uv_back,
                const glm::vec2 &tex_dims);

    const DeviceContext &m_dc;
    VmaBuffer m_headVB, m_headIB;
    uint32_t m_headIndexCount = 0;

    VmaBuffer m_bodyVB, m_bodyIB;
    uint32_t m_bodyIndexCount = 0;

    VmaBuffer m_leftArmVB, m_leftArmIB;
    uint32_t m_leftArmIndexCount = 0;

    VmaBuffer m_rightArmVB, m_rightArmIB;
    uint32_t m_rightArmIndexCount = 0;

    VmaBuffer m_leftLegVB, m_leftLegIB;
    uint32_t m_leftLegIndexCount = 0;

    VmaBuffer m_rightLegVB, m_rightLegIB;
    uint32_t m_rightLegIndexCount = 0;
};