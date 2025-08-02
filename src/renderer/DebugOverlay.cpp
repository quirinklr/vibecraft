#include "DebugOverlay.h"
#include "renderer/resources/UploadHelpers.h"
#include "renderer/resources/TextureManager.h"
#include "Player.h"
#include "Settings.h"
#include <stdexcept>
#include <vector>
#include <array>
#include <fstream>
#include <cstdio>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace
{
    std::vector<char> readBinaryFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("failed to open file: " + filename);
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        return buffer;
    }

    VulkanHandle<VkShaderModule, ShaderModuleDeleter>
    createShaderModule(VkDevice device, const std::vector<char> &code)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }
        return VulkanHandle<VkShaderModule, ShaderModuleDeleter>(shaderModule, {device});
    }
}

static stbtt_bakedchar cdata[96];

DebugOverlay::DebugOverlay(DeviceContext &deviceContext, VkCommandPool commandPool)
    : m_deviceContext(deviceContext), m_commandPool(commandPool)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_deviceContext.getPhysicalDevice(), &properties);
    m_gpuName = properties.deviceName;
    m_raytracingSupport = m_deviceContext.isRayTracingSupported() ? "Yes" : "No";

    createFontTexture();
}

void DebugOverlay::cleanupPipeline()
{
    m_pipeline = {nullptr, {}};
    m_pipelineLayout = {nullptr, {}};
    m_descriptorPool = {nullptr, {}};
    m_descriptorSetLayout = {nullptr, {}};
    m_vertexBuffer = {};
}

void DebugOverlay::recreate(VkRenderPass renderPass, VkExtent2D viewportExtent)
{
    m_renderPass = renderPass;
    m_viewportExtent = viewportExtent;

    cleanupPipeline();
    createPipeline(renderPass);
}

DebugOverlay::~DebugOverlay() {}

void DebugOverlay::createFontTexture()
{

    std::ifstream ttf_file("C:/Windows/Fonts/consola.ttf", std::ios::binary);
    if (!ttf_file.is_open())
    {
        throw std::runtime_error("Failed to open font file. Make sure 'consola.ttf' is accessible.");
    }
    std::vector<unsigned char> ttf_buffer((std::istreambuf_iterator<char>(ttf_file)), std::istreambuf_iterator<char>());

    const int bitmap_w = 512;
    const int bitmap_h = 512;
    std::vector<unsigned char> font_bitmap(bitmap_w * bitmap_h);

    stbtt_BakeFontBitmap(ttf_buffer.data(), 0, 15.0f, font_bitmap.data(), bitmap_w, bitmap_h, 32, 96, cdata);

    VkDeviceSize imageSize = bitmap_w * bitmap_h;
    VmaBuffer stagingBuffer = UploadHelpers::createDeviceLocalBufferFromData(
        m_deviceContext, m_commandPool, font_bitmap.data(), imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {(uint32_t)bitmap_w, (uint32_t)bitmap_h, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{0, VMA_MEMORY_USAGE_GPU_ONLY};
    m_fontImage = VmaImage(m_deviceContext.getAllocator(), imageInfo, allocInfo);

    VkCommandBuffer cmd = UploadHelpers::beginSingleTimeCommands(m_deviceContext, m_commandPool);
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;

    UploadHelpers::transitionImageLayout(cmd, m_fontImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    UploadHelpers::copyBufferToImage(cmd, stagingBuffer.get(), m_fontImage.get(), bitmap_w, bitmap_h);
    UploadHelpers::transitionImageLayout(cmd, m_fontImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    UploadHelpers::endSingleTimeCommands(m_deviceContext, m_commandPool, cmd);

    m_fontImageView = TextureManager::createImageView(m_deviceContext.getDevice(), m_fontImage.get(), VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSampler sampler;
    vkCreateSampler(m_deviceContext.getDevice(), &samplerInfo, nullptr, &sampler);
    m_fontSampler = VulkanHandle<VkSampler, SamplerDeleter>(sampler, {m_deviceContext.getDevice()});
}

void DebugOverlay::createPipeline(VkRenderPass renderPass)
{

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    VkDescriptorSetLayout dsl;
    vkCreateDescriptorSetLayout(m_deviceContext.getDevice(), &layoutInfo, nullptr, &dsl);
    m_descriptorSetLayout = VulkanHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter>(dsl, {m_deviceContext.getDevice()});

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VkDescriptorPool pool;
    vkCreateDescriptorPool(m_deviceContext.getDevice(), &poolInfo, nullptr, &pool);
    m_descriptorPool = VulkanHandle<VkDescriptorPool, DescriptorPoolDeleter>(pool, {m_deviceContext.getDevice()});

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &dsl;

    VkPipelineLayout pl;
    vkCreatePipelineLayout(m_deviceContext.getDevice(), &pipelineLayoutInfo, nullptr, &pl);
    m_pipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(pl, {m_deviceContext.getDevice()});

    auto vertShader = createShaderModule(m_deviceContext.getDevice(), readBinaryFile("shaders/debug.vert.spv"));
    auto fragShader = createShaderModule(m_deviceContext.getDevice(), readBinaryFile("shaders/debug.frag.spv"));

    VkPipelineShaderStageCreateInfo stages[2] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShader.get(), "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShader.get(), "main"}};
}
void DebugOverlay::update(const Player &player, const Settings &settings, float fps, int64_t seed)
{
}

void DebugOverlay::draw(VkCommandBuffer commandBuffer)
{
}