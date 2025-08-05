#include "PipelineCache.h"
#include "../Vertex.h"
#include "../RayTracingPushConstants.h"
#include "../command/CommandManager.h"

#include <glm/glm.hpp>
#include <fstream>
#include <vector>
#include <array>
#include <stdexcept>

namespace
{
    std::vector<char> readBinaryFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("failed to open file: " + filename);

        const size_t size = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(size);
        file.seekg(0);
        file.read(buffer.data(), size);
        return buffer;
    }

    VulkanHandle<VkShaderModule, ShaderModuleDeleter> makeShader(VkDevice device, const std::string &path)
    {
        auto code = readBinaryFile(path);
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = code.size();
        ci.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule mod{};
        if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
            throw std::runtime_error("failed to create shader module: " + path);

        return VulkanHandle<VkShaderModule, ShaderModuleDeleter>(mod, {device});
    }

    VkPipeline buildPipeline(VkDevice device,
                             VkRenderPass renderPass,
                             VkPipelineLayout layout,
                             VkPolygonMode polyMode,
                             VkCullModeFlags cullMode,
                             bool enableBlending,
                             bool depthWrite,
                             bool depthTest,
                             const std::string &vertShaderPath,
                             const std::string &fragShaderPath)
    {
        auto vert = makeShader(device, vertShaderPath);
        auto frag = makeShader(device, fragShaderPath);

        VkPipelineShaderStageCreateInfo stages[2]{
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert.get(), "main"},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag.get(), "main"}};

        const auto binding = Vertex::getBindingDescription();
        const auto attrs = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vin{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vin.vertexBindingDescriptionCount = 1;
        vin.pVertexBindingDescriptions = &binding;
        vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vin.pVertexAttributeDescriptions = attrs.data();

        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vp.viewportCount = 1;
        vp.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = polyMode;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth = 1.0f;
        rs.cullMode = cullMode;

        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
        ds.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = 0xf;
        blendAttachment.blendEnable = enableBlending ? VK_TRUE : VK_FALSE;
        if (enableBlending)
        {
            blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount = 1;
        cb.pAttachments = &blendAttachment;

        std::array<VkDynamicState, 2> dyn{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynS{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynS.dynamicStateCount = static_cast<uint32_t>(dyn.size());
        dynS.pDynamicStates = dyn.data();

        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vin;
        info.pInputAssemblyState = &ia;
        info.pViewportState = &vp;
        info.pRasterizationState = &rs;
        info.pMultisampleState = &ms;
        info.pDepthStencilState = &ds;
        info.pColorBlendState = &cb;
        info.pDynamicState = &dynS;
        info.layout = layout;
        info.renderPass = renderPass;
        info.subpass = 0;

        VkPipeline pipe{};
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipe) != VK_SUCCESS)
            throw std::runtime_error("vkCreateGraphicsPipelines failed for " + vertShaderPath);

        return pipe;
    }
}

PipelineCache::PipelineCache(const DeviceContext &deviceContext,
                             const SwapChainContext &swapChainContext,
                             const DescriptorLayout &descriptorLayout)
    : m_DeviceContext(deviceContext),
      m_SwapChainContext(swapChainContext),
      m_DescriptorLayout(descriptorLayout)
{
    createPipelines();

    if (m_DeviceContext.isRayTracingSupported())
    {
        createRayTracingPipeline();
    }

    createItemPipeline();
    createPlayerPipeline();
    createBreakOverlayPipeline();
}

PipelineCache::~PipelineCache() = default;

