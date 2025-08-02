#pragma once

#include "VulkanWrappers.h"
#include "core/DeviceContext.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

class Player;
class VulkanRenderer;
struct Settings;

class DebugOverlay
{
public:
    DebugOverlay(DeviceContext &deviceContext, VkCommandPool commandPool);
    ~DebugOverlay();

    void update(const Player &player, const Settings &settings, float fps, int64_t seed);
    void draw(VkCommandBuffer commandBuffer);

    void recreate(VkRenderPass renderPass, VkExtent2D viewportExtent);

private:
    void createFontTexture();

    void createPipeline(VkRenderPass renderPass);
    void cleanupPipeline();
    void generateStringMesh(float x, float y, const std::string &text, std::vector<glm::vec4> &vertices);

    DeviceContext &m_deviceContext;
    VkCommandPool m_commandPool;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkExtent2D m_viewportExtent;

    VmaImage m_fontImage;
    VulkanHandle<VkImageView, ImageViewDeleter> m_fontImageView;
    VulkanHandle<VkSampler, SamplerDeleter> m_fontSampler;

    VulkanHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter> m_descriptorSetLayout;
    VulkanHandle<VkDescriptorPool, DescriptorPoolDeleter> m_descriptorPool;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_pipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_pipeline;

    VmaBuffer m_vertexBuffer;
    uint32_t m_vertexCount = 0;

    std::string m_gpuName;
    std::string m_raytracingSupport;
};