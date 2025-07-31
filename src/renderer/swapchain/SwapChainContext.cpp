#include "SwapChainContext.h"
#include "../resources/TextureManager.h"
#include <stdexcept>
#include <algorithm>
#include <array>

struct SwapChainContext::SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainContext::SwapChainContext(const Window &window, const Settings &settings, const InstanceContext &instanceContext, const DeviceContext &deviceContext)
    : m_Window(window), m_Settings(settings), m_InstanceContext(instanceContext), m_DeviceContext(deviceContext)
{
    createSwapChainAndDependencies();
}

SwapChainContext::~SwapChainContext()
{
    cleanupSwapChain();
}

void SwapChainContext::createSwapChainAndDependencies()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_DeviceContext.getPhysicalDevice());
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_InstanceContext.getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    DeviceContext::QueueFamilyIndices indices = m_DeviceContext.findQueueFamilies(m_DeviceContext.getPhysicalDevice());
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    if (vkCreateSwapchainKHR(m_DeviceContext.getDevice(), &createInfo, nullptr, &swapchain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }
    m_SwapChain = VulkanHandle<VkSwapchainKHR, SwapchainDeleter>(swapchain, {m_DeviceContext.getDevice()});

    vkGetSwapchainImagesKHR(m_DeviceContext.getDevice(), m_SwapChain.get(), &imageCount, nullptr);
    m_SwapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_DeviceContext.getDevice(), m_SwapChain.get(), &imageCount, m_SwapChainImages.data());
    m_SwapChainImageFormat = surfaceFormat.format;
    m_SwapChainExtent = extent;

    createImageViews();
    createRenderPass();
    createDepthResources();
    createFramebuffers();
}

void SwapChainContext::cleanupSwapChain()
{
    m_DepthImageView = {nullptr, {}};
    m_DepthImage = {};
    m_SwapChainFramebuffers.clear();
    m_RenderPass = {nullptr, {}};
    m_SwapChainImageViews.clear();
    m_SwapChain = {nullptr, {}};
}

void SwapChainContext::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_Window.getGLFWwindow(), &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_Window.getGLFWwindow(), &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(m_DeviceContext.getDevice());
    cleanupSwapChain();
    createSwapChainAndDependencies();
}

SwapChainContext::SwapChainSupportDetails SwapChainContext::querySwapChainSupport(const VkPhysicalDevice pdevice)
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdevice, m_InstanceContext.getSurface(), &details.capabilities);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, m_InstanceContext.getSurface(), &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, m_InstanceContext.getSurface(), &formatCount, details.formats.data());
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, m_InstanceContext.getSurface(), &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, m_InstanceContext.getSurface(), &presentModeCount, details.presentModes.data());
    }
    return details;
}

VkSurfaceFormatKHR SwapChainContext::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
    for (const auto &availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR SwapChainContext::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &modes)
{
    if (m_Settings.vsync)
        return VK_PRESENT_MODE_FIFO_KHR;

    for (auto m : modes)
        if (m == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return VK_PRESENT_MODE_IMMEDIATE_KHR;

    for (auto m : modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return VK_PRESENT_MODE_MAILBOX_KHR;

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChainContext::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(m_Window.getGLFWwindow(), &width, &height);
        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)};
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

void SwapChainContext::createImageViews()
{
    m_SwapChainImageViews.resize(m_SwapChainImages.size());
    for (size_t i = 0; i < m_SwapChainImages.size(); i++)
    {
        m_SwapChainImageViews[i] = TextureManager::createImageView(m_DeviceContext.getDevice(), m_SwapChainImages[i], m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void SwapChainContext::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_SwapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments{colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(m_DeviceContext.getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
    m_RenderPass = VulkanHandle<VkRenderPass, RenderPassDeleter>(renderPass, {m_DeviceContext.getDevice()});
}

void SwapChainContext::createDepthResources()
{
    VkFormat depthFormat = findDepthFormat();
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_SwapChainExtent.width;
    imageInfo.extent.height = m_SwapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    m_DepthImage = VmaImage(m_DeviceContext.getAllocator(), imageInfo, allocInfo);
    m_DepthImageView = TextureManager::createImageView(m_DeviceContext.getDevice(), m_DepthImage.get(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void SwapChainContext::createFramebuffers()
{
    m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());
    for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments = {m_SwapChainImageViews[i].get(), m_DepthImageView.get()};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass.get();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_SwapChainExtent.width;
        framebufferInfo.height = m_SwapChainExtent.height;
        framebufferInfo.layers = 1;

        VkFramebuffer fb;
        if (vkCreateFramebuffer(m_DeviceContext.getDevice(), &framebufferInfo, nullptr, &fb) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
        m_SwapChainFramebuffers[i] = VulkanHandle<VkFramebuffer, FramebufferDeleter>(fb, {m_DeviceContext.getDevice()});
    }
}

VkFormat SwapChainContext::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_DeviceContext.getPhysicalDevice(), format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

VkFormat SwapChainContext::findDepthFormat()
{
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
