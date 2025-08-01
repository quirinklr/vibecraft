#include "DeviceContext.h"
#include <stdexcept>
#include <set>
#include <string>

DeviceContext::DeviceContext(const InstanceContext &instanceContext)
    : m_InstanceContext(instanceContext)
{
    pickPhysicalDevice();
    checkRayTracingSupport();
    createLogicalDevice();

    VmaAllocatorCreateInfo allocatorInfo = {};

    if (m_rayTracingSupported)
    {
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = m_PhysicalDevice;
    allocatorInfo.device = m_Device.get();
    allocatorInfo.instance = m_InstanceContext.getInstance();
    vmaCreateAllocator(&allocatorInfo, &m_Allocator);
}

DeviceContext::~DeviceContext()
{
    if (m_Allocator)
    {
        vmaDestroyAllocator(m_Allocator);
    }
}

void DeviceContext::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_InstanceContext.getInstance(), &deviceCount, nullptr);
    if (deviceCount == 0)
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_InstanceContext.getInstance(), &deviceCount, devices.data());
    for (const auto &device : devices)
    {
        if (isDeviceSuitable(device))
        {
            m_PhysicalDevice = device;
            break;
        }
    }
    if (m_PhysicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("failed to find a suitable GPU!");
}

void DeviceContext::checkRayTracingSupport()
{

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(m_rayTracingExtensions.begin(), m_rayTracingExtensions.end());
    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    if (requiredExtensions.empty())
    {

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
        accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipelineFeatures{};
        pipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &accelFeatures;
        accelFeatures.pNext = &pipelineFeatures;
        vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &deviceFeatures2);

        if (accelFeatures.accelerationStructure && pipelineFeatures.rayTracingPipeline)
        {
            m_rayTracingSupported = true;

            m_deviceExtensions.insert(m_deviceExtensions.end(), m_rayTracingExtensions.begin(), m_rayTracingExtensions.end());
        }
    }
}

void DeviceContext::createLogicalDevice()
{
    QueueFamilyIndices idx = findQueueFamilies(m_PhysicalDevice);

    std::set<uint32_t> families = {idx.graphicsFamily.value(), idx.presentFamily.value()};
    if (idx.transferFamily)
        families.insert(idx.transferFamily.value());

    float prio = 1.f;
    std::vector<VkDeviceQueueCreateInfo> qInfos;
    for (auto f : families)
    {
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qi.queueFamilyIndex = f;
        qi.queueCount = 1;
        qi.pQueuePriorities = &prio;
        qInfos.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};
    features.wideLines = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;
    features.logicOp = VK_TRUE;
    features.shaderStorageImageWriteWithoutFormat = VK_TRUE;

    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.queueCreateInfoCount = static_cast<uint32_t>(qInfos.size());
    ci.pQueueCreateInfos = qInfos.data();
    ci.pEnabledFeatures = &features;
    ci.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    ci.ppEnabledExtensionNames = m_deviceExtensions.data();

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipelineFeatures{};
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};

    if (m_rayTracingSupported)
    {
        accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelFeatures.accelerationStructure = VK_TRUE;

        pipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        pipelineFeatures.rayTracingPipeline = VK_TRUE;

        bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

        ci.pNext = &bufferDeviceAddressFeatures;
        bufferDeviceAddressFeatures.pNext = &accelFeatures;
        accelFeatures.pNext = &pipelineFeatures;
    }

#ifndef NDEBUG
    if (m_enableValidationLayers)
    {
        ci.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        ci.ppEnabledLayerNames = m_validationLayers.data();
    }
#endif

    VkDevice dev;
    if (vkCreateDevice(m_PhysicalDevice, &ci, nullptr, &dev) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device!");

    m_Device = {dev, {}};

    vkGetDeviceQueue(dev, idx.graphicsFamily.value(), 0, &m_GraphicsQueue);
    vkGetDeviceQueue(dev, idx.presentFamily.value(), 0, &m_PresentQueue);
    if (idx.transferFamily)
        vkGetDeviceQueue(dev, idx.transferFamily.value(), 0, &m_TransferQueue);
}

DeviceContext::QueueFamilyIndices DeviceContext::findQueueFamilies(const VkPhysicalDevice pdevice) const
{
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, props.data());

    int i = 0;
    for (auto &q : props)
    {
        if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(pdevice, i, m_InstanceContext.getSurface(), &present);
        if (present)
            indices.presentFamily = i;
        if ((q.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(q.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            indices.transferFamily = i;
        if (indices.isComplete())
            break;
        ++i;
    }
    return indices;
}

bool DeviceContext::checkDeviceExtensionSupport(const VkPhysicalDevice pdevice)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());
    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

bool DeviceContext::isDeviceSuitable(const VkPhysicalDevice pdevice)
{
    QueueFamilyIndices indices = findQueueFamilies(pdevice);

    std::vector<const char *> baseExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extensionCount, availableExtensions.data());
    std::set<std::string> required(baseExtensions.begin(), baseExtensions.end());
    for (const auto &ext : availableExtensions)
    {
        required.erase(ext.extensionName);
    }

    bool extensionsSupported = required.empty();
    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        swapChainAdequate = true;
    }
    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}