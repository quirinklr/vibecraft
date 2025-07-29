#pragma once
#include "VulkanWrappers.h"
struct UploadJob
{
    VkBuffer stagingVB = VK_NULL_HANDLE;
    VkDeviceSize stagingVbOffset = 0;
    VkDeviceSize stagingVbSize = 0;

    VkBuffer stagingIB = VK_NULL_HANDLE;
    VkDeviceSize stagingIbOffset = 0;
    VkDeviceSize stagingIbSize = 0;

    VkFence fence = VK_NULL_HANDLE;
    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
};
