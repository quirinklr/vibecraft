#pragma once

#include "../../VulkanWrappers.h"
#include "InstanceContext.h"
#include <optional>

class DeviceContext {
public:
    DeviceContext(const InstanceContext& instanceContext);
    ~DeviceContext();

    VkPhysicalDevice getPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice getDevice() const { return m_Device.get(); }
    VmaAllocator getAllocator() const { return m_Allocator; }
    VkQueue getGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue getPresentQueue() const { return m_PresentQueue; }

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

private:
    void pickPhysicalDevice();
    void createLogicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    const InstanceContext& m_InstanceContext;
    VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};
    VulkanHandle<VkDevice, DeviceDeleter> m_Device;
    VmaAllocator m_Allocator{VK_NULL_HANDLE};
    VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
    VkQueue m_PresentQueue{VK_NULL_HANDLE};

    const std::vector<const char *> m_DeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const std::vector<const char *> m_ValidationLayers{"VK_LAYER_KHRONOS_validation"};
    const bool m_EnableValidationLayers = true;
};
