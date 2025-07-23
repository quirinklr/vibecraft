#pragma once

#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

struct UploadJob
{
    VkBuffer stagingVB;

    VmaAllocation stagingVbAlloc;

    VkBuffer stagingIB;

    VmaAllocation stagingIbAlloc;

    VkFence fence;
};