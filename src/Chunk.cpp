#include "Chunk.h"
#include "FastNoiseLite.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

Chunk::Chunk(glm::ivec3 pos) : m_Pos(pos)
{
    m_Blocks.resize(WIDTH * HEIGHT * DEPTH, 0);
    m_ModelMatrix = glm::translate(glm::mat4(1.f), glm::vec3(pos.x * WIDTH, pos.y * HEIGHT, pos.z * DEPTH));
    m_State.store(State::INITIAL, std::memory_order_release);
}

Chunk::~Chunk() {}

BlockID Chunk::getBlock(int x, int y, int z) const
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return 0;
    return m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x];
}

void Chunk::generateTerrain(FastNoiseLite &noise)
{
    for (int x = 0; x < WIDTH; ++x)
        for (int z = 0; z < DEPTH; ++z)
        {
            float gx = float(m_Pos.x * WIDTH + x);
            float gz = float(m_Pos.z * DEPTH + z);
            float h = noise.GetNoise(gx, gz);
            int ground = 64 + int(h * 30.f);
            for (int y = 0; y < ground && y < HEIGHT; ++y)
                m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x] = 1;
        }
    m_State.store(State::TERRAIN_READY, std::memory_order_release);
}

void Chunk::buildMeshCpu()
{
    if (m_State.load() != State::TERRAIN_READY)
        return;
    m_Vertices.clear();
    m_Indices.clear();
    m_Vertices.reserve(16384);
    m_Indices.reserve(24576);
    auto addQuad = [&](const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2, const glm::vec3 &v3)
    {
        uint32_t start = static_cast<uint32_t>(m_Vertices.size());
        m_Vertices.push_back({v0, {1, 1, 1}, {0, 0}});
        m_Vertices.push_back({v1, {1, 1, 1}, {1, 0}});
        m_Vertices.push_back({v2, {1, 1, 1}, {1, 1}});
        m_Vertices.push_back({v3, {1, 1, 1}, {0, 1}});
        m_Indices.push_back(start + 0);
        m_Indices.push_back(start + 1);
        m_Indices.push_back(start + 2);
        m_Indices.push_back(start + 0);
        m_Indices.push_back(start + 2);
        m_Indices.push_back(start + 3);
    };
    const glm::ivec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int z = 0; z < DEPTH; ++z)
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x)
            {
                if (getBlock(x, y, z) == 0)
                    continue;
                glm::vec3 p{x, y, z};
                for (int d = 0; d < 6; ++d)
                {
                    glm::ivec3 n{x + dirs[d].x, y + dirs[d].y, z + dirs[d].z};
                    if (getBlock(n.x, n.y, n.z) != 0)
                        continue;
                    switch (d)
                    {
                    case 0:
                        addQuad(p + glm::vec3(1, 0, 0), p + glm::vec3(1, 0, 1), p + glm::vec3(1, 1, 1), p + glm::vec3(1, 1, 0));
                        break;
                    case 1:
                        addQuad(p + glm::vec3(0, 0, 1), p + glm::vec3(0, 0, 0), p + glm::vec3(0, 1, 0), p + glm::vec3(0, 1, 1));
                        break;
                    case 2:
                        addQuad(p + glm::vec3(0, 1, 1), p + glm::vec3(1, 1, 1), p + glm::vec3(1, 1, 0), p + glm::vec3(0, 1, 0));
                        break;
                    case 3:
                        addQuad(p + glm::vec3(0, 0, 0), p + glm::vec3(1, 0, 0), p + glm::vec3(1, 0, 1), p + glm::vec3(0, 0, 1));
                        break;
                    case 4:
                        addQuad(p + glm::vec3(0, 0, 1), p + glm::vec3(1, 0, 1), p + glm::vec3(1, 1, 1), p + glm::vec3(0, 1, 1));
                        break;
                    case 5:
                        addQuad(p + glm::vec3(1, 0, 0), p + glm::vec3(0, 0, 0), p + glm::vec3(0, 1, 0), p + glm::vec3(1, 1, 0));
                        break;
                    }
                }
            }
    m_State.store(State::CPU_MESH_READY, std::memory_order_release);
}
void Chunk::markReady(VulkanRenderer &renderer)
{

    if (m_State.load(std::memory_order_acquire) != State::GPU_PENDING)
    {
        return;
    }

    if (m_Upload.fence == VK_NULL_HANDLE)
    {
        m_State.store(State::GPU_READY, std::memory_order_release);
        return;
    }

    VkResult st = vkGetFenceStatus(renderer.getDevice(), m_Upload.fence);

    if (st == VK_NOT_READY)
    {
        return;
    }

    if (st == VK_SUCCESS)
    {

        vkDestroyFence(renderer.getDevice(), m_Upload.fence, nullptr);

        vmaDestroyBuffer(renderer.m_Allocator, m_Upload.stagingVB, m_Upload.stagingVbAlloc);
        vmaDestroyBuffer(renderer.m_Allocator, m_Upload.stagingIB, m_Upload.stagingIbAlloc);

        m_Upload = {};

        m_State.store(State::GPU_READY, std::memory_order_release);
        return;
    }

    throw std::runtime_error("vkGetFenceStatus hat einen Fehler zurückgegeben, GPU-Upload fehlgeschlagen!");
}

bool Chunk::uploadMesh(VulkanRenderer &renderer)
{
    if (m_State.load(std::memory_order_acquire) != State::CPU_MESH_READY)
        return false;
    if (m_VertexBuffer != VK_NULL_HANDLE)
    {
        cleanup(renderer);
    }
    if (m_Vertices.empty() || m_Indices.empty())
    {
        m_IndexCount = 0;
        m_State.store(State::GPU_READY, std::memory_order_release);
        return true;
    }
    renderer.createChunkMeshBuffers(m_Vertices, m_Indices, m_Upload, m_VertexBuffer, m_VertexBufferAllocation, m_IndexBuffer, m_IndexBufferAllocation);
    m_IndexCount = static_cast<uint32_t>(m_Indices.size());
    m_Vertices.clear();
    m_Vertices.shrink_to_fit();
    m_Indices.clear();
    m_Indices.shrink_to_fit();
    if (m_Upload.fence == VK_NULL_HANDLE)
        m_State.store(State::GPU_READY, std::memory_order_release);
    else
        m_State.store(State::GPU_PENDING, std::memory_order_release);
    return true;
}

void Chunk::generateMesh(VulkanRenderer &renderer)
{
    buildMeshCpu();
    uploadMesh(renderer);
}

void Chunk::cleanup(VulkanRenderer &renderer)
{
    if (m_State.load() == State::GPU_PENDING && m_Upload.fence != VK_NULL_HANDLE)
    {
        vkWaitForFences(renderer.getDevice(), 1, &m_Upload.fence, VK_TRUE, UINT64_MAX);
        markReady(renderer);
    }

    if (m_VertexBuffer != VK_NULL_HANDLE)
    {
        renderer.enqueueDestroy(m_VertexBuffer, m_VertexBufferAllocation);
    }
    if (m_IndexBuffer != VK_NULL_HANDLE)
    {
        renderer.enqueueDestroy(m_IndexBuffer, m_IndexBufferAllocation);
    }

    m_VertexBuffer = VK_NULL_HANDLE;
    m_IndexBuffer = VK_NULL_HANDLE;
    m_VertexBufferAllocation = VK_NULL_HANDLE;
    m_IndexBufferAllocation = VK_NULL_HANDLE;
    m_IndexCount = 0;
    m_State.store(State::INITIAL);
}