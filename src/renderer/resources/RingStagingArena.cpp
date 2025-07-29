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
bool RingStagingArena::alloc(VkDeviceSize sz, VkDeviceSize &offset)
{
    std::scoped_lock l(m_Mtx);
    const VkDeviceSize align = 256;
    sz = (sz + align - 1) & ~(align - 1);
    if (sz > m_Size)
        return false;
    if (m_Head + sz > m_Size)
        m_Head = 0;
    offset = m_Head;
    m_Head += sz;
    return true;
}
