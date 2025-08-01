#pragma once

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"
#include "../Vertex.h"
#include "../../UploadJob.h"
#include "RingStagingArena.h"

class UploadHelpers
{
public:
    static void copyBuffer(const DeviceContext &deviceContext, VkCommandPool commandPool, VkBuffer src, VkBuffer dst, VkDeviceSize size, VkFence *outFence = nullptr);
    static void transitionImageLayout(
        VkCommandBuffer cmdbuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageSubresourceRange subresourceRange,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask);
    static void copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    static void stageChunkMesh(RingStagingArena &arena,
                               const std::vector<Vertex> &v,
                               const std::vector<uint32_t> &i,
                               UploadJob &up);
    static void submitChunkMeshUpload(const DeviceContext &dc, VkCommandPool pool,
                                      UploadJob &up,
                                      VmaBuffer &vb,
                                      VmaBuffer &ib);
    static VmaBuffer createDeviceLocalBufferFromData(
        const DeviceContext &dc, VkCommandPool pool,
        const void *data, VkDeviceSize size, VkBufferUsageFlags usage);
    static VmaBuffer createDeviceLocalBufferFromDataWithStaging(
        const DeviceContext &dc,
        VkCommandBuffer cmd,
        const void *data,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        std::vector<VmaBuffer> &frameStagingBuffers);

    static VkCommandBuffer beginSingleTimeCommands(const DeviceContext &dc, VkCommandPool pool);
    static void endSingleTimeCommands(const DeviceContext &dc, VkCommandPool pool, VkCommandBuffer cmd);
};
