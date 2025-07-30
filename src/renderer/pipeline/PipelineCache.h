#pragma once

#include "../../VulkanWrappers.h"
#include "../core/DeviceContext.h"
#include "../swapchain/SwapChainContext.h"
#include "DescriptorLayout.h"

class PipelineCache
{
public:
    PipelineCache(const DeviceContext &deviceContext, const SwapChainContext &swapChainContext, const DescriptorLayout &descriptorLayout);
    ~PipelineCache();

    VkPipeline getGraphicsPipeline() const { return m_GraphicsPipeline.get(); }
    VkPipelineLayout getGraphicsPipelineLayout() const { return m_PipelineLayout.get(); }
    VkPipeline getCrosshairPipeline() const { return m_CrosshairPipeline.get(); }
    VkPipelineLayout getCrosshairPipelineLayout() const { return m_CrosshairPipelineLayout.get(); }
    VkPipeline getWireframePipeline() const { return m_WireframePipeline.get(); }
    VkPipeline getDebugPipeline() const { return m_DebugPipeline.get(); }
    VkPipelineLayout getDebugPipelineLayout() const { return m_DebugPipelineLayout.get(); }

    void createPipelines();

private:
    void createCrosshairPipeline();
    void createDebugPipeline();

    static std::vector<char> readFile(const std::string &filename);
    static VulkanHandle<VkShaderModule, ShaderModuleDeleter>
    createShaderModule(const std::vector<char> &, VkDevice);

    const DeviceContext &m_DeviceContext;
    const SwapChainContext &m_SwapChainContext;
    const DescriptorLayout &m_DescriptorLayout;

    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_PipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_GraphicsPipeline;
    VulkanHandle<VkPipeline, PipelineDeleter> m_WireframePipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_DebugPipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_DebugPipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_CrosshairPipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_CrosshairPipeline;
};