void PipelineCache::createBreakOverlayPipeline()
{
    VkDevice dev = m_DeviceContext.getDevice();
    VkRenderPass rp = m_SwapChainContext.getRenderPass();

    struct PushConstants
    {
        glm::mat4 model;
        int stage;
    };
    VkPushConstantRange pcRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants)};

    VkDescriptorSetLayout dsls[] = {m_DescriptorLayout.getDescriptorSetLayout()};

    VkPipelineLayoutCreateInfo plCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plCI.setLayoutCount = 1;
    plCI.pSetLayouts = dsls;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges = &pcRange;

    VkPipelineLayout layoutRaw{};
    if (vkCreatePipelineLayout(dev, &plCI, nullptr, &layoutRaw) != VK_SUCCESS)
        throw std::runtime_error("failed to create break overlay pipeline layout!");
    m_BreakOverlayPipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(layoutRaw, {dev});

    m_BreakOverlayPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(
        buildPipeline(dev, rp, m_BreakOverlayPipelineLayout.get(), VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, true, false, true, "shaders/break_overlay.vert.spv", "shaders/break_overlay.frag.spv"), {dev});
}

void PipelineCache::createPlayerPipeline()
{
    VkDevice dev = m_DeviceContext.getDevice();
    VkRenderPass rp = m_SwapChainContext.getRenderPass();

    VkPushConstantRange pcRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)};

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslCI{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslCI.bindingCount = static_cast<uint32_t>(bindings.size());
    dslCI.pBindings = bindings.data();
    VkDescriptorSetLayout dsl;
    vkCreateDescriptorSetLayout(dev, &dslCI, nullptr, &dsl);
    m_PlayerDescriptorSetLayout = VulkanHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter>(dsl, {dev});

    VkPipelineLayoutCreateInfo plCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plCI.setLayoutCount = 1;

    VkDescriptorSetLayout playerDsl = m_PlayerDescriptorSetLayout.get();
    plCI.pSetLayouts = &playerDsl;

    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges = &pcRange;

    VkPipelineLayout layoutRaw{};
    if (vkCreatePipelineLayout(dev, &plCI, nullptr, &layoutRaw) != VK_SUCCESS)
        throw std::runtime_error("failed to create player pipeline layout!");
    m_PlayerPipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(layoutRaw, {dev});

    m_PlayerPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(
        buildPipeline(dev, rp, m_PlayerPipelineLayout.get(), VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, false, true, true, "shaders/player.vert.spv", "shaders/player.frag.spv"), {dev});
}

void PipelineCache::createItemPipeline()
{
    VkDevice dev = m_DeviceContext.getDevice();
    VkRenderPass rp = m_SwapChainContext.getRenderPass();

    struct ItemPushConstant
    {
        glm::mat4 model;
        glm::vec3 tileOrigin;
    };
    VkPushConstantRange pcRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ItemPushConstant)};

    VkDescriptorSetLayout dsl = m_DescriptorLayout.getDescriptorSetLayout();
    VkPipelineLayoutCreateInfo plCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plCI.setLayoutCount = 1;
    plCI.pSetLayouts = &dsl;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges = &pcRange;

    VkPipelineLayout layoutRaw{};
    if (vkCreatePipelineLayout(dev, &plCI, nullptr, &layoutRaw) != VK_SUCCESS)
        throw std::runtime_error("failed to create item pipeline layout!");
    m_ItemPipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(layoutRaw, {dev});

    m_ItemPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(
        buildPipeline(dev, rp, m_ItemPipelineLayout.get(), VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false, true, true, "shaders/item.vert.spv", "shaders/item.frag.spv"), {dev});
}

