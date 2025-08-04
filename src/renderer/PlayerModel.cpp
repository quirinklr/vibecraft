#include "PlayerModel.h"
#include "Vertex.h"
#include "resources/UploadHelpers.h"
#include <vector>
PlayerModel::PlayerModel(const DeviceContext &dc, VkCommandPool pool) : m_dc(dc)
{

    std::vector<Vertex> headVertices, bodyVertices, leftArmVertices, rightArmVertices, leftLegVertices, rightLegVertices;
    std::vector<uint32_t> headIndices, bodyIndices, leftArmIndices, rightArmIndices, leftLegIndices, rightLegIndices;

    const glm::vec2 TEX_DIMS(64.0f, 64.0f);

    addBox(headVertices, headIndices, {-4, 24, -4}, {8, 8, 8}, {8, 0}, {16, 0}, {0, 8}, {16, 8}, {8, 8}, {24, 8}, TEX_DIMS);
    addBox(headVertices, headIndices, {-4.5, 23.5, -4.5}, {9, 9, 9}, {40, 0}, {48, 0}, {32, 8}, {48, 8}, {40, 8}, {56, 8}, TEX_DIMS);

    addBox(bodyVertices, bodyIndices, {-4, 12, -2}, {8, 12, 4}, {20, 16}, {28, 16}, {16, 20}, {28, 20}, {20, 20}, {32, 20}, TEX_DIMS);
    addBox(bodyVertices, bodyIndices, {-4.25, 11.75, -2.25}, {8.5, 12.5, 4.5}, {20, 32}, {28, 32}, {16, 36}, {28, 36}, {20, 36}, {32, 36}, TEX_DIMS);

    addBox(leftArmVertices, leftArmIndices, {-8, 12, -2}, {4, 12, 4}, {44, 16}, {48, 16}, {40, 20}, {48, 20}, {44, 20}, {52, 20}, TEX_DIMS);
    addBox(leftArmVertices, leftArmIndices, {-8.25, 11.75, -2.25}, {4.5, 12.5, 4.5}, {44, 32}, {48, 32}, {40, 36}, {48, 36}, {44, 36}, {52, 36}, TEX_DIMS);

    addBox(rightArmVertices, rightArmIndices, {4, 12, -2}, {4, 12, 4}, {36, 48}, {40, 48}, {32, 52}, {40, 52}, {36, 52}, {44, 52}, TEX_DIMS);
    addBox(rightArmVertices, rightArmIndices, {3.75, 11.75, -2.25}, {4.5, 12.5, 4.5}, {52, 48}, {56, 48}, {48, 52}, {56, 52}, {52, 52}, {60, 52}, TEX_DIMS);

    addBox(leftLegVertices, leftLegIndices, {-4, 0, -2}, {4, 12, 4}, {4, 16}, {8, 16}, {0, 20}, {8, 20}, {4, 20}, {12, 20}, TEX_DIMS);
    addBox(leftLegVertices, leftLegIndices, {-4.25, -0.25, -2.25}, {4.5, 12.5, 4.5}, {4, 32}, {8, 32}, {0, 36}, {8, 36}, {4, 36}, {12, 36}, TEX_DIMS);

    addBox(rightLegVertices, rightLegIndices, {0, 0, -2}, {4, 12, 4}, {20, 48}, {24, 48}, {16, 52}, {24, 52}, {20, 52}, {28, 52}, TEX_DIMS);
    addBox(rightLegVertices, rightLegIndices, {-0.25, -0.25, -2.25}, {4.5, 12.5, 4.5}, {4, 48}, {8, 48}, {0, 52}, {8, 52}, {4, 52}, {12, 52}, TEX_DIMS);

    m_headIndexCount = static_cast<uint32_t>(headIndices.size());
    m_headVB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, headVertices.data(), headVertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_headIB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, headIndices.data(), headIndices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    m_bodyIndexCount = static_cast<uint32_t>(bodyIndices.size());
    m_bodyVB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, bodyVertices.data(), bodyVertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_bodyIB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, bodyIndices.data(), bodyIndices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    m_leftArmIndexCount = static_cast<uint32_t>(leftArmIndices.size());
    m_leftArmVB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, leftArmVertices.data(), leftArmVertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_leftArmIB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, leftArmIndices.data(), leftArmIndices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    m_rightArmIndexCount = static_cast<uint32_t>(rightArmIndices.size());
    m_rightArmVB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, rightArmVertices.data(), rightArmVertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_rightArmIB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, rightArmIndices.data(), rightArmIndices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    m_leftLegIndexCount = static_cast<uint32_t>(leftLegIndices.size());
    m_leftLegVB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, leftLegVertices.data(), leftLegVertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_leftLegIB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, leftLegIndices.data(), leftLegIndices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    m_rightLegIndexCount = static_cast<uint32_t>(rightLegIndices.size());
    m_rightLegVB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, rightLegVertices.data(), rightLegVertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_rightLegIB = UploadHelpers::createDeviceLocalBufferFromData(dc, pool, rightLegIndices.data(), rightLegIndices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void PlayerModel::addBox(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices, const glm::vec3 &offset, const glm::vec3 &dims, const glm::vec2 &uv_top, const glm::vec2 &uv_bottom, const glm::vec2 &uv_left, const glm::vec2 &uv_right, const glm::vec2 &uv_front, const glm::vec2 &uv_back, const glm::vec2 &tex_dims)
{
    float x = offset.x / 16.0f;
    float y = offset.y / 16.0f;
    float z = offset.z / 16.0f;
    float w = dims.x / 16.0f;
    float h = dims.y / 16.0f;
    float d = dims.z / 16.0f;

    float W = dims.x;
    float H = dims.y;
    float D = dims.z;

    uint32_t base_index = static_cast<uint32_t>(vertices.size());

    vertices.push_back({{x, y, z + d}, {}, {uv_front.x / tex_dims.x, (uv_front.y + H) / tex_dims.y}});
    vertices.push_back({{x + w, y, z + d}, {}, {(uv_front.x + W) / tex_dims.x, (uv_front.y + H) / tex_dims.y}});
    vertices.push_back({{x + w, y + h, z + d}, {}, {(uv_front.x + W) / tex_dims.x, uv_front.y / tex_dims.y}});
    vertices.push_back({{x, y + h, z + d}, {}, {uv_front.x / tex_dims.x, uv_front.y / tex_dims.y}});

    vertices.push_back({{x + w, y, z}, {}, {(uv_back.x + W) / tex_dims.x, (uv_back.y + H) / tex_dims.y}});
    vertices.push_back({{x, y, z}, {}, {uv_back.x / tex_dims.x, (uv_back.y + H) / tex_dims.y}});
    vertices.push_back({{x, y + h, z}, {}, {uv_back.x / tex_dims.x, uv_back.y / tex_dims.y}});
    vertices.push_back({{x + w, y + h, z}, {}, {(uv_back.x + W) / tex_dims.x, uv_back.y / tex_dims.y}});

    vertices.push_back({{x, y, z}, {}, {(uv_left.x + D) / tex_dims.x, (uv_left.y + H) / tex_dims.y}});
    vertices.push_back({{x, y, z + d}, {}, {uv_left.x / tex_dims.x, (uv_left.y + H) / tex_dims.y}});
    vertices.push_back({{x, y + h, z + d}, {}, {uv_left.x / tex_dims.x, uv_left.y / tex_dims.y}});
    vertices.push_back({{x, y + h, z}, {}, {(uv_left.x + D) / tex_dims.x, uv_left.y / tex_dims.y}});

    vertices.push_back({{x + w, y, z + d}, {}, {(uv_right.x + D) / tex_dims.x, (uv_right.y + H) / tex_dims.y}});
    vertices.push_back({{x + w, y, z}, {}, {uv_right.x / tex_dims.x, (uv_right.y + H) / tex_dims.y}});
    vertices.push_back({{x + w, y + h, z}, {}, {uv_right.x / tex_dims.x, uv_right.y / tex_dims.y}});
    vertices.push_back({{x + w, y + h, z + d}, {}, {(uv_right.x + D) / tex_dims.x, uv_right.y / tex_dims.y}});

    vertices.push_back({{x, y + h, z + d}, {}, {uv_top.x / tex_dims.x, (uv_top.y + D) / tex_dims.y}});
    vertices.push_back({{x + w, y + h, z + d}, {}, {(uv_top.x + W) / tex_dims.x, (uv_top.y + D) / tex_dims.y}});
    vertices.push_back({{x + w, y + h, z}, {}, {(uv_top.x + W) / tex_dims.x, uv_top.y / tex_dims.y}});
    vertices.push_back({{x, y + h, z}, {}, {uv_top.x / tex_dims.x, uv_top.y / tex_dims.y}});

    vertices.push_back({{x, y, z}, {}, {uv_bottom.x / tex_dims.x, (uv_bottom.y + D) / tex_dims.y}});
    vertices.push_back({{x + w, y, z}, {}, {(uv_bottom.x + W) / tex_dims.x, (uv_bottom.y + D) / tex_dims.y}});
    vertices.push_back({{x + w, y, z + d}, {}, {(uv_bottom.x + W) / tex_dims.x, uv_bottom.y / tex_dims.y}});
    vertices.push_back({{x, y, z + d}, {}, {uv_bottom.x / tex_dims.x, uv_bottom.y / tex_dims.y}});

    uint32_t bi;

    bi = base_index + 0;
    indices.insert(indices.end(), {bi, bi + 1, bi + 2, bi, bi + 2, bi + 3});

    bi = base_index + 4;
    indices.insert(indices.end(), {bi, bi + 1, bi + 2, bi, bi + 2, bi + 3});

    bi = base_index + 8;
    indices.insert(indices.end(), {bi, bi + 1, bi + 2, bi, bi + 2, bi + 3});

    bi = base_index + 12;
    indices.insert(indices.end(), {bi, bi + 1, bi + 2, bi, bi + 2, bi + 3});

    bi = base_index + 16;
    indices.insert(indices.end(), {bi, bi + 1, bi + 2, bi, bi + 2, bi + 3});

    bi = base_index + 20;
    indices.insert(indices.end(), {bi, bi + 1, bi + 2, bi, bi + 2, bi + 3});
}

void PlayerModel::drawHead(VkCommandBuffer cmd) const
{
    VkBuffer vbs[] = {m_headVB.get()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_headIB.get(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_headIndexCount, 1, 0, 0, 0);
}

void PlayerModel::drawBody(VkCommandBuffer cmd) const
{
    VkBuffer vbs[] = {m_bodyVB.get()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_bodyIB.get(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_bodyIndexCount, 1, 0, 0, 0);
}

void PlayerModel::drawLeftArm(VkCommandBuffer cmd) const
{
    VkBuffer vbs[] = {m_leftArmVB.get()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_leftArmIB.get(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_leftArmIndexCount, 1, 0, 0, 0);
}

void PlayerModel::drawRightArm(VkCommandBuffer cmd) const
{
    VkBuffer vbs[] = {m_rightArmVB.get()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_rightArmIB.get(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_rightArmIndexCount, 1, 0, 0, 0);
}

void PlayerModel::drawLeftLeg(VkCommandBuffer cmd) const
{
    VkBuffer vbs[] = {m_leftLegVB.get()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_leftLegIB.get(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_leftLegIndexCount, 1, 0, 0, 0);
}

void PlayerModel::drawRightLeg(VkCommandBuffer cmd) const
{
    VkBuffer vbs[] = {m_rightLegVB.get()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_rightLegIB.get(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_rightLegIndexCount, 1, 0, 0, 0);
}