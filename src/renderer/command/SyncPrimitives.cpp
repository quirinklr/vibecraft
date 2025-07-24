#include "SyncPrimitives.h"
#include <stdexcept>

SyncPrimitives::SyncPrimitives(const DeviceContext& deviceContext, const SwapChainContext& swapChainContext)
    : m_DeviceContext(deviceContext), m_SwapChainContext(swapChainContext) {
    createSyncObjects();
}

SyncPrimitives::~SyncPrimitives() {}

void SyncPrimitives::createSyncObjects() {
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_ImagesInFlight.resize(m_SwapChainContext.getSwapChainImages().size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkSemaphore sema1, sema2;
        VkFence fence;
        if (vkCreateSemaphore(m_DeviceContext.getDevice(), &semaphoreInfo, nullptr, &sema1) != VK_SUCCESS ||
            vkCreateSemaphore(m_DeviceContext.getDevice(), &semaphoreInfo, nullptr, &sema2) != VK_SUCCESS ||
            vkCreateFence(m_DeviceContext.getDevice(), &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
        m_ImageAvailableSemaphores[i] = VulkanHandle<VkSemaphore, SemaphoreDeleter>(sema1, {m_DeviceContext.getDevice()});
        m_RenderFinishedSemaphores[i] = VulkanHandle<VkSemaphore, SemaphoreDeleter>(sema2, {m_DeviceContext.getDevice()});
        m_InFlightFences[i] = VulkanHandle<VkFence, FenceDeleter>(fence, {m_DeviceContext.getDevice()});
    }
}
