#include "CommandManager.h"
#include <stdexcept>
#include <array>
#include "../../math/Ivec3Less.h"
#include <Globals.h>
#include <glm/gtc/matrix_transform.hpp>

CommandManager::CommandManager(const DeviceContext &deviceContext, const SwapChainContext &swapChainContext, const PipelineCache &pipelineCache)
    : m_DeviceContext(deviceContext), m_SwapChainContext(swapChainContext), m_PipelineCache(pipelineCache)
{
    createCommandPool();
    createCommandBuffers();
}

CommandManager::~CommandManager() {}

void CommandManager::recordCommandBuffer(
    uint32_t imageIndex, uint32_t currentFrame,
    const std::vector<std::pair<Chunk *, int>> &opaqueChunks,
    const std::vector<std::pair<Chunk *, int>> &transparentChunks,
    const std::vector<VkDescriptorSet> &descriptorSets,
    const glm::vec3 &clearColor,
    const SkyPushConstant &sun_pc,
    const SkyPushConstant &moon_pc,
    bool isSunVisible,
    bool isMoonVisible,
    VkBuffer skySphereVB, VkBuffer skySphereIB, uint32_t skySphereIndexCount,
    VkBuffer crosshairVB, VkBuffer crosshairIB, VkDescriptorSet crosshairDS,
    VkBuffer debugCubeVB, VkBuffer debugCubeIB, uint32_t debugCubeIndexCount,
    const Settings &settings, const std::vector<AABB> &debugAABBs,
    VkBuffer outlineVB,
    uint32_t outlineVertexCount,
    const std::optional<glm::ivec3> &hoveredBlockPos)
{

    VkCommandBuffer cb = m_CommandBuffers[currentFrame];

    const std::array<VkClearValue, 2> clearValues = {
        VkClearValue{.color = {{clearColor.r, clearColor.g, clearColor.b, 1.0f}}},
        VkClearValue{.depthStencil = {1.0f, 0}}};

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = m_SwapChainContext.getRenderPass();
    rp.framebuffer = m_SwapChainContext.getFramebuffers()[imageIndex].get();
    rp.renderArea = {{0, 0}, m_SwapChainContext.getSwapChainExtent()};
    rp.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rp.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cb, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{0, 0,
                  float(m_SwapChainContext.getSwapChainExtent().width),
                  float(m_SwapChainContext.getSwapChainExtent().height),
                  0, 1};
    VkRect2D sc{{0, 0}, m_SwapChainContext.getSwapChainExtent()};
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &sc);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineCache.getSkyPipeline());
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PipelineCache.getSkyPipelineLayout(),
                            0, 1, &descriptorSets[currentFrame], 0, nullptr);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cb, 0, 1, &skySphereVB, offsets);
    vkCmdBindIndexBuffer(cb, skySphereIB, 0, VK_INDEX_TYPE_UINT32);

    if (isSunVisible)
    {
        vkCmdPushConstants(cb, m_PipelineCache.getSkyPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyPushConstant), &sun_pc);
        vkCmdDrawIndexed(cb, skySphereIndexCount, 1, 0, 0, 0);
    }

    if (isMoonVisible)
    {
        vkCmdPushConstants(cb, m_PipelineCache.getSkyPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyPushConstant), &moon_pc);
        vkCmdDrawIndexed(cb, skySphereIndexCount, 1, 0, 0, 0);
    }

    VkPipeline mainPipe = settings.wireframe
                              ? m_PipelineCache.getWireframePipeline()
                              : m_PipelineCache.getGraphicsPipeline();

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipe);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PipelineCache.getGraphicsPipelineLayout(),
                            0, 1, &descriptorSets[currentFrame], 0, nullptr);

    uint32_t instanceOffset = 0;
    for (uint32_t i = 0; i < opaqueChunks.size(); ++i)
    {
        const auto &[chunk, lod] = opaqueChunks[i];
        const ChunkMesh *mesh = chunk->getMesh(lod);

        VkBuffer vertexBuffers[] = {mesh->vertexBuffer};
        VkDeviceSize chunk_offsets[] = {0};
        vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, chunk_offsets);
        vkCmdBindIndexBuffer(cb, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cb, mesh->indexCount, 1, 0, 0, i);
    }

    instanceOffset += static_cast<uint32_t>(opaqueChunks.size());

    if (outlineVertexCount > 0 && hoveredBlockPos)
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineCache.getOutlinePipeline());
        vkCmdSetLineWidth(cb, 2.0f);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_PipelineCache.getOutlinePipelineLayout(),
                                0, 1, &descriptorSets[currentFrame], 0, nullptr);
        VkDeviceSize outline_offsets[] = {0};
        vkCmdBindVertexBuffers(cb, 0, 1, &outlineVB, outline_offsets);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(*hoveredBlockPos));
        vkCmdPushConstants(cb, m_PipelineCache.getOutlinePipelineLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);

        vkCmdDraw(cb, outlineVertexCount, 1, 0, 0);
    }

    if (!settings.wireframe)
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineCache.getWaterPipeline());

        for (uint32_t i = 0; i < transparentChunks.size(); ++i)
        {
            const auto &[chunk, lod] = transparentChunks[i];
            const ChunkMesh *mesh = chunk->getTransparentMesh(lod);

            VkBuffer vertexBuffers[] = {mesh->vertexBuffer};
            VkDeviceSize chunk_offsets[] = {0};
            vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, chunk_offsets);
            vkCmdBindIndexBuffer(cb, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cb, mesh->indexCount, 1, 0, 0, instanceOffset + i);
        }
    }

    if (settings.showCollisionBoxes && !debugAABBs.empty())
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineCache.getDebugPipeline());
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_PipelineCache.getDebugPipelineLayout(),
                                0, 1, &descriptorSets[currentFrame], 0, nullptr);
        VkBuffer vertexBuffers[] = {debugCubeVB};
        VkDeviceSize debug_offsets[] = {0};
        vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, debug_offsets);
        vkCmdBindIndexBuffer(cb, debugCubeIB, 0, VK_INDEX_TYPE_UINT32);

        for (const auto &aabb : debugAABBs)
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), aabb.min);
            model = glm::scale(model, aabb.max - aabb.min);
            vkCmdPushConstants(cb, m_PipelineCache.getDebugPipelineLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
            vkCmdDrawIndexed(cb, debugCubeIndexCount, 1, 0, 0, 0);
        }
    }

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineCache.getCrosshairPipeline());
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineCache.getCrosshairPipelineLayout(), 0, 1, &crosshairDS, 0, nullptr);
    VkDeviceSize crosshair_offsets[] = {0};
    vkCmdBindVertexBuffers(cb, 0, 1, &crosshairVB, crosshair_offsets);
    vkCmdBindIndexBuffer(cb, crosshairIB, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(cb);
}

