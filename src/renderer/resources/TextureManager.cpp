#include "TextureManager.h"
#include "UploadHelpers.h"
#include <stb_image.h>
#include <stdexcept>
#include <cstring>

TextureManager::TextureManager(const DeviceContext &deviceContext,
                               VkCommandPool commandPool)
    : m_DeviceContext(deviceContext),
      m_CommandPool(commandPool) {}

TextureManager::~TextureManager() {}

VulkanHandle<VkImageView, ImageViewDeleter> TextureManager::createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture image view!");
    }
    return VulkanHandle<VkImageView, ImageViewDeleter>(imageView, {device});
}

void TextureManager::createTextureImage(const char *path)
{
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("failed to load texture image!");
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VmaBuffer stagingBuffer;
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0,
                                      imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo allocInfo{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                          VMA_MEMORY_USAGE_CPU_ONLY};
        stagingBuffer = VmaBuffer(m_DeviceContext.getAllocator(), bufferInfo, allocInfo);
        void *data;
        vmaMapMemory(m_DeviceContext.getAllocator(), stagingBuffer.getAllocation(), &data);
        std::memcpy(data, pixels, static_cast<size_t>(imageSize));
        vmaUnmapMemory(m_DeviceContext.getAllocator(), stagingBuffer.getAllocation());
    }
    stbi_image_free(pixels);

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {(uint32_t)texWidth, (uint32_t)texHeight, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{0, VMA_MEMORY_USAGE_GPU_ONLY};
    m_TextureImage = VmaImage(m_DeviceContext.getAllocator(), imageInfo, allocInfo);

    UploadHelpers::transitionImageLayout(m_DeviceContext, m_CommandPool,
                                         m_TextureImage.get(), VK_FORMAT_R8G8B8A8_SRGB,
                                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    UploadHelpers::copyBufferToImage(m_DeviceContext, m_CommandPool,
                                     stagingBuffer.get(), m_TextureImage.get(),
                                     static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    UploadHelpers::transitionImageLayout(m_DeviceContext, m_CommandPool,
                                         m_TextureImage.get(), VK_FORMAT_R8G8B8A8_SRGB,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureManager::createTextureImageView()
{
    m_TextureImageView = createImageView(m_DeviceContext.getDevice(), m_TextureImage.get(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void TextureManager::createTextureSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VkSampler sampler;
    if (vkCreateSampler(m_DeviceContext.getDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture sampler!");
    }
    m_TextureSampler = VulkanHandle<VkSampler, SamplerDeleter>(sampler, {m_DeviceContext.getDevice()});
}
