#pragma once

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"

class DescriptorLayout {
public:
    DescriptorLayout(const DeviceContext& deviceContext);
    ~DescriptorLayout();

    VkDescriptorSetLayout getDescriptorSetLayout() const { return m_DescriptorSetLayout.get(); }

private:
    void createDescriptorSetLayout();

    const DeviceContext& m_DeviceContext;
    VulkanHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter> m_DescriptorSetLayout;
};
