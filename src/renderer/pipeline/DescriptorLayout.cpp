#include "DescriptorLayout.h"
#include <stdexcept>
#include <array>

DescriptorLayout::DescriptorLayout(const DeviceContext &deviceContext) : m_DeviceContext(deviceContext)
{
    createDescriptorSetLayout();
}

DescriptorLayout::~DescriptorLayout() {}

void DescriptorLayout::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding ubo{};
    ubo.binding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;

    ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding sampler{};
    sampler.binding = 1;
    sampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler.descriptorCount = 1;
    sampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{ubo, sampler};
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(m_DeviceContext.getDevice(), &info, nullptr, &layout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout");
    }
    m_DescriptorSetLayout = VulkanHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter>(layout, {m_DeviceContext.getDevice()});
}