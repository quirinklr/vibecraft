#include "UploadHelpers.h"
#include <stdexcept>
#include <Globals.h>
#include <iostream>

void UploadHelpers::copyBuffer(const DeviceContext &dc,
                               VkCommandPool pool,
                               VkBuffer src,
                               VkBuffer dst,
                               VkDeviceSize size,
                               VkFence *outFence)
{
    std::scoped_lock lock(gGraphicsQueueMutex);

    VkCommandBuffer cmd{};
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(dc.getDevice(), &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkQueue queue = dc.getGraphicsQueue();

    if (outFence)
    {

        if (*outFence == VK_NULL_HANDLE)
        {
            VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            vkCreateFence(dc.getDevice(), &fenceInfo, nullptr, outFence);
        }
        vkQueueSubmit(queue, 1, &submitInfo, *outFence);
        DeferredFreeQueue::push(dc.getDevice(), pool, cmd, *outFence);
    }
    else
    {

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkDeviceWaitIdle(dc.getDevice());
        vkFreeCommandBuffers(dc.getDevice(), pool, 1, &cmd);
    }
}

VmaBuffer UploadHelpers::createDeviceLocalBufferFromDataWithStaging(
    const DeviceContext &dc,
    VkCommandBuffer cmd,
    const void *data,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    std::vector<VmaBuffer> &frameStagingBuffers)
{

    VmaBuffer stagingBuffer;
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0,
                                      size,
                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                          usage |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        stagingBuffer = VmaBuffer(dc.getAllocator(), bufferInfo, allocInfo);
        void *mappedData;
        vmaMapMemory(dc.getAllocator(), stagingBuffer.getAllocation(), &mappedData);
        memcpy(mappedData, data, size);
        vmaUnmapMemory(dc.getAllocator(), stagingBuffer.getAllocation());
    }

    VmaBuffer deviceLocalBuffer;
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0,
                                      size,
                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                          usage |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        deviceLocalBuffer = VmaBuffer(dc.getAllocator(), bufferInfo, allocInfo);
    }

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, stagingBuffer.get(), deviceLocalBuffer.get(), 1, &copyRegion);

    frameStagingBuffers.push_back(std::move(stagingBuffer));

    return deviceLocalBuffer;
}

VkCommandBuffer UploadHelpers::beginSingleTimeCommands(const DeviceContext &dc, VkCommandPool pool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(dc.getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void UploadHelpers::endSingleTimeCommands(const DeviceContext &dc, VkCommandPool pool, VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(dc.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkDeviceWaitIdle(dc.getDevice());

    vkFreeCommandBuffers(dc.getDevice(), pool, 1, &cmd);
}

void UploadHelpers::transitionImageLayout(
    VkCommandBuffer cmdbuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageSubresourceRange subresourceRange,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = subresourceRange;

    switch (oldLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        barrier.srcAccessMask = 0;
        break;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_GENERAL:

        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    default:
        break;
    }

    switch (newLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_GENERAL:
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        break;
    }

    vkCmdPipelineBarrier(
        cmdbuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
}

void UploadHelpers::copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void UploadHelpers::stageChunkMesh(RingStagingArena &arena,
                                   const std::vector<Vertex> &v,
                                   const std::vector<uint32_t> &i,
                                   UploadJob &up)
{
    if (v.empty() || i.empty())
        return;

    VkDeviceSize vs = v.size() * sizeof(Vertex);
    VkDeviceSize is = i.size() * sizeof(uint32_t);

    if (!arena.alloc(vs, up.stagingVbOffset))
        return;
    if (!arena.alloc(is, up.stagingIbOffset))
        return;

    up.stagingVB = arena.getBuffer();
    up.stagingVbSize = vs;
    up.stagingIB = arena.getBuffer();
    up.stagingIbSize = is;

    uint8_t *base = static_cast<uint8_t *>(arena.getMapped());
    memcpy(base + up.stagingVbOffset, v.data(), vs);
    memcpy(base + up.stagingIbOffset, i.data(), is);
}

void UploadHelpers::submitChunkMeshUpload(const DeviceContext &dc,
                                          VkCommandPool pool,
                                          UploadJob &up,
                                          VmaBuffer &vb,
                                          VmaBuffer &ib)
{
    up.cmdBuffer = VK_NULL_HANDLE;
    up.fence = VK_NULL_HANDLE;

    if (up.stagingVbSize == 0 || up.stagingIbSize == 0)
    {

        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vkCreateFence(dc.getDevice(), &fi, nullptr, &up.fence);
        return;
    }

    bool useRtFlags = dc.isRayTracingSupported();
    VkBufferUsageFlags vbUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkBufferUsageFlags ibUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (useRtFlags)
    {
        vbUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        ibUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }

    VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    VmaAllocationCreateInfo a{};
    a.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    b.size = up.stagingVbSize;
    b.usage = vbUsage;
    vb = VmaBuffer(dc.getAllocator(), b, a);

    b.size = up.stagingIbSize;
    b.usage = ibUsage;
    ib = VmaBuffer(dc.getAllocator(), b, a);

    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(dc.getDevice(), &ai, &up.cmdBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffer for chunk mesh upload");
    }

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(up.cmdBuffer, &bi);

    VkBufferCopy c1{up.stagingVbOffset, 0, up.stagingVbSize};
    vkCmdCopyBuffer(up.cmdBuffer, up.stagingVB, vb.get(), 1, &c1);

    VkBufferCopy c2{up.stagingIbOffset, 0, up.stagingIbSize};
    vkCmdCopyBuffer(up.cmdBuffer, up.stagingIB, ib.get(), 1, &c2);

    vkEndCommandBuffer(up.cmdBuffer);

    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(dc.getDevice(), &fi, nullptr, &up.fence) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create fence for chunk mesh upload");
    }
}

VmaBuffer UploadHelpers::createDeviceLocalBufferFromData(
    const DeviceContext &dc, VkCommandPool pool,
    const void *data, VkDeviceSize size, VkBufferUsageFlags usage)
{

    VmaBuffer stagingBuffer;
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0,
                                      size,
                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                          usage |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
        VmaAllocationCreateInfo allocInfo{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
        stagingBuffer = VmaBuffer(dc.getAllocator(), bufferInfo, allocInfo);
        void *mappedData;
        vmaMapMemory(dc.getAllocator(), stagingBuffer.getAllocation(), &mappedData);
        memcpy(mappedData, data, size);
        vmaUnmapMemory(dc.getAllocator(), stagingBuffer.getAllocation());
    }

    VmaBuffer deviceLocalBuffer;
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0,
                                      size,
                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                          usage |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
        VmaAllocationCreateInfo allocInfo{0, VMA_MEMORY_USAGE_GPU_ONLY};
        deviceLocalBuffer = VmaBuffer(dc.getAllocator(), bufferInfo, allocInfo);
    }

    copyBuffer(dc, pool, stagingBuffer.get(), deviceLocalBuffer.get(), size);

    return deviceLocalBuffer;
}