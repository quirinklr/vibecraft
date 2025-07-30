#include "VulkanRenderer.h"
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include "math/Ivec3Less.h"
#include "renderer/UniformBufferObject.h"
#include <Globals.h>
#include "generation/TerrainGenerator.h"

VulkanRenderer::VulkanRenderer(Window &window, const Settings &settings)
    : m_Window(window), m_Settings(settings)
{
    m_InstanceContext = std::make_unique<InstanceContext>(m_Window);
    m_DeviceContext = std::make_unique<DeviceContext>(*m_InstanceContext);
    VkDeviceSize arenaSize = 64ull * 1024 * 1024;
    m_StagingArena = std::make_unique<RingStagingArena>(*m_DeviceContext, arenaSize);
    if (m_DeviceContext->hasTransferQueue())
    {
        VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        ci.queueFamilyIndex =
            m_DeviceContext->findQueueFamilies(m_DeviceContext->getPhysicalDevice()).transferFamily.value();
        vkCreateCommandPool(m_DeviceContext->getDevice(), &ci, nullptr, &m_TransferCommandPool);
    }

    m_SwapChainContext = std::make_unique<SwapChainContext>(m_Window, m_Settings, *m_InstanceContext, *m_DeviceContext);
    m_DescriptorLayout = std::make_unique<DescriptorLayout>(*m_DeviceContext);
    m_PipelineCache = std::make_unique<PipelineCache>(*m_DeviceContext, *m_SwapChainContext, *m_DescriptorLayout);
    m_CommandManager = std::make_unique<CommandManager>(*m_DeviceContext, *m_SwapChainContext, *m_PipelineCache);
    m_SyncPrimitives = std::make_unique<SyncPrimitives>(*m_DeviceContext, *m_SwapChainContext);
    m_TextureManager = std::make_unique<TextureManager>(*m_DeviceContext, m_CommandManager->getCommandPool());

    m_TextureManager->createTextureImage("textures/blocks_atlas.png");
    m_TextureManager->createTextureImageView();
    m_TextureManager->createTextureSampler();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCrosshairVertexBuffer();
    createDebugCubeMesh();
}

