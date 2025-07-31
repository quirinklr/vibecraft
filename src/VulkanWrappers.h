#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <utility>
#include <vector>
#include <mutex>
#include <tuple>

struct DeviceDeleter
{
    void operator()(VkDevice d) const
    {
        if (d)
            vkDestroyDevice(d, nullptr);
    }
};
struct InstanceDeleter
{
    void operator()(VkInstance i) const
    {
        if (i)
            vkDestroyInstance(i, nullptr);
    }
};

struct SurfaceDeleter
{
    VkInstance instance;
    void operator()(VkSurfaceKHR s) const
    {
        if (s)
            vkDestroySurfaceKHR(instance, s, nullptr);
    }
};
struct SwapchainDeleter
{
    VkDevice device;
    void operator()(VkSwapchainKHR s) const
    {
        if (s)
            vkDestroySwapchainKHR(device, s, nullptr);
    }
};
struct ImageViewDeleter
{
    VkDevice device;
    void operator()(VkImageView v) const
    {
        if (v)
            vkDestroyImageView(device, v, nullptr);
    }
};
struct RenderPassDeleter
{
    VkDevice device;
    void operator()(VkRenderPass p) const
    {
        if (p)
            vkDestroyRenderPass(device, p, nullptr);
    }
};
struct PipelineLayoutDeleter
{
    VkDevice device;
    void operator()(VkPipelineLayout l) const
    {
        if (l)
            vkDestroyPipelineLayout(device, l, nullptr);
    }
};
struct PipelineDeleter
{
    VkDevice device;
    void operator()(VkPipeline p) const
    {
        if (p)
            vkDestroyPipeline(device, p, nullptr);
    }
};
struct FramebufferDeleter
{
    VkDevice device;
    void operator()(VkFramebuffer f) const
    {
        if (f)
            vkDestroyFramebuffer(device, f, nullptr);
    }
};
struct CommandPoolDeleter
{
    VkDevice device;
    void operator()(VkCommandPool p) const
    {
        if (p)
            vkDestroyCommandPool(device, p, nullptr);
    }
};
struct SemaphoreDeleter
{
    VkDevice device;
    void operator()(VkSemaphore s) const
    {
        if (s)
            vkDestroySemaphore(device, s, nullptr);
    }
};
struct FenceDeleter
{
    VkDevice device;
    void operator()(VkFence f) const
    {
        if (f)
            vkDestroyFence(device, f, nullptr);
    }
};
struct DescriptorSetLayoutDeleter
{
    VkDevice device;
    void operator()(VkDescriptorSetLayout layout) const
    {
        if (layout)
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }
};
struct DescriptorPoolDeleter
{
    VkDevice device;
    void operator()(VkDescriptorPool p) const
    {
        if (p)
            vkDestroyDescriptorPool(device, p, nullptr);
    }
};
struct SamplerDeleter
{
    VkDevice device;
    void operator()(VkSampler s) const
    {
        if (s)
            vkDestroySampler(device, s, nullptr);
    }
};
struct ShaderModuleDeleter
{
    VkDevice device;
    void operator()(VkShaderModule m) const
    {
        if (m)
            vkDestroyShaderModule(device, m, nullptr);
    }
};

struct DeferredFreeQueue
{

    static void push(VkDevice device,
                     VkCommandPool pool,
                     VkCommandBuffer cmd,
                     VkFence fence);

    static void poll();

private:
    static std::mutex mtx;
    static std::vector<std::tuple<VkDevice, VkCommandPool, VkCommandBuffer, VkFence>> items;
};

template <typename T, typename Deleter>
class VulkanHandle
{
public:
    VulkanHandle() : m_Handle(VK_NULL_HANDLE), m_Deleter{} {}

    VulkanHandle(T handle, Deleter deleter) : m_Handle(handle), m_Deleter(deleter) {}

    ~VulkanHandle()
    {
        if (m_Handle != VK_NULL_HANDLE)
        {
            m_Deleter(m_Handle);
        }
    }

    VulkanHandle(const VulkanHandle &) = delete;
    VulkanHandle &operator=(const VulkanHandle &) = delete;

