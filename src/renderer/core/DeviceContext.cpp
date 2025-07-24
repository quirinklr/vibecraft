#include "DeviceContext.h"
#include <stdexcept>
#include <set>

DeviceContext::DeviceContext(const InstanceContext& instanceContext) : m_InstanceContext(instanceContext) {
    pickPhysicalDevice();
    createLogicalDevice();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
    allocatorInfo.physicalDevice = m_PhysicalDevice;
    allocatorInfo.device = m_Device.get();
    allocatorInfo.instance = m_InstanceContext.getInstance();
    vmaCreateAllocator(&allocatorInfo, &m_Allocator);
}

DeviceContext::~DeviceContext() {
    if (m_Allocator) {
        vmaDestroyAllocator(m_Allocator);
    }
}

void DeviceContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_InstanceContext.getInstance(), &deviceCount, nullptr);
    if (deviceCount == 0)
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_InstanceContext.getInstance(), &deviceCount, devices.data());
    for (const auto &device : devices) {
        if (isDeviceSuitable(device)) {
            m_PhysicalDevice = device;
            break;
        }
    }
    if (m_PhysicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("failed to find a suitable GPU!");
}

void DeviceContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_PhysicalDevice);
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.wideLines = VK_TRUE;
    deviceFeatures.logicOp = VK_TRUE;
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();
    if (m_EnableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkDevice device;
    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
    m_Device = VulkanHandle<VkDevice, DeviceDeleter>(device, {});

    vkGetDeviceQueue(m_Device.get(), indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device.get(), indices.presentFamily.value(), 0, &m_PresentQueue);
}

DeviceContext::QueueFamilyIndices DeviceContext::findQueueFamilies(const VkPhysicalDevice pdevice) const {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &queueFamilyCount, queueFamilies.data());
    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(pdevice, i, m_InstanceContext.getSurface(), &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }
        if (indices.isComplete()) {
            break;
        }
        i++;
    }
    return indices;
}


bool DeviceContext::checkDeviceExtensionSupport(const VkPhysicalDevice pdevice) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extensionCount, availableExtensions.data());
    std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());
    for (const auto &extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

bool DeviceContext::isDeviceSuitable(const VkPhysicalDevice pdevice) {
    QueueFamilyIndices indices = findQueueFamilies(pdevice);
    bool extensionsSupported = checkDeviceExtensionSupport(pdevice);
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        // SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pdevice);
        // swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        swapChainAdequate = true; // Simplified for now
    }
    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}
