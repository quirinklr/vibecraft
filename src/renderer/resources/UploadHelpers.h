#pragma once

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"
#include "../Vertex.h"
#include "../../UploadJob.h"

class UploadHelpers {
public:
    static void copyBuffer(const DeviceContext& deviceContext, VkCommandPool commandPool, VkBuffer src, VkBuffer dst, VkDeviceSize size, VkFence* outFence = nullptr);
    static void transitionImageLayout(const DeviceContext& deviceContext, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    static void copyBufferToImage(const DeviceContext& deviceContext, VkCommandPool commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    static void createChunkMeshBuffers(const DeviceContext& deviceContext, VkCommandPool commandPool, const std::vector<Vertex>& v, const std::vector<uint32_t>& i, UploadJob& up, VkBuffer& vb, VmaAllocation& va, VkBuffer& ib, VmaAllocation& ia);
};
