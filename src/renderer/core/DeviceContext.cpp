#include "DeviceContext.h"
#include <stdexcept>
#include <set>

DeviceContext::DeviceContext(const InstanceContext &instanceContext) : m_InstanceContext(instanceContext)
{
    pickPhysicalDevice();
    createLogicalDevice();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
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

    VkPhysicalDeviceFeatures feat{};
    feat.wideLines = VK_TRUE;
    feat.fillModeNonSolid = VK_TRUE;
    feat.logicOp = VK_TRUE;

    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.queueCreateInfoCount = static_cast<uint32_t>(qInfos.size());
    ci.pQueueCreateInfos = qInfos.data();
    ci.pEnabledFeatures = &feat;
    ci.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());

    ci.ppEnabledExtensionNames = m_DeviceExtensions.data();

    #ifndef NDEBUG
        if (m_EnableValidationLayers)
        {
            ci.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            ci.ppEnabledLayerNames = m_ValidationLayers.data();
        }
    #endif
    else
    {
        ci.enabledLayerCount = 0;
    }

    VkDevice dev;
    vkCreateDevice(m_PhysicalDevice, &ci, nullptr, &dev);
    m_Device = {dev, {}};

    vkGetDeviceQueue(dev, idx.graphicsFamily.value(), 0, &m_GraphicsQueue);
    vkGetDeviceQueue(dev, idx.presentFamily.value(), 0, &m_PresentQueue);
    if (idx.transferFamily)
        vkGetDeviceQueue(dev, idx.transferFamily.value(), 0, &m_TransferQueue);

    QueueFamilyIndices tmp = idx;
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
    std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());
    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

bool DeviceContext::isDeviceSuitable(const VkPhysicalDevice pdevice)
{
    QueueFamilyIndices indices = findQueueFamilies(pdevice);
    bool extensionsSupported = checkDeviceExtensionSupport(pdevice);
    bool swapChainAdequate = false;
    if (extensionsSupported)
    {

        swapChainAdequate = true;
    }
    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}
