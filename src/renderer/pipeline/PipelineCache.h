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
    VkPipeline getOutlinePipeline() const { return m_OutlinePipeline.get(); }
    VkPipelineLayout getOutlinePipelineLayout() const { return m_OutlinePipelineLayout.get(); }
    VkPipeline getWireframePipeline() const { return m_WireframePipeline.get(); }
    VkPipeline getDebugPipeline() const { return m_DebugPipeline.get(); }
    VkPipelineLayout getDebugPipelineLayout() const { return m_DebugPipelineLayout.get(); }
    VkPipeline getTransparentPipeline() const { return m_TransparentPipeline.get(); }
    VkPipeline getWaterPipeline() const { return m_WaterPipeline.get(); }
    VkPipeline getSkyPipeline() const { return m_SkyPipeline.get(); }
    VkPipelineLayout getSkyPipelineLayout() const { return m_SkyPipelineLayout.get(); }
    VkPipeline getRayTracingPipeline() const { return m_RayTracingPipeline.get(); }
    VkPipelineLayout getRayTracingPipelineLayout() const { return m_RayTracingPipelineLayout.get(); }
    VkPipeline getItemPipeline() const { return m_ItemPipeline.get(); }
    VkPipelineLayout getItemPipelineLayout() const { return m_ItemPipelineLayout.get(); }
    VkPipeline getPlayerPipeline() const { return m_PlayerPipeline.get(); }
    VkPipelineLayout getPlayerPipelineLayout() const { return m_PlayerPipelineLayout.get(); }
    VkDescriptorSetLayout getPlayerDescriptorSetLayout() const { return m_PlayerDescriptorSetLayout.get(); }

    void createPipelines();

private:
    void createPlayerPipeline();
    void createRayTracingPipeline();
    void createSkyPipeline();
    void createCrosshairPipeline();
    void createOutlinePipeline();
    void createDebugPipeline();
    void createItemPipeline();

    const DeviceContext &m_DeviceContext;
    const SwapChainContext &m_SwapChainContext;
    const DescriptorLayout &m_DescriptorLayout;

    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_PipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_GraphicsPipeline;
    VulkanHandle<VkPipeline, PipelineDeleter> m_TransparentPipeline;
    VulkanHandle<VkPipeline, PipelineDeleter> m_WireframePipeline;
    VulkanHandle<VkPipeline, PipelineDeleter> m_DebugPipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_OutlinePipelineLayout;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_DebugPipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_OutlinePipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_CrosshairPipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_CrosshairPipeline;
    VulkanHandle<VkPipeline, PipelineDeleter> m_WaterPipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_SkyPipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_SkyPipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_RayTracingPipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_RayTracingPipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_ItemPipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_ItemPipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_PlayerPipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_PlayerPipeline;
    VulkanHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter> m_PlayerDescriptorSetLayout;
};