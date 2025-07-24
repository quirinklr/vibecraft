#pragma once

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"
#include "../swapchain/SwapChainContext.h"
#include "DescriptorLayout.h"

class PipelineCache {
public:
    PipelineCache(const DeviceContext& deviceContext, const SwapChainContext& swapChainContext, const DescriptorLayout& descriptorLayout);
    ~PipelineCache();

    VkPipeline getGraphicsPipeline() const { return m_GraphicsPipeline.get(); }
    VkPipelineLayout getGraphicsPipelineLayout() const { return m_PipelineLayout.get(); }
    VkPipeline getCrosshairPipeline() const { return m_CrosshairPipeline.get(); }
    VkPipelineLayout getCrosshairPipelineLayout() const { return m_CrosshairPipelineLayout.get(); }

private:
    void createGraphicsPipeline();
    void createCrosshairPipeline();
    static std::vector<char> readFile(const std::string& filename);
    VulkanHandle<VkShaderModule, ShaderModuleDeleter> createShaderModule(const std::vector<char>& code);

    const DeviceContext& m_DeviceContext;
    const SwapChainContext& m_SwapChainContext;
    const DescriptorLayout& m_DescriptorLayout;

    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_PipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_GraphicsPipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_CrosshairPipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_CrosshairPipeline;
};
