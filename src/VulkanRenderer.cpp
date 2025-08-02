#include "VulkanRenderer.h"
#include <stdexcept>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/common.hpp>
#include "math/Ivec3Less.h"
#include "renderer/UniformBufferObject.h"
#include "Globals.h"
#include "generation/TerrainGenerator.h"
#include <stb_image.h>
#include <numeric>
#include "renderer/RayTracingPushConstants.h"

VulkanRenderer::VulkanRenderer(Window &window,
                               Settings &settings,
                               Player *player,
                               TerrainGenerator *terrainGen)
    : m_Window(window), m_Settings(settings), m_player(player), m_terrainGen(terrainGen)
{

    m_InstanceContext = std::make_unique<InstanceContext>(m_Window);
    m_DeviceContext = std::make_unique<DeviceContext>(*m_InstanceContext);

    VkDeviceSize arenaSize = 64ull * 1024 * 1024;
    m_StagingArena = std::make_unique<RingStagingArena>(*m_DeviceContext, arenaSize);

    if (m_DeviceContext->hasTransferQueue())
    {
        VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        ci.queueFamilyIndex = m_DeviceContext->findQueueFamilies(
                                                 m_DeviceContext->getPhysicalDevice())
                                  .transferFamily.value();
        vkCreateCommandPool(m_DeviceContext->getDevice(), &ci, nullptr, &m_TransferCommandPool);
    }

    m_SwapChainContext = std::make_unique<SwapChainContext>(
        m_Window, m_Settings, *m_InstanceContext, *m_DeviceContext);
    m_DescriptorLayout = std::make_unique<DescriptorLayout>(*m_DeviceContext);
    m_PipelineCache = std::make_unique<PipelineCache>(
        *m_DeviceContext, *m_SwapChainContext, *m_DescriptorLayout);
    m_CommandManager = std::make_unique<CommandManager>(
        *m_DeviceContext, *m_SwapChainContext, *m_PipelineCache);
    m_SyncPrimitives = std::make_unique<SyncPrimitives>(
        *m_DeviceContext, *m_SwapChainContext);

    m_debugOverlay = std::make_unique<DebugOverlay>(
        *m_DeviceContext,
        m_CommandManager->getCommandPool(),
        m_SwapChainContext->getRenderPass(),
        m_SwapChainContext->getSwapChainExtent());

    m_TextureManager = std::make_unique<TextureManager>(
        *m_DeviceContext, m_CommandManager->getCommandPool());
    m_TextureManager->createTextureImage("textures/blocks_atlas.png");
    m_TextureManager->createTextureImageView();
    m_TextureManager->createTextureSampler();

    createSkyResources();
    createLightUbo();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCrosshairVertexBuffer();
    createDebugCubeMesh();

    if (m_DeviceContext->isRayTracingSupported())
    {
        loadRayTracingFunctions();
        createRayTracingResources();

        m_rtDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = m_rtDescriptorPool.get();
        ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        std::vector<VkDescriptorSetLayout> layouts(
            MAX_FRAMES_IN_FLIGHT, m_rtDescriptorSetLayout.get());
        ai.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(m_DeviceContext->getDevice(),
                                     &ai, m_rtDescriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate RT descriptor sets");

        createShaderBindingTable();
    }
}

VulkanRenderer::~VulkanRenderer()
{

    if (m_DeviceContext && m_DeviceContext->getDevice() != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_DeviceContext->getDevice());
    }

    m_tlas.destroy(m_DeviceContext->getDevice());

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {

        m_blasBuildScratchBuffers[i].clear();
        m_asBuildStagingBuffers[i].clear();
        m_BufferDestroyQueue[i].clear();
        m_ImageDestroyQueue[i].clear();

        for (auto &as : m_AsDestroyQueue[i])
        {
            as.destroy(m_DeviceContext->getDevice());
        }
        m_AsDestroyQueue[i].clear();
    }

    if (m_TransferCommandPool != VK_NULL_HANDLE && m_DeviceContext)
    {
        vkDestroyCommandPool(m_DeviceContext->getDevice(), m_TransferCommandPool, nullptr);
        m_TransferCommandPool = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::drawFrame(Camera &camera,
                               const glm::vec3 &playerPos,
                               std::map<glm::ivec3, std::shared_ptr<Chunk>, ivec3_less> &chunks,
                               const glm::ivec3 &playerChunkPos,
                               uint32_t gameTicks,
                               const std::vector<AABB> &debugAABBs,
                               bool showDebugOverlay)
{
    const uint32_t slot = m_CurrentFrame;

    vkWaitForFences(m_DeviceContext->getDevice(), 1, m_SyncPrimitives->getInFlightFencePtr(slot), VK_TRUE, UINT64_MAX);

    VkResult deviceStatus = vkGetFenceStatus(m_DeviceContext->getDevice(), m_SyncPrimitives->getInFlightFence(slot));
    if (deviceStatus == VK_ERROR_DEVICE_LOST)
    {
        throw std::runtime_error("Vulkan device lost detected after fence wait!");
    }

    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(
        m_DeviceContext->getDevice(), m_SwapChainContext->getSwapChain(),
        UINT64_MAX,
        m_SyncPrimitives->getImageAvailableSemaphore(slot),
        VK_NULL_HANDLE, &imageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        m_SwapChainContext->recreateSwapChain();
        if (m_debugOverlay)
            m_debugOverlay->onWindowResize(m_SwapChainContext->getSwapChainExtent());
        return;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image");
    }

    m_asBuildStagingBuffers[slot].clear();
    m_blasBuildScratchBuffers[slot].clear();
    m_BufferDestroyQueue[slot].clear();
    m_ImageDestroyQueue[slot].clear();
    for (auto &as : m_AsDestroyQueue[slot])
    {
        as.destroy(m_DeviceContext->getDevice());
    }
    m_AsDestroyQueue[slot].clear();

    updateLightUbo(slot, gameTicks);
    glm::vec3 skyColor = updateUniformBuffer(slot, camera, playerPos);
    updateDescriptorSets();
    if (showDebugOverlay && m_debugOverlay)
    {
        m_debugOverlay->update(*m_player, m_Settings, 250.f, m_terrainGen->getSeed());
    }

    std::vector<std::pair<Chunk *, int>> renderChunks;
    renderChunks.reserve(chunks.size());
    const Frustum &fr = camera.getFrustum();
    for (auto &[pos, ch] : chunks)
    {
        ch->markReady(*this);
        if (!fr.intersects(ch->getAABB()))
            continue;

        float d = glm::distance(glm::vec2(pos), glm::vec2(playerChunkPos));
        int req = (!m_Settings.lodDistances.empty() && d <= m_Settings.lodDistances[0]) ? 0 : 1;
        int best = ch->getBestAvailableLOD(req);
        if (best != -1)
        {
            renderChunks.emplace_back(ch.get(), best);
        }
    }

    VkCommandBuffer cmd = m_CommandManager->getCommandBuffer(slot);
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &bi);

    if (m_DeviceContext->isRayTracingSupported() && (m_Settings.rayTracingFlags & SettingsEnums::SHADOWS))
    {
        try
        {
            buildBlas(renderChunks, cmd);
            buildTlasAsync(renderChunks, cmd, slot);

            if (m_tlas.handle != VK_NULL_HANDLE)
            {
                updateRtDescriptorSet(slot);

                RayTracePushConstants pc{};
                pc.viewInverse = glm::inverse(camera.getViewMatrix());
                pc.projInverse = glm::inverse(camera.getProjectionMatrix());
                pc.cameraPos = playerPos + glm::vec3(0, 1.8f * 0.9f, 0);
                memcpy(&pc.lightDirection, m_LightUbosMapped[slot], sizeof(glm::vec3));

                m_CommandManager->recordRayTraceCommand(
                    cmd, slot, m_rtDescriptorSets[slot],
                    &m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion,
                    &pc, m_rtShadowImage.get());

                VkMemoryBarrier memoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(
                    cmd,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
            }
            else
            {
                if (m_Settings.rayTracingFlags & SettingsEnums::SHADOWS)
                {
                    m_Settings.rayTracingFlags &= ~SettingsEnums::SHADOWS;
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Exception during ray tracing setup: " << e.what() << ", disabling shadows.\n";
            m_Settings.rayTracingFlags &= ~SettingsEnums::SHADOWS;
            m_tlas.handle = VK_NULL_HANDLE;
        }
    }

    float time_of_day = static_cast<float>(gameTicks) / 24000.0f;
    float sun_angle = time_of_day * 2.0f * glm::pi<float>() - glm::half_pi<float>();
    float moon_angle = sun_angle + glm::pi<float>();

    glm::mat4 invView = glm::inverse(camera.getViewMatrix());
    glm::vec3 camPos = glm::vec3(invView[3]);

    glm::vec3 sunDir = glm::normalize(glm::vec3(sin(sun_angle), cos(sun_angle), 0.2f));
    glm::vec3 moonDir = glm::normalize(glm::vec3(sin(moon_angle), cos(moon_angle), 0.2f));

    auto makeBillboard = [&](const glm::vec3 &objPos, const glm::vec3 &cam, const glm::vec3 &up)
    {
        glm::vec3 look = glm::normalize(cam - objPos);
        glm::vec3 right = glm::normalize(glm::cross(up, look));
        glm::vec3 up2 = glm::cross(look, right);
        glm::mat4 M(glm::vec4(right, 0), glm::vec4(up2, 0), glm::vec4(look, 0), glm::vec4(objPos, 1));
        return M;
    };

    constexpr float DIST = 400.f;
    constexpr float SIZE = 50.f;
    glm::vec3 sunPos = camPos + sunDir * DIST;
    glm::vec3 moonPos = camPos + moonDir * DIST;

    SkyPushConstant sun_pc;
    sun_pc.model = makeBillboard(sunPos, camPos, {0, 1, 0}) * glm::scale(glm::mat4(1.f), glm::vec3(SIZE));
    sun_pc.is_sun = 1;

    SkyPushConstant moon_pc;
    moon_pc.model = makeBillboard(moonPos, camPos, {0, 1, 0}) * glm::scale(glm::mat4(1.f), glm::vec3(SIZE));
    moon_pc.is_sun = 0;

    bool isSunVisible = sunDir.y > -0.1f;
    bool isMoonVisible = moonDir.y > -0.1f;

    m_CommandManager->recordCommandBuffer(
        imageIndex, slot, renderChunks, m_DescriptorSets,
        skyColor, sun_pc, moon_pc, isSunVisible, isMoonVisible,
        m_SkySphereVertexBuffer.get(), m_SkySphereIndexBuffer.get(), m_SkySphereIndexCount,
        m_CrosshairVertexBuffer.get(),
        m_DebugCubeVertexBuffer.get(), m_DebugCubeIndexBuffer.get(), m_DebugCubeIndexCount,
        m_Settings, debugAABBs);

    vkEndCommandBuffer(cmd);

    VkSemaphore waitSemaphores[] = {m_SyncPrimitives->getImageAvailableSemaphore(slot)};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {m_SyncPrimitives->getRenderFinishedSemaphore(slot)};

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = waitSemaphores;
    si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = signalSemaphores;

    {
        std::scoped_lock lk(gGraphicsQueueMutex);
        vkResetFences(m_DeviceContext->getDevice(), 1, m_SyncPrimitives->getInFlightFencePtr(slot));
        if (vkQueueSubmit(m_DeviceContext->getGraphicsQueue(), 1, &si, m_SyncPrimitives->getInFlightFence(slot)) != VK_SUCCESS)
        {
            throw std::runtime_error("vkQueueSubmit failed");
        }
    }

    VkSwapchainKHR swapChains[] = {m_SwapChainContext->getSwapChain()};
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = signalSemaphores;
    pi.swapchainCount = 1;
    pi.pSwapchains = swapChains;
    pi.pImageIndices = &imageIndex;

    {
        std::scoped_lock lk(gGraphicsQueueMutex);
        res = vkQueuePresentKHR(m_DeviceContext->getPresentQueue(), &pi);
    }

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || m_Window.wasWindowResized())
    {
        m_Window.resetWindowResizedFlag();
        m_SwapChainContext->recreateSwapChain();
        if (m_debugOverlay)
            m_debugOverlay->onWindowResize(m_SwapChainContext->getSwapChainExtent());

        recreateRayTracingShadowImage();
    }

    else if (res == VK_ERROR_DEVICE_LOST)
    {
        throw std::runtime_error("Vulkan device lost during present");
    }
    else if (res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swap chain image");
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::recreateRayTracingShadowImage()
{
    if (!m_DeviceContext->isRayTracingSupported())
        return;

    if (m_rtShadowImage.get() != VK_NULL_HANDLE)
        enqueueDestroy(std::move(m_rtShadowImage));

    if (m_rtShadowImageView.get() != VK_NULL_HANDLE)
        m_rtShadowImageView = {};

    createRayTracingResources();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        updateRtDescriptorSet(i);
}

void VulkanRenderer::loadRayTracingFunctions()
{
    vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(m_DeviceContext->getDevice(), "vkGetBufferDeviceAddressKHR");
    vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(m_DeviceContext->getDevice(), "vkCreateAccelerationStructureKHR");
    vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(m_DeviceContext->getDevice(), "vkDestroyAccelerationStructureKHR");
    vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(m_DeviceContext->getDevice(), "vkGetAccelerationStructureBuildSizesKHR");
    vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(m_DeviceContext->getDevice(), "vkGetAccelerationStructureDeviceAddressKHR");
    vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(m_DeviceContext->getDevice(), "vkCmdBuildAccelerationStructuresKHR");
    vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(m_DeviceContext->getDevice(), "vkCreateRayTracingPipelinesKHR");
    vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(m_DeviceContext->getDevice(), "vkGetRayTracingShaderGroupHandlesKHR");
    vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(m_DeviceContext->getDevice(), "vkCmdTraceRaysKHR");
    m_rtFunctionsLoaded = true;
}

void VulkanRenderer::createRayTracingResources()
{
    VkExtent2D extent = m_SwapChainContext->getSwapChainExtent();
    VkFormat shadowFormat = VK_FORMAT_R32_SFLOAT;
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(m_DeviceContext->getPhysicalDevice(), shadowFormat, &props);
    if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        shadowFormat = VK_FORMAT_R8_UNORM;

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = shadowFormat;
    imgInfo.extent = {extent.width, extent.height, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    m_rtShadowImage = VmaImage(m_DeviceContext->getAllocator(), imgInfo, allocCI);
    if (m_rtShadowImage.get() == VK_NULL_HANDLE)
        throw std::runtime_error("vmaCreateImage failed for RT shadow image");

    m_rtShadowImageView = TextureManager::createImageView(
        m_DeviceContext->getDevice(), m_rtShadowImage.get(),
        shadowFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    VkCommandBuffer cmd = UploadHelpers::beginSingleTimeCommands(
        *m_DeviceContext, m_CommandManager->getCommandPool());

    VkImageSubresourceRange rng{};
    rng.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    rng.baseMipLevel = 0;
    rng.levelCount = 1;
    rng.baseArrayLayer = 0;
    rng.layerCount = 1;

    UploadHelpers::transitionImageLayout(
        cmd, m_rtShadowImage.get(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        rng,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

    UploadHelpers::endSingleTimeCommands(
        *m_DeviceContext, m_CommandManager->getCommandPool(), cmd);

    std::array<VkDescriptorSetLayoutBinding, 2> b{};
    b[0].binding = 0;
    b[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    b[0].descriptorCount = 1;
    b[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    b[1].binding = 1;
    b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    b[1].descriptorCount = 1;
    b[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    li.bindingCount = static_cast<uint32_t>(b.size());
    li.pBindings = b.data();

    VkDescriptorSetLayout tmpLayout;
    vkCreateDescriptorSetLayout(
        m_DeviceContext->getDevice(), &li, nullptr, &tmpLayout);

    m_rtDescriptorSetLayout = VulkanHandle<
        VkDescriptorSetLayout, DescriptorSetLayoutDeleter>(
        tmpLayout, {m_DeviceContext->getDevice()});

    std::array<VkDescriptorPoolSize, 2> ps{};
    ps[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    ps[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    ps[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    ps[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = MAX_FRAMES_IN_FLIGHT;
    pi.poolSizeCount = static_cast<uint32_t>(ps.size());
    pi.pPoolSizes = ps.data();

    VkDescriptorPool pool;
    vkCreateDescriptorPool(m_DeviceContext->getDevice(), &pi, nullptr, &pool);

    m_rtDescriptorPool = VulkanHandle<
        VkDescriptorPool, DescriptorPoolDeleter>(
        pool, {m_DeviceContext->getDevice()});
}

void VulkanRenderer::createShaderBindingTable()
{
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 props2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(m_DeviceContext->getPhysicalDevice(), &props2);

    const uint32_t handleSize = rtProps.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = (handleSize + rtProps.shaderGroupBaseAlignment - 1) & ~(rtProps.shaderGroupBaseAlignment - 1);

    const uint32_t groupCount = 5;
    const uint32_t sbtSize = groupCount * handleSizeAligned;

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    vkGetRayTracingShaderGroupHandlesKHR(m_DeviceContext->getDevice(), m_PipelineCache->getRayTracingPipeline(), 0, groupCount, sbtSize, shaderHandleStorage.data());

    m_shaderBindingTable = UploadHelpers::createDeviceLocalBufferFromData(
        *m_DeviceContext, m_CommandManager->getCommandPool(), shaderHandleStorage.data(), sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    VkDeviceAddress sbtAddress = getBufferDeviceAddress(m_shaderBindingTable.get());

    m_rgenRegion.deviceAddress = sbtAddress;
    m_rgenRegion.stride = handleSizeAligned;
    m_rgenRegion.size = handleSizeAligned;

    m_missRegion.deviceAddress = sbtAddress + 1 * handleSizeAligned;
    m_missRegion.stride = handleSizeAligned;
    m_missRegion.size = 2 * handleSizeAligned;

    m_hitRegion.deviceAddress = sbtAddress + 3 * handleSizeAligned;
    m_hitRegion.stride = handleSizeAligned;
    m_hitRegion.size = 2 * handleSizeAligned;

    m_callRegion.deviceAddress = 0;
    m_callRegion.stride = 0;
    m_callRegion.size = 0;
}

void VulkanRenderer::updateRtDescriptorSet(uint32_t frame)
{
    if (m_tlas.handle == VK_NULL_HANDLE)
        return;

    VkDescriptorSet dst = m_rtDescriptorSets[frame];

    VkWriteDescriptorSetAccelerationStructureKHR asInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &m_tlas.handle;

    VkWriteDescriptorSet asWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    asWrite.pNext = &asInfo;
    asWrite.dstSet = dst;
    asWrite.dstBinding = 0;
    asWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    asWrite.descriptorCount = 1;

    VkDescriptorImageInfo img{};
    img.imageView = m_rtShadowImageView.get();
    img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet imgWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    imgWrite.dstSet = dst;
    imgWrite.dstBinding = 1;
    imgWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imgWrite.descriptorCount = 1;
    imgWrite.pImageInfo = &img;

    std::array<VkWriteDescriptorSet, 2> w = {asWrite, imgWrite};
    vkUpdateDescriptorSets(m_DeviceContext->getDevice(),
                           static_cast<uint32_t>(w.size()), w.data(), 0, nullptr);
}

void VulkanRenderer::buildBlas(const std::vector<std::pair<Chunk *, int>> &chunksToBuild, VkCommandBuffer cmd)
{
    if (!m_rtFunctionsLoaded || cmd == VK_NULL_HANDLE)
    {
        return;
    }

    std::vector<std::pair<Chunk *, int>> dirty;
    dirty.reserve(chunksToBuild.size());
    for (const auto &p : chunksToBuild)
    {
        if (p.first->m_blas_dirty.load(std::memory_order_acquire) && p.first->getState() == Chunk::State::GPU_READY)
        {
            dirty.emplace_back(p);
        }
    }

    if (dirty.empty())
    {
        return;
    }

    m_tlasIsDirty = true;

    const size_t MAX_BLAS_PER_FRAME = 32;
    if (dirty.size() > MAX_BLAS_PER_FRAME)
    {
        dirty.resize(MAX_BLAS_PER_FRAME);
    }

    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos;
    std::vector<VkAccelerationStructureGeometryKHR> geometries;
    std::vector<VkAccelerationStructureGeometryTrianglesDataKHR> triangles;
    std::vector<uint32_t> triangleCounts;
    std::vector<ChunkMesh *> targetMeshes;

    buildInfos.reserve(dirty.size());
    geometries.reserve(dirty.size());
    triangles.reserve(dirty.size());
    triangleCounts.reserve(dirty.size());
    targetMeshes.reserve(dirty.size());

    for (auto &p : dirty)
    {
        Chunk *chunk = p.first;
        int lod = p.second;
        ChunkMesh *mesh = chunk->getMesh(lod);

        if (!mesh || mesh->indexCount == 0 || mesh->vertexCount == 0 || mesh->vertexBuffer.get() == VK_NULL_HANDLE || mesh->indexBuffer.get() == VK_NULL_HANDLE)
        {
            chunk->m_blas_dirty.store(false, std::memory_order_release);
            continue;
        }

        if (mesh->blas.handle != VK_NULL_HANDLE)
            enqueueDestroy(std::move(mesh->blas));

        VkDeviceAddress vAddr = getBufferDeviceAddress(mesh->vertexBuffer.get());
        VkDeviceAddress iAddr = getBufferDeviceAddress(mesh->indexBuffer.get());

        if (vAddr == 0 || iAddr == 0)
        {
            std::cout << "[WARNING] buildBlas: Skipping chunk at (" << chunk->getPos().x << ", " << chunk->getPos().z
                      << ") due to null buffer device address. The mesh might still be uploading." << std::endl;
            continue;
        }

        triangles.push_back({VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                             nullptr,
                             VK_FORMAT_R32G32B32_SFLOAT,
                             {.deviceAddress = vAddr},
                             sizeof(Vertex),
                             mesh->vertexCount > 0 ? mesh->vertexCount - 1 : 0,
                             VK_INDEX_TYPE_UINT32,
                             {.deviceAddress = iAddr},
                             0});

        geometries.push_back({VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                              nullptr,
                              VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                              {.triangles = {}},
                              VK_GEOMETRY_OPAQUE_BIT_KHR});

        buildInfos.push_back({VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                              nullptr,
                              VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                              VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                              VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                              0,
                              0,
                              1,
                              nullptr,
                              {}});

        triangleCounts.push_back(mesh->indexCount / 3);
        targetMeshes.push_back(mesh);
    }

    if (buildInfos.empty())
        return;

    std::vector<VkAccelerationStructureBuildSizesInfoKHR> sizeInfos(buildInfos.size(), {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR});
    std::vector<const VkAccelerationStructureBuildRangeInfoKHR *> rangesPtrs(buildInfos.size());
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges(buildInfos.size());

    for (size_t i = 0; i < buildInfos.size(); ++i)
    {
        geometries[i].geometry.triangles = triangles[i];
        buildInfos[i].pGeometries = &geometries[i];

        vkGetAccelerationStructureBuildSizesKHR(m_DeviceContext->getDevice(),
                                                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &buildInfos[i], &triangleCounts[i], &sizeInfos[i]);

        createAccelerationStructure(*m_DeviceContext, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, sizeInfos[i], targetMeshes[i]->blas);
        buildInfos[i].dstAccelerationStructure = targetMeshes[i]->blas.handle;

        VmaBuffer scratch = createScratchBuffer(sizeInfos[i].buildScratchSize);
        buildInfos[i].scratchData.deviceAddress = getBufferDeviceAddress(scratch.get());
        m_blasBuildScratchBuffers[m_CurrentFrame].push_back(std::move(scratch));

        ranges[i].primitiveCount = triangleCounts[i];
        rangesPtrs[i] = &ranges[i];
    }

    vkCmdBuildAccelerationStructuresKHR(cmd, static_cast<uint32_t>(buildInfos.size()),
                                        buildInfos.data(), rangesPtrs.data());

    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    for (size_t i = 0; i < targetMeshes.size(); ++i)
    {
        VkAccelerationStructureDeviceAddressInfoKHR ai{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
        ai.accelerationStructure = targetMeshes[i]->blas.handle;
        targetMeshes[i]->blas.deviceAddress =
            vkGetAccelerationStructureDeviceAddressKHR(
                m_DeviceContext->getDevice(), &ai);
    }

    for (auto &p : dirty)
        p.first->m_blas_dirty.store(false, std::memory_order_release);
}

void VulkanRenderer::buildTlasAsync(const std::vector<std::pair<Chunk *, int>> &drawList,
                                    VkCommandBuffer cmd, uint32_t frame)
{
    if (!m_rtFunctionsLoaded || !m_DeviceContext->isRayTracingSupported() || cmd == VK_NULL_HANDLE)
        return;

    if (!m_tlasIsDirty)
    {
        return;
    }

    try
    {
        std::vector<VkAccelerationStructureInstanceKHR> instances;
        instances.reserve(drawList.size());

        for (auto &p : drawList)
        {
            const Chunk *c = p.first;
            const ChunkMesh *m = c->getMesh(p.second);

            if (!m || m->blas.handle == VK_NULL_HANDLE || c->m_blas_dirty.load(std::memory_order_acquire))
                continue;

            VkAccelerationStructureInstanceKHR inst{};
            glm::mat4 tr = glm::transpose(c->getModelMatrix());
            memcpy(&inst.transform, &tr, sizeof(inst.transform));
            inst.mask = 0xFF;
            inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            inst.accelerationStructureReference = m->blas.deviceAddress;
            instances.push_back(inst);
        }

        if (instances.empty())
        {
            if (m_tlas.handle != VK_NULL_HANDLE)
            {
                enqueueDestroy(std::move(m_tlas));
                m_tlas.handle = VK_NULL_HANDLE;
            }
            m_tlasIsDirty = false;
            return;
        }

        VkDeviceSize sizeBytes = sizeof(instances[0]) * instances.size();

        if (m_tlasInstanceBuffer.get() != VK_NULL_HANDLE)
        {
            enqueueDestroy(std::move(m_tlasInstanceBuffer));
        }

        m_tlasInstanceBuffer = UploadHelpers::createDeviceLocalBufferFromDataWithStaging(
            *m_DeviceContext, cmd,
            instances.data(), sizeBytes,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            m_asBuildStagingBuffers[frame]);

        VkDeviceAddress instAddr = getBufferDeviceAddress(m_tlasInstanceBuffer.get());

        VkAccelerationStructureGeometryInstancesDataKHR instData{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
        instData.data.deviceAddress = instAddr;

        static VkAccelerationStructureGeometryKHR geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geom.geometry.instances = instData;

        uint32_t primCount = static_cast<uint32_t>(instances.size());
        VkAccelerationStructureBuildGeometryInfoKHR build{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        build.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build.geometryCount = 1;
        build.pGeometries = &geom;

        VkAccelerationStructureBuildSizesInfoKHR sz{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(m_DeviceContext->getDevice(),
                                                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &build, &primCount, &sz);

        if (m_tlas.handle != VK_NULL_HANDLE)
        {
            enqueueDestroy(std::move(m_tlas));
        }
        createAccelerationStructure(*m_DeviceContext, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, sz, m_tlas);
        build.dstAccelerationStructure = m_tlas.handle;

        VmaBuffer scratch = createScratchBuffer(sz.buildScratchSize);
        build.scratchData.deviceAddress = getBufferDeviceAddress(scratch.get());
        m_blasBuildScratchBuffers[m_CurrentFrame].push_back(std::move(scratch));

        VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primCount};
        const VkAccelerationStructureBuildRangeInfoKHR *pRange = &range;

            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &build, &pRange);

        VkMemoryBarrier tlasBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        tlasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        tlasBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             0, 1, &tlasBarrier,
                             0, nullptr,
                             0, nullptr);

        VkBufferMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = m_tlasInstanceBuffer.get();
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            0, nullptr,
            1, &barrier,
            0, nullptr);

        VkAccelerationStructureDeviceAddressInfoKHR dai{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
        dai.accelerationStructure = m_tlas.handle;
        m_tlas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(m_DeviceContext->getDevice(), &dai);

        m_tlasIsDirty = false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in buildTlasAsync: " << e.what() << std::endl;
        return;
    }
}

VkDeviceAddress VulkanRenderer::getBufferDeviceAddress(VkBuffer buffer)
{

    if (buffer == VK_NULL_HANDLE)
    {
        return 0;
    }
    VkBufferDeviceAddressInfoKHR info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    info.buffer = buffer;
    return vkGetBufferDeviceAddressKHR(m_DeviceContext->getDevice(), &info);
}

VmaBuffer VulkanRenderer::createScratchBuffer(VkDeviceSize size)
{
    VkDeviceSize align = m_DeviceContext->getScratchAlignment();
    size = (size + align - 1) & ~(align - 1);

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    return VmaBuffer(m_DeviceContext->getAllocator(), bufferInfo, allocInfo);
}

glm::vec3 VulkanRenderer::updateUniformBuffer(uint32_t currentImage, Camera &camera, const glm::vec3 &playerPos)
{
    const float eyeHeight = 1.8f * 0.9f;
    glm::vec3 cameraWorldPos = playerPos + glm::vec3(0.0f, eyeHeight, 0.0f);

    LightUbo lightUbo;
    memcpy(&lightUbo, m_LightUbosMapped[currentImage], sizeof(LightUbo));

    float sunUpFactor = glm::smoothstep(-0.25f, 0.25f, lightUbo.lightDirection.y);

    glm::vec3 dayColor(0.5f, 0.7f, 1.0f);

    glm::vec3 nightColor(0.04f, 0.06f, 0.1f);
    glm::vec3 sunsetColor(1.0f, 0.45f, 0.15f);

    glm::vec3 skyColor = glm::mix(nightColor, dayColor, sunUpFactor);

    float sunsetFactor = pow(1.0f - abs(lightUbo.lightDirection.y), 16.0f);
    skyColor = glm::mix(skyColor, sunsetColor, sunsetFactor);

    UniformBufferObject ubo{};
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.cameraPos = cameraWorldPos;
    ubo.skyColor = skyColor;
    ubo.time = static_cast<float>(glfwGetTime());
    ubo.isUnderwater = (cameraWorldPos.y < TerrainGenerator::SEA_LEVEL) ? 1 : 0;
    ubo.flags = m_Settings.rayTracingFlags;

    memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));

    return skyColor;
}

void VulkanRenderer::enqueueDestroy(AccelerationStructure &&as)
{
    if (as.handle != VK_NULL_HANDLE)
    {
        m_AsDestroyQueue[m_CurrentFrame].push_back(std::move(as));
    }
}

void VulkanRenderer::enqueueDestroy(VmaBuffer &&buffer)
{
    if (buffer.get() != VK_NULL_HANDLE)
    {
        m_BufferDestroyQueue[m_CurrentFrame].push_back(std::move(buffer));
    }
}

void VulkanRenderer::enqueueDestroy(VmaImage &&image)
{
    if (image.get() != VK_NULL_HANDLE)
    {
        m_ImageDestroyQueue[m_CurrentFrame].push_back(std::move(image));
    }
}

void VulkanRenderer::enqueueDestroy(VkBuffer buffer, VmaAllocation allocation)
{
    if (buffer != VK_NULL_HANDLE)
    {
        m_BufferDestroyQueue[m_CurrentFrame].emplace_back(m_DeviceContext->getAllocator(), buffer, allocation);
    }
}

void VulkanRenderer::updateLightUbo(uint32_t currentImage, uint32_t gameTicks)
{
    LightUbo ubo{};

    float time_of_day = static_cast<float>(gameTicks) / 24000.0f;
    float sun_angle = time_of_day * 2.0f * glm::pi<float>() - glm::half_pi<float>();

    glm::vec3 sunDir = glm::normalize(glm::vec3(sin(sun_angle), cos(sun_angle), 0.2f));

    ubo.lightDirection = sunDir;

    memcpy(m_LightUbosMapped[currentImage], &ubo, sizeof(ubo));
}

void VulkanRenderer::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_UniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        m_UniformBuffers[i] = VmaBuffer(m_DeviceContext->getAllocator(), bufferInfo, allocInfo);

        VmaAllocationInfo allocationInfo;
        vmaGetAllocationInfo(m_DeviceContext->getAllocator(), m_UniformBuffers[i].getAllocation(), &allocationInfo);
        m_UniformBuffersMapped[i] = allocationInfo.pMappedData;
    }
}

void VulkanRenderer::createLightUbo()
{
    VkDeviceSize bufferSize = sizeof(LightUbo);
    m_LightUbos.resize(MAX_FRAMES_IN_FLIGHT);
    m_LightUbosMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        m_LightUbos[i] = VmaBuffer(m_DeviceContext->getAllocator(), bufferInfo, allocInfo);

        VmaAllocationInfo allocationInfo;
        vmaGetAllocationInfo(m_DeviceContext->getAllocator(), m_LightUbos[i].getAllocation(), &allocationInfo);
        m_LightUbosMapped[i] = allocationInfo.pMappedData;
    }
}

VulkanHandle<VkImageView, ImageViewDeleter> VulkanRenderer::createTexture(const char *path, VmaImage &outImage)
{
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels)
    {
        throw std::runtime_error("failed to load texture image: " + std::string(path));
    }
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VmaBuffer stagingBuffer;
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        stagingBuffer = VmaBuffer(m_DeviceContext->getAllocator(), bufferInfo, allocInfo);
        void *data;
        vmaMapMemory(m_DeviceContext->getAllocator(), stagingBuffer.getAllocation(), &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vmaUnmapMemory(m_DeviceContext->getAllocator(), stagingBuffer.getAllocation());
    }

    stbi_image_free(pixels);

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {(uint32_t)texWidth, (uint32_t)texHeight, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    outImage = VmaImage(m_DeviceContext->getAllocator(), imageInfo, allocInfo);

    VkCommandBuffer cmd = UploadHelpers::beginSingleTimeCommands(*m_DeviceContext, m_CommandManager->getCommandPool());

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;

    UploadHelpers::transitionImageLayout(
        cmd, outImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        range, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {(uint32_t)texWidth, (uint32_t)texHeight, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer.get(), outImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    UploadHelpers::transitionImageLayout(
        cmd, outImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        range, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    UploadHelpers::endSingleTimeCommands(*m_DeviceContext, m_CommandManager->getCommandPool(), cmd);

    return TextureManager::createImageView(m_DeviceContext->getDevice(), outImage.get(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanRenderer::createSkyResources()
{
    m_SunTextureView = createTexture("textures/sun.png", m_SunTexture);
    m_MoonTextureView = createTexture("textures/moon.png", m_MoonTexture);

    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}};
    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
    m_SkySphereIndexCount = static_cast<uint32_t>(indices.size());

    m_SkySphereVertexBuffer = UploadHelpers::createDeviceLocalBufferFromData(
        *m_DeviceContext, m_CommandManager->getCommandPool(),
        vertices.data(), sizeof(Vertex) * vertices.size(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_SkySphereIndexBuffer = UploadHelpers::createDeviceLocalBufferFromData(
        *m_DeviceContext, m_CommandManager->getCommandPool(),
        indices.data(), sizeof(uint32_t) * indices.size(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void VulkanRenderer::createDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 4);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(m_DeviceContext->getDevice(), &poolInfo, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool!");

    m_DescriptorPool = VulkanHandle<VkDescriptorPool, DescriptorPoolDeleter>(pool, {m_DeviceContext->getDevice()});
}

void VulkanRenderer::createDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorLayout->getDescriptorSetLayout());
    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_DescriptorPool.get();
    alloc.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    alloc.pSetLayouts = layouts.data();
    m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_DeviceContext->getDevice(), &alloc, m_DescriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate descriptor sets");
}

void VulkanRenderer::updateDescriptorSets()
{
    VkDescriptorBufferInfo mainUboInfo{m_UniformBuffers[m_CurrentFrame].get(), 0, sizeof(UniformBufferObject)};
    VkDescriptorImageInfo atlasInfo{m_TextureManager->getTextureSampler(), m_TextureManager->getTextureImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo sunInfo{m_TextureManager->getTextureSampler(), m_SunTextureView.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo lightUboInfo{m_LightUbos[m_CurrentFrame].get(), 0, sizeof(LightUbo)};
    VkDescriptorImageInfo moonInfo{m_TextureManager->getTextureSampler(), m_MoonTextureView.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkDescriptorImageInfo shadowImageInfo{};
    shadowImageInfo.sampler = m_TextureManager->getTextureSampler();
    if (m_rtShadowImageView.get() != VK_NULL_HANDLE)
    {
        shadowImageInfo.imageView = m_rtShadowImageView.get();
        shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    else
    {
        shadowImageInfo.imageView = m_TextureManager->getTextureImageView();
        shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    std::array<VkWriteDescriptorSet, 6> writes{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_DescriptorSets[m_CurrentFrame];
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &mainUboInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_DescriptorSets[m_CurrentFrame];
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &atlasInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_DescriptorSets[m_CurrentFrame];
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &sunInfo;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = m_DescriptorSets[m_CurrentFrame];
    writes[3].dstBinding = 3;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[3].descriptorCount = 1;
    writes[3].pBufferInfo = &lightUboInfo;

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = m_DescriptorSets[m_CurrentFrame];
    writes[4].dstBinding = 4;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].descriptorCount = 1;
    writes[4].pImageInfo = &moonInfo;

    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = m_DescriptorSets[m_CurrentFrame];
    writes[5].dstBinding = 5;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[5].descriptorCount = 1;
    writes[5].pImageInfo = &shadowImageInfo;

    vkUpdateDescriptorSets(m_DeviceContext->getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRenderer::createCrosshairVertexBuffer()
{
    const float halfLenPx = 10.0f;
    float w = float(m_SwapChainContext->getSwapChainExtent().width);
    float h = float(m_SwapChainContext->getSwapChainExtent().height);
    float ndcHalfX = halfLenPx * 2.0f / w;
    float ndcHalfY = halfLenPx * 2.0f / h;
    std::array<glm::vec2, 4> verts = {{{-ndcHalfX, 0.0f}, {+ndcHalfX, 0.0f}, {0.0f, -ndcHalfY}, {0.0f, +ndcHalfY}}};
    VkDeviceSize size = sizeof(verts);

    VmaBuffer stagingBuffer;
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        stagingBuffer = VmaBuffer(m_DeviceContext->getAllocator(), bufferInfo, allocInfo);
        void *data;
        vmaMapMemory(m_DeviceContext->getAllocator(), stagingBuffer.getAllocation(), &data);
        memcpy(data, verts.data(), size);
        vmaUnmapMemory(m_DeviceContext->getAllocator(), stagingBuffer.getAllocation());
    }

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    m_CrosshairVertexBuffer = VmaBuffer(m_DeviceContext->getAllocator(), bufferInfo, allocInfo);

    UploadHelpers::copyBuffer(*m_DeviceContext, m_CommandManager->getCommandPool(), stagingBuffer.get(), m_CrosshairVertexBuffer.get(), size);
}

void VulkanRenderer::createDebugCubeMesh()
{
    std::vector<glm::vec3> vertices = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
    std::vector<uint32_t> indices = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7};
    m_DebugCubeIndexCount = static_cast<uint32_t>(indices.size());

    VkDeviceSize vertexSize = sizeof(vertices[0]) * vertices.size();
    VkDeviceSize indexSize = sizeof(indices[0]) * indices.size();

    m_DebugCubeVertexBuffer = UploadHelpers::createDeviceLocalBufferFromData(
        *m_DeviceContext, m_CommandManager->getCommandPool(),
        vertices.data(), vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_DebugCubeIndexBuffer = UploadHelpers::createDeviceLocalBufferFromData(
        *m_DeviceContext, m_CommandManager->getCommandPool(),
        indices.data(), indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void VulkanRenderer::generateSphereMesh(float radius, int sectors, int stacks,
                                        std::vector<Vertex> &outVertices,
                                        std::vector<uint32_t> &outIndices)
{
    outVertices.clear();
    outIndices.clear();

    for (int i = 0; i <= stacks; ++i)
    {
        float stackAngle = glm::pi<float>() / 2.0f - (float)i * (glm::pi<float>() / stacks);
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j)
        {
            float sectorAngle = (float)j * (2.0f * glm::pi<float>() / sectors);

            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            float u = (float)j / sectors;
            float v = (float)i / stacks;

            outVertices.push_back({{x, y, z}, {1.0f, 1.0f, 1.0f}, {u, v}});
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < sectors; ++j)
        {
            int first = (i * (sectors + 1)) + j;
            int second = first + sectors + 1;

            outIndices.push_back(first);
            outIndices.push_back(second);
            outIndices.push_back(first + 1);

            outIndices.push_back(second);
            outIndices.push_back(second + 1);
            outIndices.push_back(first + 1);
        }
    }
}