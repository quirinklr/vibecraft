#pragma once

#include "Window.h"
#include "Settings.h"
#include "Camera.h"
#include "UploadJob.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <map>
#include <memory>
#include "math/Ivec3Less.h"

#include "VulkanWrappers.h"

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class VulkanRenderer
{
    friend class Chunk;

public:
    VulkanRenderer(Window &window, const Settings &settings);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer &) = delete;
    VulkanRenderer &operator=(const VulkanRenderer &) = delete;

    void drawFrame(Camera &camera, const std::map<glm::ivec3, std::unique_ptr<Chunk>, ivec3_less> &chunks);

    void createChunkMeshBuffers(const std::vector<Vertex> &v,
                                const std::vector<uint32_t> &i,
                                UploadJob &up,
                                VkBuffer &vb, VmaAllocation &va,
                                VkBuffer &ib, VmaAllocation &ia);

    void enqueueDestroy(VmaBuffer &&buffer);
    void enqueueDestroy(VmaImage &&image);
    void enqueueDestroy(VkBuffer buffer, VmaAllocation allocation);
    
    VkDevice getDevice() const { return m_Device.get(); }

private:
    void cleanupSwapChain();
    void recreateSwapChain();
    Window &m_Window;
    const Settings &m_Settings;

    VulkanHandle<VkInstance, InstanceDeleter> m_Instance;
    VulkanHandle<VkSurfaceKHR, SurfaceDeleter> m_Surface;
    VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};
    VulkanHandle<VkDevice, DeviceDeleter> m_Device;

    VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
    VkQueue m_PresentQueue{VK_NULL_HANDLE};
    VmaAllocator m_Allocator{VK_NULL_HANDLE};

    VulkanHandle<VkSwapchainKHR, SwapchainDeleter> m_SwapChain;
    std::vector<VkImage> m_SwapChainImages;
    VkFormat m_SwapChainImageFormat{};
    VkExtent2D m_SwapChainExtent{};
    std::vector<VulkanHandle<VkImageView, ImageViewDeleter>> m_SwapChainImageViews;

    VulkanHandle<VkRenderPass, RenderPassDeleter> m_RenderPass;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_PipelineLayout;
    VulkanHandle<VkPipeline, PipelineDeleter> m_GraphicsPipeline;
    std::vector<VulkanHandle<VkFramebuffer, FramebufferDeleter>> m_SwapChainFramebuffers;

    VmaImage m_DepthImage;
    VulkanHandle<VkImageView, ImageViewDeleter> m_DepthImageView;

    VulkanHandle<VkCommandPool, CommandPoolDeleter> m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VulkanHandle<VkSemaphore, SemaphoreDeleter>> m_ImageAvailableSemaphores;
    std::vector<VulkanHandle<VkSemaphore, SemaphoreDeleter>> m_RenderFinishedSemaphores;
    std::vector<VulkanHandle<VkFence, FenceDeleter>> m_InFlightFences;
    std::vector<VkFence> m_ImagesInFlight;
    uint32_t m_CurrentFrame{0};

    std::vector<VmaBuffer> m_UniformBuffers;
    std::vector<void *> m_UniformBuffersMapped;

    VulkanHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter> m_DescriptorSetLayout;
    VulkanHandle<VkDescriptorPool, DescriptorPoolDeleter> m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;

    VulkanHandle<VkPipeline, PipelineDeleter> m_CrosshairPipeline;
    VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter> m_CrosshairPipelineLayout;
    VmaBuffer m_CrosshairVertexBuffer;

    VmaImage m_TextureImage;
    VulkanHandle<VkImageView, ImageViewDeleter> m_TextureImageView;
    VulkanHandle<VkSampler, SamplerDeleter> m_TextureSampler;

    const int MAX_FRAMES_IN_FLIGHT = 2;
    const std::vector<const char *> m_DeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const bool m_EnableValidationLayers = true;
    VkDebugUtilsMessengerEXT m_DebugMessenger{VK_NULL_HANDLE};
    const std::vector<const char *> m_ValidationLayers{"VK_LAYER_KHRONOS_validation"};

    std::vector<VmaBuffer> m_BufferDestroyQueue[3];
    std::vector<VmaImage> m_ImageDestroyQueue[3];

    void initVulkan();
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChainAndDependencies();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createDepthResources();
    void createFramebuffers();
    void createCommandPool();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();
    void updateUniformBuffer(uint32_t currentImage, Camera &camera);
    void createCrosshairPipeline();
    void createCrosshairVertexBuffer();
    void recordCommandBuffer(uint32_t imageIndex, const std::map<glm::ivec3, std::unique_ptr<Chunk>, ivec3_less> &chunks);
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();

    VulkanHandle<VkImageView, ImageViewDeleter> createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkFence *outFence = nullptr);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();

    struct QueueFamilyIndices;
    struct SwapChainSupportDetails;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
    VulkanHandle<VkShaderModule, ShaderModuleDeleter> createShaderModule(const std::vector<char> &code);
    static std::vector<char> readFile(const std::string &filename);
};