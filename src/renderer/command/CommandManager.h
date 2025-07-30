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

    void recordCommandBuffer(
        uint32_t imageIndex, uint32_t currentFrame,
        const std::vector<std::pair<const Chunk *, int>> &chunksToRender,
        const std::vector<VkDescriptorSet> &descriptorSets,
        VkBuffer crosshairVertexBuffer,
        VkBuffer debugCubeVB, VkBuffer debugCubeIB, uint32_t debugCubeIndexCount,
        const Settings &settings,
        const std::vector<AABB> &debugAABBs);
        
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
