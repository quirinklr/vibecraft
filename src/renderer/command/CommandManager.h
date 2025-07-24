#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <map>
#include "../../math/Ivec3Less.h"

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"
#include "../swapchain/SwapChainContext.h"
#include "../pipeline/PipelineCache.h"
#include "../../Chunk.h"
#include "../../Camera.h"
#include "../RendererConfig.h"

class CommandManager
{
public:
    CommandManager(const DeviceContext &deviceContext, const SwapChainContext &swapChainContext, const PipelineCache &pipelineCache);
    ~CommandManager();

    void recordCommandBuffer(uint32_t imageIndex,
                             uint32_t currentFrame,
                             const std::map<glm::ivec3,
                                            std::unique_ptr<Chunk>,
                                            ivec3_less> &chunks,
                             const std::vector<VkDescriptorSet> &descriptorSets,
                             VkBuffer crosshairVertexBuffer);
    VkCommandBuffer getCommandBuffer(uint32_t index) const { return m_CommandBuffers[index]; }
    VkCommandPool getCommandPool() const { return m_CommandPool.get(); }

private:
    void createCommandPool();
    void createCommandBuffers();

    const DeviceContext &m_DeviceContext;
    const SwapChainContext &m_SwapChainContext;
    const PipelineCache &m_PipelineCache;

    VulkanHandle<VkCommandPool, CommandPoolDeleter> m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;
};
