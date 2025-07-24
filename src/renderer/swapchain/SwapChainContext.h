#pragma once

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"
#include "../../Window.h"
#include "../../Settings.h"

class SwapChainContext {
public:
    SwapChainContext(const Window& window, const Settings& settings, const InstanceContext& instanceContext, const DeviceContext& deviceContext);
    ~SwapChainContext();

    void recreateSwapChain();

    VkSwapchainKHR getSwapChain() const { return m_SwapChain.get(); }
    VkFormat getSwapChainImageFormat() const { return m_SwapChainImageFormat; }
    VkExtent2D getSwapChainExtent() const { return m_SwapChainExtent; }
    const std::vector<VulkanHandle<VkImageView, ImageViewDeleter>>& getSwapChainImageViews() const { return m_SwapChainImageViews; }
    VkRenderPass getRenderPass() const { return m_RenderPass.get(); }
    const std::vector<VulkanHandle<VkFramebuffer, FramebufferDeleter>>& getFramebuffers() const { return m_SwapChainFramebuffers; }
    const std::vector<VkImage>& getSwapChainImages() const { return m_SwapChainImages; }

private:
    void createSwapChainAndDependencies();
    void cleanupSwapChain();

    struct SwapChainSupportDetails;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void createImageViews();
    void createRenderPass();
    void createDepthResources();
    void createFramebuffers();

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();

    const Window& m_Window;
    const Settings& m_Settings;
    const InstanceContext& m_InstanceContext;
    const DeviceContext& m_DeviceContext;

    VulkanHandle<VkSwapchainKHR, SwapchainDeleter> m_SwapChain;
    std::vector<VkImage> m_SwapChainImages;
    VkFormat m_SwapChainImageFormat{};
    VkExtent2D m_SwapChainExtent{};
    std::vector<VulkanHandle<VkImageView, ImageViewDeleter>> m_SwapChainImageViews;
    VulkanHandle<VkRenderPass, RenderPassDeleter> m_RenderPass;
    std::vector<VulkanHandle<VkFramebuffer, FramebufferDeleter>> m_SwapChainFramebuffers;
    VmaImage m_DepthImage;
    VulkanHandle<VkImageView, ImageViewDeleter> m_DepthImageView;
};