void PipelineCache::createPipelines()
{
    VkDescriptorSetLayout dsl = m_DescriptorLayout.getDescriptorSetLayout();
    VkPipelineLayoutCreateInfo plCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plCI.setLayoutCount = 1;
    plCI.pSetLayouts = &dsl;
    plCI.pushConstantRangeCount = 0;
    plCI.pPushConstantRanges = nullptr;

    VkPipelineLayout layoutRaw{};
    if (vkCreatePipelineLayout(m_DeviceContext.getDevice(), &plCI, nullptr, &layoutRaw) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout!");
    m_PipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(layoutRaw, {m_DeviceContext.getDevice()});

    VkDevice dev = m_DeviceContext.getDevice();
    VkRenderPass rp = m_SwapChainContext.getRenderPass();

    const std::string defaultVert = "shaders/shader.vert.spv";
    const std::string defaultFrag = "shaders/shader.frag.spv";
    const std::string waterVert = "shaders/water.vert.spv";
    const std::string waterFrag = "shaders/water.frag.spv";

    m_GraphicsPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(
        buildPipeline(dev, rp, m_PipelineLayout.get(), VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, false, true, true, defaultVert, defaultFrag), {dev});

    m_TransparentPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(
        buildPipeline(dev, rp, m_PipelineLayout.get(), VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, true, false, true, defaultVert, defaultFrag), {dev});

    m_WaterPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(
        buildPipeline(dev, rp, m_PipelineLayout.get(), VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, true, false, true, waterVert, waterFrag), {dev});

    m_WireframePipeline = VulkanHandle<VkPipeline, PipelineDeleter>(
        buildPipeline(dev, rp, m_PipelineLayout.get(), VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT, false, true, true, defaultVert, defaultFrag), {dev});

    createSkyPipeline();
    createDebugPipeline();
    createCrosshairPipeline();
    createOutlinePipeline();
}

void PipelineCache::createRayTracingPipeline()
{
    VkDevice device = m_DeviceContext.getDevice();

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    setLayoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout rtDescriptorSetLayout;
    if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &rtDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create ray tracing descriptor set layout!");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &rtDescriptorSetLayout;

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    pcRange.offset = 0;
    pcRange.size = sizeof(RayTracePushConstants);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pcRange;

    VkPipelineLayout layout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create ray tracing pipeline layout!");
    }
    m_RayTracingPipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(layout, {device});

    auto rgen = makeShader(device, "shaders/raytracing/shadow.rgen.spv");
    auto rchit = makeShader(device, "shaders/raytracing/shadow.rchit.spv");
    auto rmiss_primary = makeShader(device, "shaders/raytracing/shadow_primary.rmiss.spv");
    auto rahit_occlusion = makeShader(device, "shaders/raytracing/shadow_occlusion.rahit.spv");
    auto rmiss_occlusion = makeShader(device, "shaders/raytracing/shadow_occlusion.rmiss.spv");

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen.get(), "main"});

    stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss_primary.get(), "main"});
    stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss_occlusion.get(), "main"});
    stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit.get(), "main"});
    stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rahit_occlusion.get(), "main"});

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    groups.push_back({VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});

    groups.push_back({VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});

    groups.push_back({VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});

    groups.push_back({VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});

    groups.push_back({VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, 4, VK_SHADER_UNUSED_KHR});

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 2;
    pipelineInfo.layout = m_RayTracingPipelineLayout.get();

    auto vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    VkPipeline rtPipeline;
    if (vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rtPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create ray tracing pipeline!");
    }
    m_RayTracingPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(rtPipeline, {device});
    vkDestroyDescriptorSetLayout(device, rtDescriptorSetLayout, nullptr);
}

void PipelineCache::createSkyPipeline()
{

    VkPushConstantRange pcRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyPushConstant)};

    VkDescriptorSetLayout dsl = m_DescriptorLayout.getDescriptorSetLayout();
    VkPipelineLayoutCreateInfo plCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plCI.setLayoutCount = 1;
    plCI.pSetLayouts = &dsl;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges = &pcRange;

    VkPipelineLayout layoutRaw{};
    if (vkCreatePipelineLayout(m_DeviceContext.getDevice(), &plCI, nullptr, &layoutRaw) != VK_SUCCESS)
        throw std::runtime_error("failed to create sky pipeline layout!");
    m_SkyPipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(layoutRaw, {m_DeviceContext.getDevice()});

    m_SkyPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(
        buildPipeline(m_DeviceContext.getDevice(),
                      m_SwapChainContext.getRenderPass(),
                      m_SkyPipelineLayout.get(),
                      VK_POLYGON_MODE_FILL,
                      VK_CULL_MODE_NONE,
                      true,
                      false,
                      false,
                      "shaders/sky/sky.vert.spv",
                      "shaders/sky/sky.frag.spv"),
        {m_DeviceContext.getDevice()});
}

