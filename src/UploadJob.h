#pragma once
#include <vulkan/vulkan.h>

struct UploadJob {
    VkBuffer       stagingVB     = VK_NULL_HANDLE;
    VkDeviceMemory stagingVBMem  = VK_NULL_HANDLE;
    VkBuffer       stagingIB     = VK_NULL_HANDLE;
    VkDeviceMemory stagingIBMem  = VK_NULL_HANDLE;
    VkFence        fence         = VK_NULL_HANDLE;
};
