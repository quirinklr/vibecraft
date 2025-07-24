#pragma once

#include "Window.h"
#include "Settings.h"
#include "Camera.h"
#include "UploadJob.h"
#include "Chunk.h"
#include "renderer/Vertex.h"
#include "renderer/core/InstanceContext.h"
#include "renderer/core/DeviceContext.h"
#include "renderer/swapchain/SwapChainContext.h"
#include "renderer/pipeline/DescriptorLayout.h"
#include "renderer/pipeline/PipelineCache.h"
#include "renderer/command/CommandManager.h"
#include "renderer/command/SyncPrimitives.h"
#include "renderer/resources/TextureManager.h"
#include "renderer/resources/UploadHelpers.h"
#include "renderer/RendererConfig.h"
#include "math/Ivec3Less.h"

#include <map>
#include <memory>

class VulkanRenderer {
    friend class Chunk;

public:
    VulkanRenderer(Window& window, const Settings& settings);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    void drawFrame(Camera& camera, const std::map<glm::ivec3, std::unique_ptr<Chunk>, ivec3_less>& chunks);
    void createChunkMeshBuffers(const std::vector<Vertex>& v, const std::vector<uint32_t>& i, UploadJob& up, VkBuffer& vb, VmaAllocation& va, VkBuffer& ib, VmaAllocation& ia);
    void enqueueDestroy(VmaBuffer&& buffer);
    void enqueueDestroy(VmaImage&& image);
    void enqueueDestroy(VkBuffer buffer, VmaAllocation allocation);

    VkDevice getDevice() const { return m_DeviceContext->getDevice(); }
    VmaAllocator getAllocator() const { return m_DeviceContext->getAllocator(); }

private:
    void updateUniformBuffer(uint32_t currentImage, Camera& camera);
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCrosshairVertexBuffer();

    Window& m_Window;
    const Settings& m_Settings;

    std::unique_ptr<InstanceContext> m_InstanceContext;
    std::unique_ptr<DeviceContext> m_DeviceContext;
    std::unique_ptr<SwapChainContext> m_SwapChainContext;
    std::unique_ptr<DescriptorLayout> m_DescriptorLayout;
    std::unique_ptr<PipelineCache> m_PipelineCache;
    std::unique_ptr<CommandManager> m_CommandManager;
    std::unique_ptr<SyncPrimitives> m_SyncPrimitives;
    std::unique_ptr<TextureManager> m_TextureManager;

    std::vector<VmaBuffer> m_UniformBuffers;
    std::vector<void*> m_UniformBuffersMapped;
    VulkanHandle<VkDescriptorPool, DescriptorPoolDeleter> m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VmaBuffer m_CrosshairVertexBuffer;

    uint32_t m_CurrentFrame{0};
    std::vector<VmaBuffer> m_BufferDestroyQueue[MAX_FRAMES_IN_FLIGHT];
    std::vector<VmaImage> m_ImageDestroyQueue[MAX_FRAMES_IN_FLIGHT];
};
