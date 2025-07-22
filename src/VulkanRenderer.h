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

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription,3> getAttributeDescriptions();
};

struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class VulkanRenderer {
    friend class Chunk;

public:
    VulkanRenderer(Window &window,const Settings &settings);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer &) = delete;
    VulkanRenderer &operator=(const VulkanRenderer &) = delete;

    void drawFrame(Camera &camera,const std::map<glm::ivec3,std::unique_ptr<Chunk>,ivec3_less> &chunks);
    void createChunkMeshBuffers(const std::vector<Vertex> &verts,
                                const std::vector<uint32_t> &inds,
                                UploadJob &up,
                                VkBuffer &vb, VkDeviceMemory &vm,
                                VkBuffer &ib, VkDeviceMemory &im);

    VkDevice getDevice() const { return m_Device; }

private:
    Window &m_Window;
    const Settings &m_Settings;

    VkInstance m_Instance{VK_NULL_HANDLE};
    VkSurfaceKHR m_Surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};
    VkDevice m_Device{VK_NULL_HANDLE};
    VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
    VkQueue m_PresentQueue{VK_NULL_HANDLE};

    VkSwapchainKHR m_SwapChain{VK_NULL_HANDLE};
    std::vector<VkImage> m_SwapChainImages;
    VkFormat m_SwapChainImageFormat{};
    VkExtent2D m_SwapChainExtent{};
    std::vector<VkImageView> m_SwapChainImageViews;

    VkRenderPass m_RenderPass{VK_NULL_HANDLE};
    VkPipelineLayout m_PipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_GraphicsPipeline{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> m_SwapChainFramebuffers;

    VkImage m_DepthImage{VK_NULL_HANDLE};
    VkDeviceMemory m_DepthImageMemory{VK_NULL_HANDLE};
    VkImageView m_DepthImageView{VK_NULL_HANDLE};

    VkCommandPool m_CommandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    std::vector<VkFence> m_ImagesInFlight;
    uint32_t m_CurrentFrame{0};

    std::vector<VkBuffer> m_UniformBuffers;
    std::vector<VkDeviceMemory> m_UniformBuffersMemory;
    VkDescriptorSetLayout m_DescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_DescriptorPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> m_DescriptorSets;

    VkImage m_TextureImage{VK_NULL_HANDLE};
    VkDeviceMemory m_TextureImageMemory{VK_NULL_HANDLE};
    VkImageView m_TextureImageView{VK_NULL_HANDLE};
    VkSampler m_TextureSampler{VK_NULL_HANDLE};

    const int MAX_FRAMES_IN_FLIGHT = 2;
    const std::vector<const char *> m_DeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const bool m_EnableValidationLayers = true;
    VkDebugUtilsMessengerEXT m_DebugMessenger{VK_NULL_HANDLE};
    const std::vector<const char *> m_ValidationLayers{"VK_LAYER_KHRONOS_validation"};

    void initVulkan();
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createDepthResources();
    void createFramebuffers();
    void createCommandPool();
    
    void createBuffer(VkDeviceSize size,VkBufferUsageFlags usage,VkMemoryPropertyFlags properties,VkBuffer &buffer,VkDeviceMemory &bufferMemory);
    void copyBuffer(VkBuffer src,VkBuffer dst,VkDeviceSize size,VkFence *outFence=nullptr);
    uint32_t findMemoryType(uint32_t typeFilter,VkMemoryPropertyFlags properties);

    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();
    void updateUniformBuffer(uint32_t currentImage,Camera &camera);
    void recordCommandBuffer(uint32_t imageIndex,const std::map<glm::ivec3,std::unique_ptr<Chunk>,ivec3_less> &chunks);
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    VkImageView createImageView(VkImage image,VkFormat format,VkImageAspectFlags aspectFlags);
    void transitionImageLayout(VkImage image,VkFormat format,VkImageLayout oldLayout,VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer,VkImage image,uint32_t width,uint32_t height);
    void createImage(uint32_t width,uint32_t height,VkFormat format,VkImageTiling tiling,VkImageUsageFlags usage,VkMemoryPropertyFlags properties,VkImage &image,VkDeviceMemory &imageMemory);
    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates,VkImageTiling tiling,VkFormatFeatureFlags features);
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
    VkShaderModule createShaderModule(const std::vector<char> &code);
    static std::vector<char> readFile(const std::string &filename);
};
