#include "VulkanWrappers.h"

DestructionQueue g_DestructionQueue;

void DestructionQueue::enqueue(std::function<void()>&& function)
{
    m_deletors.push_back(std::move(function));
}

void DestructionQueue::flush()
{
    for (auto& func : m_deletors)
    {
        func();
    }
    m_deletors.clear();
}

std::mutex DeferredFreeQueue::mtx;
std::vector<std::tuple<VkDevice, VkCommandPool, VkCommandBuffer, VkFence>>
    DeferredFreeQueue::items;

void DeferredFreeQueue::push(VkDevice device,
                             VkCommandPool pool,
                             VkCommandBuffer cmd,
                             VkFence fence)
{
    std::lock_guard<std::mutex> lock(mtx);
    items.emplace_back(device, pool, cmd, fence);
}

void DeferredFreeQueue::poll()
{
    std::lock_guard<std::mutex> lock(mtx);
    for (auto it = items.begin(); it != items.end();)
    {
        VkDevice device = std::get<0>(*it);
        VkCommandPool pool = std::get<1>(*it);
        VkCommandBuffer cmd = std::get<2>(*it);
        VkFence fence = std::get<3>(*it);

        if (vkGetFenceStatus(device, fence) == VK_SUCCESS)
        {
            vkFreeCommandBuffers(device, pool, 1, &cmd);
            it = items.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
