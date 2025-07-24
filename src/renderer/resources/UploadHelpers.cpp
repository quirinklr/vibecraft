#include "UploadHelpers.h"
#include <stdexcept>

void UploadHelpers::copyBuffer(const DeviceContext& deviceContext, VkCommandPool commandPool, VkBuffer src, VkBuffer dst, VkDeviceSize size, VkFence* outFence) {
    VkCommandBufferAllocateInfo a{};
    a.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    a.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    a.commandPool = commandPool;
    a.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(deviceContext.getDevice(), &a, &cmd);

    VkCommandBufferBeginInfo b{};
    b.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &b);

    VkBufferCopy r{0, 0, size};
    vkCmdCopyBuffer(cmd, src, dst, 1, &r);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo s{};
    s.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    s.commandBufferCount = 1;
    s.pCommandBuffers = &cmd;

    if (outFence) {
        if (*outFence == VK_NULL_HANDLE) {
            VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            vkCreateFence(deviceContext.getDevice(), &f, nullptr, outFence);
        }
        vkQueueSubmit(deviceContext.getGraphicsQueue(), 1, &s, *outFence);
    } else {
        vkQueueSubmit(deviceContext.getGraphicsQueue(), 1, &s, VK_NULL_HANDLE);
        vkQueueWaitIdle(deviceContext.getGraphicsQueue());
    }
}

void UploadHelpers::transitionImageLayout(const DeviceContext& deviceContext, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
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

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
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

void UploadHelpers::copyBufferToImage(const DeviceContext& deviceContext, VkCommandPool commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
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

void UploadHelpers::createChunkMeshBuffers(const DeviceContext& deviceContext, VkCommandPool commandPool, const std::vector<Vertex>& v, const std::vector<uint32_t>& i, UploadJob& up, VkBuffer& vb, VmaAllocation& va, VkBuffer& ib, VmaAllocation& ia) {
    if (v.empty() || i.empty()) {
        vb = ib = VK_NULL_HANDLE;
        va = ia = VK_NULL_HANDLE;
        up.fence = VK_NULL_HANDLE;
        return;
    }
    VkDeviceSize vs = sizeof(Vertex) * v.size();
    VkDeviceSize is = sizeof(uint32_t) * i.size();

    {
        VkBufferCreateInfo bInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, vs, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo aInfo{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
        vmaCreateBuffer(deviceContext.getAllocator(), &bInfo, &aInfo, &up.stagingVB, &up.stagingVbAlloc, nullptr);
        void* data;
        vmaMapMemory(deviceContext.getAllocator(), up.stagingVbAlloc, &data);
        memcpy(data, v.data(), vs);
        vmaUnmapMemory(deviceContext.getAllocator(), up.stagingVbAlloc);
    }
    {
        VkBufferCreateInfo bInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, is, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo aInfo{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
        vmaCreateBuffer(deviceContext.getAllocator(), &bInfo, &aInfo, &up.stagingIB, &up.stagingIbAlloc, nullptr);
        void* data;
        vmaMapMemory(deviceContext.getAllocator(), up.stagingIbAlloc, &data);
        memcpy(data, i.data(), is);
        vmaUnmapMemory(deviceContext.getAllocator(), up.stagingIbAlloc);
    }

    {
        VkBufferCreateInfo bInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, vs, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
        VmaAllocationCreateInfo aInfo{0, VMA_MEMORY_USAGE_GPU_ONLY};
        vmaCreateBuffer(deviceContext.getAllocator(), &bInfo, &aInfo, &vb, &va, nullptr);
    }
    {
        VkBufferCreateInfo bInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, is, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
        VmaAllocationCreateInfo aInfo{0, VMA_MEMORY_USAGE_GPU_ONLY};
        vmaCreateBuffer(deviceContext.getAllocator(), &bInfo, &aInfo, &ib, &ia, nullptr);
    }

    VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    a.commandPool = commandPool;
    a.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    a.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(deviceContext.getDevice(), &a, &cmd);

    VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(cmd, &b);

    VkBufferCopy cv{0, 0, vs};
    vkCmdCopyBuffer(cmd, up.stagingVB, vb, 1, &cv);
    VkBufferCopy ci{0, 0, is};
    vkCmdCopyBuffer(cmd, up.stagingIB, ib, 1, &ci);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd};

    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(deviceContext.getDevice(), &fi, nullptr, &up.fence);
    vkQueueSubmit(deviceContext.getGraphicsQueue(), 1, &s, up.fence);
}
