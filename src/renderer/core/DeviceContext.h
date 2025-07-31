#pragma once
#include "../../VulkanWrappers.h"
#include "InstanceContext.h"
#include <optional>
#include <mutex>

class DeviceContext
{
public:
    DeviceContext(const InstanceContext &instanceContext);
    ~DeviceContext();

    VkPhysicalDevice getPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice getDevice() const { return m_Device.get(); }
    VmaAllocator getAllocator() const { return m_Allocator; }

    VkQueue getGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue getPresentQueue() const { return m_PresentQueue; }
    VkQueue getTransferQueue() const { return m_TransferQueue; }
    bool hasTransferQueue() const { return m_TransferQueue != VK_NULL_HANDLE; }

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;
        bool isComplete() { return graphicsFamily && presentFamily; }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

private:
    void pickPhysicalDevice();
    void createLogicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    const InstanceContext &m_InstanceContext;
    VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};
    VulkanHandle<VkDevice, DeviceDeleter> m_Device;
    VmaAllocator m_Allocator{VK_NULL_HANDLE};

    VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
    VkQueue m_PresentQueue{VK_NULL_HANDLE};
    VkQueue m_TransferQueue{VK_NULL_HANDLE};

    mutable std::mutex m_CbFreeMtx;
    mutable std::vector<std::tuple<VkCommandPool, VkCommandBuffer, VkFence>> m_CbDeferred;

    const std::vector<const char *> m_DeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const std::vector<const char *> m_ValidationLayers{"VK_LAYER_KHRONOS_validation"};
    const bool m_EnableValidationLayers = true;
};