#pragma once

#include "VulkanWrappers.h"

struct UploadJob
{
    VkBuffer stagingVB = VK_NULL_HANDLE;
    VmaAllocation stagingVbAlloc = VK_NULL_HANDLE;

    VkBuffer stagingIB = VK_NULL_HANDLE;
    VmaAllocation stagingIbAlloc = VK_NULL_HANDLE;

    VkFence fence = VK_NULL_HANDLE;
};