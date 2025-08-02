#pragma once

#include "Window.h"
#include "Settings.h"
#include "Camera.h"
#include "UploadJob.h"
#include "Chunk.h"
#include "Globals.h"
#include "renderer/Vertex.h"
#include "renderer/core/InstanceContext.h"
#include "renderer/core/DeviceContext.h"
#include "renderer/swapchain/SwapChainContext.h"
#include "renderer/pipeline/DescriptorLayout.h"
#include "renderer/pipeline/PipelineCache.h"
#include "renderer/command/CommandManager.h"
#include "renderer/command/SyncPrimitives.h"
#include "renderer/resources/TextureManager.h"
#include "renderer/resources/RingStagingArena.h"
#include "renderer/resources/UploadHelpers.h"
#include "renderer/RendererConfig.h"
#include "math/Ivec3Less.h"
#include "renderer/DebugOverlay.h"
#include "renderer/RayTracingPushConstants.h"

#include <map>
#include <memory>
#include <mutex>
#include <glm/glm.hpp>

class Player;
class TerrainGenerator;

class VulkanRenderer
{
public:
    VulkanRenderer(Window &window, Settings &settings, Player *player, TerrainGenerator *terrainGen);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer &) = delete;
    VulkanRenderer &operator=(const VulkanRenderer &) = delete;

    bool drawFrame(Camera &camera,
                   const glm::vec3 &playerPos,
                   std::map<glm::ivec3, std::shared_ptr<Chunk>, ivec3_less> &chunks,
                   const glm::ivec3 &playerChunkPos,
                   uint32_t gameTicks,
                   const std::vector<AABB> &debugAABBs,
                   bool showDebugOverlay);

    void scheduleChunkGpuCleanup(std::shared_ptr<Chunk> chunk);

    void enqueueDestroy(VmaBuffer &&buffer);
    void enqueueDestroy(VmaImage &&image);
    void enqueueDestroy(VkBuffer buffer, VmaAllocation allocation);
    void enqueueDestroy(AccelerationStructure &&as);

    VkDevice getDevice() const { return m_DeviceContext->getDevice(); }
    VmaAllocator getAllocator() const { return m_DeviceContext->getAllocator(); }
    DeviceContext *getDeviceContext() const { return m_DeviceContext.get(); }
    CommandManager *getCommandManager() const { return m_CommandManager.get(); }
    VkCommandPool getTransferCommandPool() const { return m_TransferCommandPool; }
    RingStagingArena *getArena() const { return m_StagingArena.get(); }
    DebugOverlay *getDebugOverlay() const { return m_debugOverlay.get(); }

private:
    glm::vec3 updateUniformBuffer(uint32_t currentImage, Camera &camera, const glm::vec3 &playerPos);
    void updateLightUbo(uint32_t currentImage, uint32_t gameTicks);
    void buildBlas(const std::vector<std::pair<Chunk *, int>> &chunksToBuild, VkCommandBuffer cmd);
    void buildTlasAsync(const std::vector<std::pair<Chunk *, int>> &drawList, VkCommandBuffer cmd, uint32_t frame);
    void createRayTracingResources();
    void recreateRayTracingShadowImage();
    void createShaderBindingTable();
    void updateRtDescriptorSet(uint32_t frame);

    void createUniformBuffers();
    void createLightUbo();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateDescriptorSets();

    void createCrosshairVertexBuffer();
    void createDebugCubeMesh();
    void loadRayTracingFunctions();
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);
    VmaBuffer createScratchBuffer(VkDeviceSize size);

    void createSkyResources();
    VulkanHandle<VkImageView, ImageViewDeleter> createTexture(const char *path, VmaImage &outImage);
    void generateSphereMesh(float radius, int sectors, int stacks,
                            std::vector<Vertex> &outVertices,
                            std::vector<uint32_t> &outIndices);
    Window &m_Window;
    Settings &m_Settings;
    Player *m_player;
    TerrainGenerator *m_terrainGen;

    std::unique_ptr<InstanceContext> m_InstanceContext;
    std::unique_ptr<DeviceContext> m_DeviceContext;
    std::unique_ptr<SwapChainContext> m_SwapChainContext;
    std::unique_ptr<DescriptorLayout> m_DescriptorLayout;
    std::unique_ptr<PipelineCache> m_PipelineCache;
    std::unique_ptr<CommandManager> m_CommandManager;
    std::unique_ptr<SyncPrimitives> m_SyncPrimitives;
    std::unique_ptr<TextureManager> m_TextureManager;
    std::unique_ptr<RingStagingArena> m_StagingArena;
    std::vector<VmaBuffer> m_blasBuildScratchBuffers[MAX_FRAMES_IN_FLIGHT];
    std::vector<VkDescriptorSet> m_rtDescriptorSets;

    VkCommandPool m_TransferCommandPool{VK_NULL_HANDLE};
    std::unique_ptr<DebugOverlay> m_debugOverlay;

    bool m_rtFunctionsLoaded = false;
    bool m_tlasIsDirty = true;

    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

    AccelerationStructure m_tlas;
    VmaBuffer m_tlasInstanceBuffer;

    VmaImage m_rtShadowImage;
    VulkanHandle<VkImageView, ImageViewDeleter> m_rtShadowImageView;
    VulkanHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter> m_rtDescriptorSetLayout;
    VulkanHandle<VkDescriptorPool, DescriptorPoolDeleter> m_rtDescriptorPool;
    VkDescriptorSet m_rtDescriptorSet = VK_NULL_HANDLE;

    VmaBuffer m_shaderBindingTable;
    VkStridedDeviceAddressRegionKHR m_rgenRegion{};
    VkStridedDeviceAddressRegionKHR m_missRegion{};
    VkStridedDeviceAddressRegionKHR m_hitRegion{};
    VkStridedDeviceAddressRegionKHR m_callRegion{};

    std::vector<VmaBuffer> m_UniformBuffers;
    std::vector<void *> m_UniformBuffersMapped;
    std::vector<VmaBuffer> m_LightUbos;
    std::vector<void *> m_LightUbosMapped;

    VulkanHandle<VkDescriptorPool, DescriptorPoolDeleter> m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;

    VmaBuffer m_CrosshairVertexBuffer;

    VmaBuffer m_SkySphereVertexBuffer;
    VmaBuffer m_SkySphereIndexBuffer;
    uint32_t m_SkySphereIndexCount = 0;
    VmaImage m_SunTexture;
    VulkanHandle<VkImageView, ImageViewDeleter> m_SunTextureView;
    VmaImage m_MoonTexture;
    VulkanHandle<VkImageView, ImageViewDeleter> m_MoonTextureView;

    VmaBuffer m_DebugCubeVertexBuffer;
    VmaBuffer m_DebugCubeIndexBuffer;
    uint32_t m_DebugCubeIndexCount = 0;

    uint32_t m_CurrentFrame{0};
    std::vector<VmaBuffer> m_BufferDestroyQueue[MAX_FRAMES_IN_FLIGHT];
    std::vector<VmaImage> m_ImageDestroyQueue[MAX_FRAMES_IN_FLIGHT];
    std::vector<VmaBuffer> m_asBuildStagingBuffers[MAX_FRAMES_IN_FLIGHT];
    std::vector<std::shared_ptr<Chunk>> m_ChunkCleanupQueue[MAX_FRAMES_IN_FLIGHT];
    std::vector<AccelerationStructure> m_AsDestroyQueue[MAX_FRAMES_IN_FLIGHT];
};