    VulkanHandle(VulkanHandle &&other) noexcept : m_Handle(other.m_Handle), m_Deleter(other.m_Deleter)
    {
        other.m_Handle = VK_NULL_HANDLE;
    }

    VulkanHandle &operator=(VulkanHandle &&other) noexcept
    {
        if (this != &other)
        {
            if (m_Handle != VK_NULL_HANDLE)
            {
                m_Deleter(m_Handle);
            }
            m_Handle = other.m_Handle;
            m_Deleter = other.m_Deleter;
            other.m_Handle = VK_NULL_HANDLE;
        }
        return *this;
    }

    T get() const { return m_Handle; }
    operator T() const { return m_Handle; }

    T *p() { return &m_Handle; }

private:
    T m_Handle;
    Deleter m_Deleter;
};

class VmaBuffer
{
public:
    VmaBuffer() = default;
    VmaBuffer(VmaAllocator allocator, const VkBufferCreateInfo &bufferInfo, const VmaAllocationCreateInfo &allocInfo)
        : m_Allocator(allocator)
    {
        vmaCreateBuffer(m_Allocator, &bufferInfo, &allocInfo, &m_Buffer, &m_Allocation, nullptr);
    }
    VmaBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation)
        : m_Allocator(allocator), m_Buffer(buffer), m_Allocation(allocation) {}

    ~VmaBuffer()
    {
        if (m_Allocator && m_Buffer)
        {
            vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
        }
    }

    VmaBuffer(const VmaBuffer &) = delete;
    VmaBuffer &operator=(const VmaBuffer &) = delete;

    VmaBuffer(VmaBuffer &&other) noexcept
    {
        m_Allocator = other.m_Allocator;
        m_Buffer = other.m_Buffer;
        m_Allocation = other.m_Allocation;
        other.m_Allocator = nullptr;
        other.m_Buffer = nullptr;
        other.m_Allocation = nullptr;
    }
    VmaBuffer &operator=(VmaBuffer &&other) noexcept
    {
        if (this != &other)
        {
            if (m_Allocator && m_Buffer)
            {
                vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
            }
            m_Allocator = other.m_Allocator;
            m_Buffer = other.m_Buffer;
            m_Allocation = other.m_Allocation;
            other.m_Allocator = nullptr;
            other.m_Buffer = nullptr;
            other.m_Allocation = nullptr;
        }
        return *this;
    }

    VkBuffer get() const { return m_Buffer; }
    operator VkBuffer() const { return m_Buffer; }
    VmaAllocation getAllocation() const { return m_Allocation; }

private:
    VmaAllocator m_Allocator = nullptr;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
};

class VmaImage
{
public:
    VmaImage() = default;
    VmaImage(VmaAllocator allocator, const VkImageCreateInfo &imageInfo, const VmaAllocationCreateInfo &allocInfo)
        : m_Allocator(allocator)
    {
        vmaCreateImage(m_Allocator, &imageInfo, &allocInfo, &m_Image, &m_Allocation, nullptr);
    }
    ~VmaImage()
    {
        if (m_Allocator && m_Image)
        {
            vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
        }
    }

    VmaImage(const VmaImage &) = delete;
    VmaImage &operator=(const VmaImage &) = delete;

    VmaImage(VmaImage &&other) noexcept
    {
        m_Allocator = other.m_Allocator;
        m_Image = other.m_Image;
        m_Allocation = other.m_Allocation;
        other.m_Allocator = nullptr;
        other.m_Image = nullptr;
        other.m_Allocation = nullptr;
    }
    VmaImage &operator=(VmaImage &&other) noexcept
    {
        if (this != &other)
        {
            if (m_Allocator && m_Image)
            {
                vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
            }
            m_Allocator = other.m_Allocator;
            m_Image = other.m_Image;
            m_Allocation = other.m_Allocation;
            other.m_Allocator = nullptr;
            other.m_Image = nullptr;
            other.m_Allocation = nullptr;
        }
        return *this;
    }

    VkImage get() const { return m_Image; }
    operator VkImage() const { return m_Image; }
    VmaAllocation getAllocation() const { return m_Allocation; }

private:
    VmaAllocator m_Allocator = nullptr;
    VkImage m_Image = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
};