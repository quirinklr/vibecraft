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
    bool isRayTracingSupported() const { return m_rayTracingSupported; }

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
    void checkRayTracingSupport();

    const InstanceContext &m_InstanceContext;
    VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};
    VulkanHandle<VkDevice, DeviceDeleter> m_Device;
    VmaAllocator m_Allocator{VK_NULL_HANDLE};

    VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
    VkQueue m_PresentQueue{VK_NULL_HANDLE};
    VkQueue m_TransferQueue{VK_NULL_HANDLE};

    bool m_rayTracingSupported = false;
    std::vector<const char *> m_deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    const std::vector<const char *> m_rayTracingExtensions = {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};

    const std::vector<const char *> m_validationLayers{"VK_LAYER_KHRONOS_validation"};
    const bool m_enableValidationLayers = true;
};