#pragma once

#include "Window.h" // Nötig für m_Window
#include "Settings.h"
#include "Camera.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp> // Nur die Kern-glm
#include <vector>
#include <array>
#include <map>
#include <memory>
#include "math/Ivec3Less.h"

class Chunk; // Forward declaration

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
public:
    VulkanRenderer(Window &window, const Settings &settings);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer &) = delete;
    VulkanRenderer &operator=(const VulkanRenderer &) = delete;

    void drawFrame(
        Camera &camera,
        const std::map<glm::ivec3,
                       std::unique_ptr<Chunk>,
                       ivec3_less> &chunks);
    void createChunkMeshBuffers(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, VkBuffer &vertexBuffer, VkDeviceMemory &vertexMemory, VkBuffer &indexBuffer, VkDeviceMemory &indexMemory);
    VkDevice getDevice() const { return m_Device; }

private:
    Window &m_Window;
    const Settings &m_Settings;
    VkInstance m_Instance;
    VkSurfaceKHR m_Surface;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device;
    VkQueue m_GraphicsQueue;
    VkQueue m_PresentQueue;
    VkSwapchainKHR m_SwapChain;
    std::vector<VkImage> m_SwapChainImages;
    VkFormat m_SwapChainImageFormat;
    VkExtent2D m_SwapChainExtent;
    std::vector<VkImageView> m_SwapChainImageViews;
    VkRenderPass m_RenderPass;
    VkPipelineLayout m_PipelineLayout;
    VkPipeline m_GraphicsPipeline;
    std::vector<VkFramebuffer> m_SwapChainFramebuffers;

    // NEU: Depth Buffer Ressourcen
    VkImage m_DepthImage;
    VkDeviceMemory m_DepthImageMemory;
    VkImageView m_DepthImageView;

    void createDepthResources();

    VkFormat findDepthFormat();

    VkFormat findSupportedFormat(
        const std::vector<VkFormat> &candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features);

    void createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage &image,
        VkDeviceMemory &imageMemory);

    VkImageView createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags);

    VkCommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    std::vector<VkFence> m_ImagesInFlight;
    uint32_t m_CurrentFrame = 0;

    std::vector<VkBuffer> m_UniformBuffers;
    std::vector<VkDeviceMemory> m_UniformBuffersMemory;
    VkDescriptorSetLayout m_DescriptorSetLayout;
    VkDescriptorPool m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;

    VkImage m_TextureImage;
    VkDeviceMemory m_TextureImageMemory;
    VkImageView m_TextureImageView;
    VkSampler m_TextureSampler;

    const int MAX_FRAMES_IN_FLIGHT = 2;

    const std::vector<const char *> m_DeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
    const bool m_EnableValidationLayers = false;
#else
    const bool m_EnableValidationLayers = true;
#endif
    const std::vector<const char *> m_ValidationLayers = {"VK_LAYER_KHRONOS_validation"};

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
    void createFramebuffers();
    void createCommandPool();
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();
    void updateUniformBuffer(uint32_t currentImage, Camera &camera);
    void recordCommandBuffer(
        uint32_t imageIndex,
        const std::map<glm::ivec3,
                       std::unique_ptr<Chunk>,
                       ivec3_less> &chunks);

    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    struct QueueFamilyIndices;
    struct SwapChainSupportDetails;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
    VkShaderModule createShaderModule(const std::vector<char> &code);
    static std::vector<char> readFile(const std::string &filename);
};