void PipelineCache::createOutlinePipeline()
{
    auto vertShader = makeShader(m_DeviceContext.getDevice(), "shaders/outline.vert.spv");
    auto fragShader = makeShader(m_DeviceContext.getDevice(), "shaders/outline.frag.spv");

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShader.get(), "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShader.get(), "main"}};

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(glm::vec3);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrDesc{};
    attrDesc.binding = 0;
    attrDesc.location = 0;
    attrDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDesc.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = &attrDesc;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth = 2.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colorBlendAtt{};
    colorBlendAtt.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAtt;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pcRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pcRange;

    VkDescriptorSetLayout dsl = m_DescriptorLayout.getDescriptorSetLayout();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &dsl;

    VkPipelineLayout layout;
    vkCreatePipelineLayout(m_DeviceContext.getDevice(), &layoutInfo, nullptr, &layout);
    m_OutlinePipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(layout, {m_DeviceContext.getDevice()});

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_OutlinePipelineLayout.get();
    pipelineInfo.renderPass = m_SwapChainContext.getRenderPass();

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(m_DeviceContext.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    m_OutlinePipeline = VulkanHandle<VkPipeline, PipelineDeleter>(pipeline, {m_DeviceContext.getDevice()});
}

void PipelineCache::createDebugPipeline()
{

    auto vertShader = makeShader(m_DeviceContext.getDevice(), "shaders/debug.vert.spv");
    auto fragShader = makeShader(m_DeviceContext.getDevice(), "shaders/debug.frag.spv");

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShader.get(), "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShader.get(), "main"}};

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(glm::vec3);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrDesc{};
    attrDesc.binding = 0;
    attrDesc.location = 0;
    attrDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDesc.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = &attrDesc;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colorBlendAtt{};
    colorBlendAtt.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAtt;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pcRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pcRange;

    VkDescriptorSetLayout dsl = m_DescriptorLayout.getDescriptorSetLayout();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &dsl;

    VkPipelineLayout layout;
    vkCreatePipelineLayout(m_DeviceContext.getDevice(), &layoutInfo, nullptr, &layout);
    m_DebugPipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(layout, {m_DeviceContext.getDevice()});

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_DebugPipelineLayout.get();
    pipelineInfo.renderPass = m_SwapChainContext.getRenderPass();

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(m_DeviceContext.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    m_DebugPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(pipeline, {m_DeviceContext.getDevice()});
}

void PipelineCache::createCrosshairPipeline()
{
    auto vertShader = makeShader(m_DeviceContext.getDevice(), "shaders/crosshair.vert.spv");
    auto fragShader = makeShader(m_DeviceContext.getDevice(), "shaders/crosshair.frag.spv");

    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShader.get(), "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShader.get(), "main"}};

    struct CrosshairVertex
    {
        glm::vec2 pos;
        glm::vec2 uv;
    };
    VkVertexInputBindingDescription bindingDesc{0, sizeof(CrosshairVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    std::array<VkVertexInputAttributeDescription, 2> attrDescs = {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(CrosshairVertex, pos)},
        VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(CrosshairVertex, uv)}};

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.colorWriteMask = 0xf;
    blendAtt.blendEnable = VK_TRUE;
    blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAtt.colorBlendOp = VK_BLEND_OP_ADD;
    blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAtt.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAtt;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data()};

    VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo dslInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &binding};
    VkDescriptorSetLayout dsl;
    vkCreateDescriptorSetLayout(m_DeviceContext.getDevice(), &dslInfo, nullptr, &dsl);

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &dsl;

    VkPipelineLayout layout;
    vkCreatePipelineLayout(m_DeviceContext.getDevice(), &layoutInfo, nullptr, &layout);
    m_CrosshairPipelineLayout = VulkanHandle<VkPipelineLayout, PipelineLayoutDeleter>(layout, {m_DeviceContext.getDevice()});

    vkDestroyDescriptorSetLayout(m_DeviceContext.getDevice(), dsl, nullptr);

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_CrosshairPipelineLayout.get();
    pipelineInfo.renderPass = m_SwapChainContext.getRenderPass();

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(m_DeviceContext.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    m_CrosshairPipeline = VulkanHandle<VkPipeline, PipelineDeleter>(pipeline, {m_DeviceContext.getDevice()});
}