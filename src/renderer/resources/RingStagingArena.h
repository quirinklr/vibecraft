#pragma once
#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"
#include <mutex>

class RingStagingArena
{
public:
    RingStagingArena(const DeviceContext &dc, VkDeviceSize size);
    ~RingStagingArena();
    bool alloc(VkDeviceSize sz, VkDeviceSize &offset);
    VkBuffer getBuffer() const { return m_Buffer.get(); }
    void *getMapped() const { return m_Mapped; }

private:
    const DeviceContext &m_DC;
    VmaBuffer m_Buffer;
    void *m_Mapped = nullptr;
    VkDeviceSize m_Size;
    VkDeviceSize m_Head = 0;
    std::mutex m_Mtx;
};
