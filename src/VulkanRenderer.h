#pragma once

#include "Window.h"
#include "Settings.h"
#include "GameObject.h"
#include "Camera.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>
#include <array>

//   wie viele Tiles (16×16‑Pixel Bilder) liegen pro Reihe der Atlas‑Textur?
constexpr int ATLAS_TILES = 1; // bei 256‑px‑Atlas -> 16 Tiles á 16px

struct Vertex
{
    glm::vec3 pos;      // Position
    glm::vec3 color;    // Debug‑Farbe (kann später raus)
    glm::vec2 texCoord; // *****  NEU  *****

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions(); // WICHTIG: 2 -> 3
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

    void drawFrame(Camera &camera, const std::vector<GameObject> &gameObjects);

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

    VkBuffer m_VertexBuffer;
    VkDeviceMemory m_VertexBufferMemory;
    VkBuffer m_IndexBuffer;
    VkDeviceMemory m_IndexBufferMemory;

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
    // ---------- NEU: Helfer, um atlas‑korrekte UVs zu bauen ----------
    static glm::vec2 tileUV(glm::ivec2 tile, glm::vec2 local)
    {
        const float ts = 1.0f / ATLAS_TILES;   // Tile‑Breite im UV‑Raum
        return (glm::vec2(tile) + local) * ts; // Offset + lokaler Anteil
    }

    // ---------- NEU: erzeugt 36 Vertices für EINEN Würfel ----------
    static std::vector<Vertex> makeCube(glm::ivec2 side,
                                        glm::ivec2 top,
                                        glm::ivec2 bottom)
    {
        using V = Vertex;
        const glm::vec3 C = {1, 1, 1}; // Farbe vorerst weiß, wird eh per Textur ersetzt
        std::vector<V> out;
        out.reserve(36);

        auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                        glm::ivec2 tile)
        {
            //     a----b    Wir wollen zwei Triangles a‑b‑c und c‑d‑a
            //     |   /|
            //     d--c
            out.push_back({a, C, tileUV(tile, {0, 0})});
            out.push_back({b, C, tileUV(tile, {1, 0})});
            out.push_back({c, C, tileUV(tile, {1, 1})});

            out.push_back({c, C, tileUV(tile, {1, 1})});
            out.push_back({d, C, tileUV(tile, {0, 1})});
            out.push_back({a, C, tileUV(tile, {0, 0})});
        };

        // Würfel‑Ecken ( +/-0.5 um Mittelpunkt )
        glm::vec3 p000{-.5, -.5, -.5}, p001{-.5, -.5, .5},
            p010{-.5, .5, -.5}, p011{-.5, .5, .5},
            p100{.5, -.5, -.5}, p101{.5, -.5, .5},
            p110{.5, .5, -.5}, p111{.5, .5, .5};

        // +X  (rechte Seite)
        quad(p100, p101, p111, p110, side);
        // -X  (linke Seite)
        quad(p001, p000, p010, p011, side);
        // +Z  (Vorderseite)
        quad(p101, p001, p011, p111, side);
        // -Z  (Rückseite)
        quad(p000, p100, p110, p010, side);
        // +Y  (Deckel)
        quad(p110, p111, p011, p010, top);
        // -Y  (Boden)
        quad(p000, p001, p101, p100, bottom);

        return out;
    }

    // -----------------------------
    //          **AUFRUF**
    // -----------------------------
    std::vector<Vertex> m_Vertices = makeCube(
        /*side  */ {1, 3}, // Diamant‑Erz‑Tile in deinem Atlas
        /*top   */ {1, 3},
        /*bottom*/ {1, 3});

    const std::vector<uint16_t> m_Indices = {
        0, 1, 2, 3, 4, 5,
        6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29,
        30, 31, 32, 33, 34, 35};

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
    void recordCommandBuffer(uint32_t imageIndex, const std::vector<GameObject> &gameObjects);
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