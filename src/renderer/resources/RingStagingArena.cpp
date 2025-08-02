#include "RingStagingArena.h"

RingStagingArena::RingStagingArena(const DeviceContext &dc, VkDeviceSize size)
    : m_DC(dc), m_Size(size)
{
    VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    b.size = size;
    b.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo a{};
    a.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
              VMA_ALLOCATION_CREATE_MAPPED_BIT;
    a.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    m_Buffer = VmaBuffer(dc.getAllocator(), b, a);
    VmaAllocationInfo info;
    vmaGetAllocationInfo(dc.getAllocator(), m_Buffer.getAllocation(), &info);
    m_Mapped = info.pMappedData;
}

RingStagingArena::~RingStagingArena() {}

void RingStagingArena::retireCompletedRegions()
{
    while (!m_InFlight.empty() &&
           vkGetFenceStatus(m_DC.getDevice(), m_InFlight.front().fence) == VK_SUCCESS)
    {
        m_InFlight.pop_front();
    }
}
bool RingStagingArena::regionBusy(VkDeviceSize begin, VkDeviceSize end) const
{
    for (auto &r : m_InFlight)
        if (!(end <= r.begin || begin >= r.end))
            return true;
    return false;
}
void RingStagingArena::onUploadSubmitted(VkDeviceSize begin,
                                         VkDeviceSize size,
                                         VkFence fence)
{
    std::scoped_lock l(m_Mtx);
    m_InFlight.push_back({begin, begin + size, fence});
}

bool RingStagingArena::alloc(VkDeviceSize sz, VkDeviceSize &offset)
{
    std::scoped_lock l(m_Mtx);

    const VkDeviceSize align = 256;
    sz = (sz + align - 1) & ~(align - 1);
    if (sz > m_Size)
        return false;

    while (true)
    {
        retireCompletedRegions();

        if (m_Head + sz > m_Size)
            m_Head = 0;

        if (!regionBusy(m_Head, m_Head + sz))
        {
            offset = m_Head;
            m_Head += sz;
            return true;
        }

        if (!m_InFlight.empty())
            vkWaitForFences(m_DC.getDevice(), 1, &m_InFlight.front().fence,
                            VK_TRUE, UINT64_MAX);
        else
            return false;
    }
}