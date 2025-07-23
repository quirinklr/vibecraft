#include "Chunk.h"
#include "FastNoiseLite.h"
#include "VulkanRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

Chunk::Chunk(glm::ivec3 pos) : m_Pos(pos)
{
    m_Blocks.resize(WIDTH * HEIGHT * DEPTH, 0);
    m_ModelMatrix = glm::translate(glm::mat4(1.f),
                                   glm::vec3(pos.x * WIDTH,
                                             pos.y * HEIGHT,
                                             pos.z * DEPTH));
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

    constexpr size_t MAX_FACES_PER_CHUNK = WIDTH * DEPTH * 128 * 6;
    m_Vertices.reserve(MAX_FACES_PER_CHUNK * 4);
    m_Indices.reserve(MAX_FACES_PER_CHUNK * 6);

    auto addQuad = [&](const glm::vec3 &v0,
                       const glm::vec3 &v1,
                       const glm::vec3 &v2,
                       const glm::vec3 &v3)
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

    const glm::ivec3 dirs[6] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    for (int z = 0; z < DEPTH; ++z)
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x)
            {
                if (getBlock(x, y, z) == 0)
                    continue;

                glm::vec3 p{x, y, z};

                for (int d = 0; d < 6; ++d)
                {
                    glm::ivec3 n{x + dirs[d].x,
                                 y + dirs[d].y,
                                 z + dirs[d].z};

                    if (getBlock(n.x, n.y, n.z) != 0)
                        continue;

                    switch (d)
                    {
                    case 0:
                        addQuad(p + glm::vec3(1, 0, 0),
                                p + glm::vec3(1, 0, 1),
                                p + glm::vec3(1, 1, 1),
                                p + glm::vec3(1, 1, 0));
                        break;
                    case 1:
                        addQuad(p + glm::vec3(0, 0, 1),
                                p + glm::vec3(0, 0, 0),
                                p + glm::vec3(0, 1, 0),
                                p + glm::vec3(0, 1, 1));
                        break;
                    case 2:
                        addQuad(p + glm::vec3(0, 1, 1),
                                p + glm::vec3(1, 1, 1),
                                p + glm::vec3(1, 1, 0),
                                p + glm::vec3(0, 1, 0));
                        break;
                    case 3:
                        addQuad(p + glm::vec3(0, 0, 0),
                                p + glm::vec3(1, 0, 0),
                                p + glm::vec3(1, 0, 1),
                                p + glm::vec3(0, 0, 1));
                        break;
                    case 4:
                        addQuad(p + glm::vec3(0, 0, 1),
                                p + glm::vec3(1, 0, 1),
                                p + glm::vec3(1, 1, 1),
                                p + glm::vec3(0, 1, 1));
                        break;
                    case 5:
                        addQuad(p + glm::vec3(1, 0, 0),
                                p + glm::vec3(0, 0, 0),
                                p + glm::vec3(0, 1, 0),
                                p + glm::vec3(1, 1, 0));
                        break;
                    }
                }
            }

    m_State.store(State::CPU_MESH_READY, std::memory_order_release);
}

void Chunk::markReady(VkDevice dev)
{
    if (m_State.load(std::memory_order_acquire) != State::GPU_PENDING)
        return;

    if (m_Upload.fence == VK_NULL_HANDLE)
    {
        m_State.store(State::GPU_READY, std::memory_order_release);
        return;
    }

    VkResult st = vkGetFenceStatus(dev, m_Upload.fence);
    if (st == VK_NOT_READY)
        return;

    if (st == VK_SUCCESS)
    {
        vkDestroyFence(dev, m_Upload.fence, nullptr);
        vkDestroyBuffer(dev, m_Upload.stagingVB, nullptr);
        vkFreeMemory(dev, m_Upload.stagingVBMem, nullptr);
        vkDestroyBuffer(dev, m_Upload.stagingIB, nullptr);
        vkFreeMemory(dev, m_Upload.stagingIBMem, nullptr);
        m_Upload = {};
        m_State.store(State::GPU_READY, std::memory_order_release);
        return;
    }

    throw std::runtime_error("vkGetFenceStatus failed");
}

bool Chunk::uploadMesh(VulkanRenderer &r)
{
    if (m_State.load(std::memory_order_acquire) != State::CPU_MESH_READY)
        return false;

    if (m_Vertices.empty() || m_Indices.empty())
    {
        m_State.store(State::GPU_READY, std::memory_order_release);
        return true;
    }

    r.createChunkMeshBuffers(m_Vertices, m_Indices,
                             m_Upload,
                             m_VertexBuffer, m_VertexBufferMemory,
                             m_IndexBuffer, m_IndexBufferMemory);

    m_IndexCount = static_cast<uint32_t>(m_Indices.size());
    m_Vertices.clear();
    m_Indices.clear();

    if (m_Upload.fence == VK_NULL_HANDLE)
        m_State.store(State::GPU_READY, std::memory_order_release);
    else
        m_State.store(State::GPU_PENDING, std::memory_order_release);

    return true;
}

void Chunk::generateMesh(VulkanRenderer &renderer)
{

    if (m_VertexBuffer != VK_NULL_HANDLE)
        cleanup(renderer);

    buildMeshCpu();

    uploadMesh(renderer);
}

void Chunk::cleanup(VulkanRenderer &r)
{

    if (m_Upload.fence)
        vkDestroyFence(r.getDevice(), m_Upload.fence, nullptr);
    if (m_Upload.stagingVB)
        vkDestroyBuffer(r.getDevice(), m_Upload.stagingVB, nullptr);
    if (m_Upload.stagingVBMem)
        vkFreeMemory(r.getDevice(), m_Upload.stagingVBMem, nullptr);
    if (m_Upload.stagingIB)
        vkDestroyBuffer(r.getDevice(), m_Upload.stagingIB, nullptr);
    if (m_Upload.stagingIBMem)
        vkFreeMemory(r.getDevice(), m_Upload.stagingIBMem, nullptr);
    m_Upload = {};

    r.enqueueDestroy(m_VertexBuffer, m_VertexBufferMemory);
    r.enqueueDestroy(m_IndexBuffer, m_IndexBufferMemory);

    m_VertexBuffer = m_IndexBuffer = VK_NULL_HANDLE;
    m_VertexBufferMemory = m_IndexBufferMemory = VK_NULL_HANDLE;
}
