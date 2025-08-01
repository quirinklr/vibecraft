#pragma once

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"
#include "../swapchain/SwapChainContext.h"
#include "../RendererConfig.h"

class SyncPrimitives {
public:
    SyncPrimitives(const DeviceContext& deviceContext, const SwapChainContext& swapChainContext);
    ~SyncPrimitives();

    VkSemaphore getImageAvailableSemaphore(uint32_t index) const { return m_ImageAvailableSemaphores[index].get(); }
    VkSemaphore getRenderFinishedSemaphore(uint32_t index) const { return m_RenderFinishedSemaphores[index].get(); }
    VkFence getInFlightFence(uint32_t index) const { return m_InFlightFences[index].get(); }
    VkFence* getInFlightFencePtr(uint32_t index) { return m_InFlightFences[index].p(); }

private:
    void createSyncObjects();

    const DeviceContext& m_DeviceContext;
    const SwapChainContext& m_SwapChainContext;

    std::vector<VulkanHandle<VkSemaphore, SemaphoreDeleter>> m_ImageAvailableSemaphores;
    std::vector<VulkanHandle<VkSemaphore, SemaphoreDeleter>> m_RenderFinishedSemaphores;
    std::vector<VulkanHandle<VkFence, FenceDeleter>> m_InFlightFences;
};