VulkanRenderer::~VulkanRenderer()
{
    vkDeviceWaitIdle(m_DeviceContext->getDevice());
    if (m_TransferCommandPool)
        vkDestroyCommandPool(m_DeviceContext->getDevice(), m_TransferCommandPool, nullptr);
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

void VulkanRenderer::drawFrame(Camera &camera,
                               const glm::vec3 &playerPos,
                               const std::map<glm::ivec3, std::shared_ptr<Chunk>, ivec3_less> &chunks,
                               const glm::ivec3 &playerChunkPos,
                               const Settings &settings,
                               const std::vector<AABB> &debugAABBs)
{
    vkWaitForFences(m_DeviceContext->getDevice(), 1, m_SyncPrimitives->getInFlightFencePtr(m_CurrentFrame), VK_TRUE, UINT64_MAX);

    m_BufferDestroyQueue[m_CurrentFrame].clear();
    m_ImageDestroyQueue[m_CurrentFrame].clear();

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_DeviceContext->getDevice(),
                                            m_SwapChainContext->getSwapChain(),
                                            UINT64_MAX,
                                            m_SyncPrimitives->getImageAvailableSemaphore(m_CurrentFrame),
                                            VK_NULL_HANDLE,
                                            &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        m_SwapChainContext->recreateSwapChain();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swap chain image");

    if (m_SyncPrimitives->getImageInFlight(imageIndex) != VK_NULL_HANDLE)
        vkWaitForFences(m_DeviceContext->getDevice(), 1, &m_SyncPrimitives->getImageInFlight(imageIndex), VK_TRUE, UINT64_MAX);
    m_SyncPrimitives->getImageInFlight(imageIndex) = m_SyncPrimitives->getInFlightFence(m_CurrentFrame);

    updateUniformBuffer(m_CurrentFrame, camera, playerPos);

    std::vector<std::pair<const Chunk *, int>> renderChunks;
    renderChunks.reserve(chunks.size());
    const Frustum &frustum = camera.getFrustum();

    for (auto const &[pos, chunkPtr] : chunks)
    {
        chunkPtr->markReady(*this);
        if (!frustum.intersects(chunkPtr->getAABB()))
            continue;

        float dist = glm::distance(glm::vec2(pos.x, pos.z), glm::vec2(playerChunkPos.x, playerChunkPos.z));
        int reqLod = (!settings.lodDistances.empty() && dist <= settings.lodDistances[0]) ? 0 : 1;
        int best = chunkPtr->getBestAvailableLOD(reqLod);
        if (best != -1)
            renderChunks.push_back({chunkPtr.get(), best});
    }

    vkResetFences(m_DeviceContext->getDevice(), 1, m_SyncPrimitives->getInFlightFencePtr(m_CurrentFrame));
    vkResetCommandBuffer(m_CommandManager->getCommandBuffer(m_CurrentFrame), 0);

    m_CommandManager->recordCommandBuffer(
        imageIndex, m_CurrentFrame, renderChunks, m_DescriptorSets,
        m_CrosshairVertexBuffer.get(),
        m_DebugCubeVertexBuffer.get(), m_DebugCubeIndexBuffer.get(), m_DebugCubeIndexCount,
        settings, debugAABBs);

    VkSemaphore waitSemas[]{m_SyncPrimitives->getImageAvailableSemaphore(m_CurrentFrame)};
    VkPipelineStageFlags waitStages[]{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemas[]{m_SyncPrimitives->getRenderFinishedSemaphore(m_CurrentFrame)};

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = waitSemas;
    si.pWaitDstStageMask = waitStages;
    VkCommandBuffer cb = m_CommandManager->getCommandBuffer(m_CurrentFrame);
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = signalSemas;

    {
        std::scoped_lock lk(gGraphicsQueueMutex);
        vkQueueSubmit(m_DeviceContext->getGraphicsQueue(), 1, &si, m_SyncPrimitives->getInFlightFence(m_CurrentFrame));
    }

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = signalSemas;
    VkSwapchainKHR swp[]{m_SwapChainContext->getSwapChain()};
    pi.swapchainCount = 1;
    pi.pSwapchains = swp;
    pi.pImageIndices = &imageIndex;

    {
        std::scoped_lock lk(gGraphicsQueueMutex);
        result = vkQueuePresentKHR(m_DeviceContext->getPresentQueue(), &pi);
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_Window.wasWindowResized())
    {
        m_Window.resetWindowResizedFlag();
        m_SwapChainContext->recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
        throw std::runtime_error("failed to present swap chain image");

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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

void VulkanRenderer::updateUniformBuffer(uint32_t currentImage, Camera &camera, const glm::vec3 &playerPos)
{

    const float eyeHeight = 1.8f * 0.9f;
    glm::vec3 cameraWorldPos = playerPos + glm::vec3(0.0f, eyeHeight, 0.0f);

    UniformBufferObject ubo{};
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.cameraPos = cameraWorldPos;
    ubo.skyColor = glm::vec3(0.5f, 0.7f, 1.0f);
    ubo.time = static_cast<float>(glfwGetTime());

    ubo.isUnderwater = (cameraWorldPos.y < TerrainGenerator::SEA_LEVEL) ? 1 : 0;

    memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
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

void VulkanRenderer::createDescriptorPool()
{
    VkDescriptorPoolSize sizes[2]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = 2;
    info.pPoolSizes = sizes;
    info.maxSets = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(m_DeviceContext->getDevice(), &info, nullptr, &pool) != VK_SUCCESS)
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
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_UniformBuffers[i].get();
        bufInfo.offset = 0;
        bufInfo.range = sizeof(UniformBufferObject);
        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView = m_TextureManager->getTextureImageView();
        imgInfo.sampler = m_TextureManager->getTextureSampler();
        VkWriteDescriptorSet writes[2]{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_DescriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufInfo;
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_DescriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imgInfo;
        vkUpdateDescriptorSets(m_DeviceContext->getDevice(), 2, writes, 0, nullptr);
    }
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
        VmaAllocationCreateInfo allocInfo{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
        stagingBuffer = VmaBuffer(m_DeviceContext->getAllocator(), bufferInfo, allocInfo);
        void *data;
        vmaMapMemory(m_DeviceContext->getAllocator(), stagingBuffer.getAllocation(), &data);
        memcpy(data, verts.data(), size);
        vmaUnmapMemory(m_DeviceContext->getAllocator(), stagingBuffer.getAllocation());
    }

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
    VmaAllocationCreateInfo allocInfo{0, VMA_MEMORY_USAGE_GPU_ONLY};
    m_CrosshairVertexBuffer = VmaBuffer(m_DeviceContext->getAllocator(), bufferInfo, allocInfo);

    UploadHelpers::copyBuffer(*m_DeviceContext, m_CommandManager->getCommandPool(), stagingBuffer.get(), m_CrosshairVertexBuffer.get(), size);
}