void CommandManager::createCommandPool()
{
    DeviceContext::QueueFamilyIndices queueFamilyIndices = m_DeviceContext.findQueueFamilies(m_DeviceContext.getPhysicalDevice());
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    VkCommandPool pool;
    if (vkCreateCommandPool(m_DeviceContext.getDevice(), &poolInfo, nullptr, &pool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool!");
    }
    m_CommandPool = VulkanHandle<VkCommandPool, CommandPoolDeleter>(pool, {m_DeviceContext.getDevice()});
}

void CommandManager::createCommandBuffers()
{
    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = m_CommandPool.get();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    allocInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());
    if (vkAllocateCommandBuffers(m_DeviceContext.getDevice(), &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate graphics command buffers");
}

void CommandManager::recordRayTraceCommand(VkCommandBuffer cb, uint32_t currentFrame, VkDescriptorSet rtDescriptorSet,
                                           const VkStridedDeviceAddressRegionKHR *rgenRegion,
                                           const VkStridedDeviceAddressRegionKHR *missRegion,
                                           const VkStridedDeviceAddressRegionKHR *hitRegion,
                                           const VkStridedDeviceAddressRegionKHR *callRegion,
                                           const void *pushConstants, VkImage shadowImage)
{
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_PipelineCache.getRayTracingPipeline());
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_PipelineCache.getRayTracingPipelineLayout(), 0, 1, &rtDescriptorSet, 0, 0);

    vkCmdPushConstants(
        cb,
        m_PipelineCache.getRayTracingPipelineLayout(),
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0,
        sizeof(RayTracePushConstants),
        pushConstants);

    VkExtent2D extent = m_SwapChainContext.getSwapChainExtent();
    auto vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(m_DeviceContext.getDevice(), "vkCmdTraceRaysKHR");

    VkImageMemoryBarrier toGeneral{};
    toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toGeneral.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.image = shadowImage;
    toGeneral.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toGeneral.subresourceRange.levelCount = 1;
    toGeneral.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        0, nullptr,
        0, nullptr,
        1, &toGeneral);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = shadowImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
}