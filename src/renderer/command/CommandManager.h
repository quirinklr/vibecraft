#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <map>
#include <optional>
#include "../../math/Ivec3Less.h"

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"
#include "../swapchain/SwapChainContext.h"
#include "../pipeline/PipelineCache.h"
#include "../../Chunk.h"
#include "../../Camera.h"
#include "../RendererConfig.h"
#include "../RayTracingPushConstants.h"
#include "../../Item.h"
#include <vulkan/vulkan.h>

struct SkyPushConstant
{
    glm::mat4 model;
    alignas(4) int is_sun;
};

class CommandManager
{
public:
    CommandManager(const DeviceContext &deviceContext, const SwapChainContext &swapChainContext, const PipelineCache &pipelineCache);
    ~CommandManager();

    void recordCommandBuffer(
        uint32_t imageIndex, uint32_t currentFrame,
        const std::vector<std::pair<Chunk *, int>> &opaqueChunks,
        const std::vector<std::pair<Chunk *, int>> &transparentChunks,
        const std::vector<VkDescriptorSet> &descriptorSets,
        const glm::vec3 &clearColor,
        const SkyPushConstant &sun_pc,
        const SkyPushConstant &moon_pc,
        bool isSunVisible,
        bool isMoonVisible,
        VkBuffer skySphereVB, VkBuffer skySphereIB, uint32_t skySphereIndexCount,
        VkBuffer crosshairVB, VkBuffer crosshairIB, VkDescriptorSet crosshairDS,
        VkBuffer debugCubeVB, VkBuffer debugCubeIB, uint32_t debugCubeIndexCount,
        const Settings &settings, const std::vector<AABB> &debugAABBs,
        VkBuffer outlineVB,
        uint32_t outlineVertexCount,
        const std::optional<glm::ivec3> &hoveredBlockPos,
        const std::vector<std::unique_ptr<Item>> &items,
        VkBuffer itemVB, VkBuffer itemIB, uint32_t itemIndexCount);

    VkCommandBuffer getCommandBuffer(uint32_t index) const { return m_CommandBuffers[index]; }
    VkCommandPool getCommandPool() const { return m_CommandPool.get(); }
    void recordRayTraceCommand(VkCommandBuffer cb, uint32_t currentFrame, VkDescriptorSet rtDescriptorSet,
                               const VkStridedDeviceAddressRegionKHR *rgenRegion,
                               const VkStridedDeviceAddressRegionKHR *missRegion,
                               const VkStridedDeviceAddressRegionKHR *hitRegion,
                               const VkStridedDeviceAddressRegionKHR *callRegion,
                               const void *pushConstants, VkImage shadowImage);

private:
    void createCommandPool();
    void createCommandBuffers();

    const DeviceContext &m_DeviceContext;
    const SwapChainContext &m_SwapChainContext;
    const PipelineCache &m_PipelineCache;

    VulkanHandle<VkCommandPool, CommandPoolDeleter> m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;
};