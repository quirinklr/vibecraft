#pragma once

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"

class TextureManager
{
public:
    TextureManager(const DeviceContext &deviceContext, VkCommandPool commandPool);
    ~TextureManager();

    void createTextureImage(const char *path);
    void createTextureImageView();
    void createTextureSampler();

    static VulkanHandle<VkImageView, ImageViewDeleter> createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    VkImageView getTextureImageView() const { return m_TextureImageView.get(); }
    VkSampler getTextureSampler() const { return m_TextureSampler.get(); }

private:
    const DeviceContext &m_DeviceContext;
    VkCommandPool m_CommandPool;

    VmaImage m_TextureImage;
    VulkanHandle<VkImageView, ImageViewDeleter> m_TextureImageView;
    VulkanHandle<VkSampler, SamplerDeleter> m_TextureSampler;
};
