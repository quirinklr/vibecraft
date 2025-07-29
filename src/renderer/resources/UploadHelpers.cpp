#include "UploadHelpers.h"
#include <stdexcept>
#include <Globals.h>

void UploadHelpers::copyBuffer(const DeviceContext &deviceContext,
                               VkCommandPool commandPool,
                               VkBuffer src, VkBuffer dst,
                               VkDeviceSize size, VkFence *outFence)
{
    std::scoped_lock lk(gGraphicsQueueMutex);

    VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    a.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    a.commandPool = commandPool;
    a.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(deviceContext.getDevice(), &a, &cmd);

    VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &b);

    VkBufferCopy r{0, 0, size};
    vkCmdCopyBuffer(cmd, src, dst, 1, &r);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    s.commandBufferCount = 1;
    s.pCommandBuffers = &cmd;

    if (outFence)
    {
        if (*outFence == VK_NULL_HANDLE)
        {
            VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            vkCreateFence(deviceContext.getDevice(), &f, nullptr, outFence);
        }
        vkQueueSubmit(deviceContext.getGraphicsQueue(), 1, &s, *outFence);
    }
    else
    {
        vkQueueSubmit(deviceContext.getGraphicsQueue(), 1, &s, VK_NULL_HANDLE);
        vkQueueWaitIdle(deviceContext.getGraphicsQueue());
    }
}

void UploadHelpers::transitionImageLayout(const DeviceContext &deviceContext, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(deviceContext.getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(deviceContext.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(deviceContext.getGraphicsQueue());

    vkFreeCommandBuffers(deviceContext.getDevice(), commandPool, 1, &commandBuffer);
}

void UploadHelpers::copyBufferToImage(const DeviceContext &deviceContext, VkCommandPool commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(deviceContext.getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

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

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(deviceContext.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(deviceContext.getGraphicsQueue());

    vkFreeCommandBuffers(deviceContext.getDevice(), commandPool, 1, &commandBuffer);
}

void UploadHelpers::stageChunkMesh(VmaAllocator allocator, const std::vector<Vertex> &v, const std::vector<uint32_t> &i, UploadJob &up)
{
    if (v.empty() || i.empty())
    {
        up.stagingVB = VK_NULL_HANDLE;
        up.stagingIB = VK_NULL_HANDLE;
        return;
    }

    VkDeviceSize vs = sizeof(Vertex) * v.size();
    VkDeviceSize is = sizeof(uint32_t) * i.size();

    {
        VkBufferCreateInfo bInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, vs, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo aInfo{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
        vmaCreateBuffer(allocator, &bInfo, &aInfo, &up.stagingVB, &up.stagingVbAlloc, nullptr);
        void *data;
        vmaMapMemory(allocator, up.stagingVbAlloc, &data);
        memcpy(data, v.data(), vs);
        vmaUnmapMemory(allocator, up.stagingVbAlloc);
    }
    {
        VkBufferCreateInfo bInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, is, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo aInfo{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
        vmaCreateBuffer(allocator, &bInfo, &aInfo, &up.stagingIB, &up.stagingIbAlloc, nullptr);
        void *data;
        vmaMapMemory(allocator, up.stagingIbAlloc, &data);
        memcpy(data, i.data(), is);
        vmaUnmapMemory(allocator, up.stagingIbAlloc);
    }
}

void UploadHelpers::submitChunkMeshUpload(const DeviceContext &dc,
                                          VkCommandPool cmdPool,
                                          UploadJob &up,
                                          VkBuffer &vb, VmaAllocation &va,
                                          VkBuffer &ib, VmaAllocation &ia)
{
    std::scoped_lock lk(gGraphicsQueueMutex);

    VmaAllocationInfo info;
    vmaGetAllocationInfo(dc.getAllocator(), up.stagingVbAlloc, &info);
    VkDeviceSize vs = info.size;
    vmaGetAllocationInfo(dc.getAllocator(), up.stagingIbAlloc, &info);
    VkDeviceSize is = info.size;

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    bci.size = vs;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vmaCreateBuffer(dc.getAllocator(), &bci, &aci, &vb, &va, nullptr);

    bci.size = is;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    vmaCreateBuffer(dc.getAllocator(), &bci, &aci, &ib, &ia, nullptr);

    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    vkAllocateCommandBuffers(dc.getDevice(), &cbai, &up.cmdBuffer);

    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(up.cmdBuffer, &cbbi);

    VkBufferCopy copy{0, 0, vs};
    vkCmdCopyBuffer(up.cmdBuffer, up.stagingVB, vb, 1, &copy);
    copy.size = is;
    vkCmdCopyBuffer(up.cmdBuffer, up.stagingIB, ib, 1, &copy);
    vkEndCommandBuffer(up.cmdBuffer);

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(dc.getDevice(), &fci, nullptr, &up.fence);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &up.cmdBuffer;
    vkQueueSubmit(dc.getGraphicsQueue(), 1, &si, up.fence);
}
