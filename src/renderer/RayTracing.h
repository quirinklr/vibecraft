

#pragma once
#include "../VulkanWrappers.h"
#include "core/DeviceContext.h"

struct AccelerationStructure
{
    VmaBuffer buffer;
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = 0;

    AccelerationStructure() = default;

    AccelerationStructure(AccelerationStructure &&other) noexcept
        : buffer(std::move(other.buffer)), handle(other.handle), deviceAddress(other.deviceAddress)
    {
        other.handle = VK_NULL_HANDLE;
        other.deviceAddress = 0;
    }

    AccelerationStructure &operator=(AccelerationStructure &&other) noexcept
    {
        if (this != &other)
        {

            buffer = std::move(other.buffer);
            handle = other.handle;
            deviceAddress = other.deviceAddress;

            other.handle = VK_NULL_HANDLE;
            other.deviceAddress = 0;
        }
        return *this;
    }

    AccelerationStructure(const AccelerationStructure &) = delete;
    AccelerationStructure &operator=(const AccelerationStructure &) = delete;

    void destroy(VkDevice device)
    {
        if (handle)
        {
            auto vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
            if (vkDestroyAccelerationStructureKHR)
            {
                vkDestroyAccelerationStructureKHR(device, handle, nullptr);
            }
        }
    }
};

inline void createAccelerationStructure(
    const DeviceContext &dc,
    VkAccelerationStructureTypeKHR type,
    VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo,
    AccelerationStructure &outAS)
{

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = buildSizeInfo.accelerationStructureSize;
    bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    outAS.buffer = VmaBuffer(dc.getAllocator(), bufferInfo, allocInfo);

    VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    createInfo.buffer = outAS.buffer.get();
    createInfo.size = buildSizeInfo.accelerationStructureSize;
    createInfo.type = type;

    auto vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(dc.getDevice(), "vkCreateAccelerationStructureKHR");
    if (vkCreateAccelerationStructureKHR(dc.getDevice(), &createInfo, nullptr, &outAS.handle) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create acceleration structure!");
    }

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.accelerationStructure = outAS.handle;
    auto vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(dc.getDevice(), "vkGetAccelerationStructureDeviceAddressKHR");
    outAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(dc.getDevice(), &addressInfo);
}