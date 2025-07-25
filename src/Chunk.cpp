#include "Chunk.h"
#include "VulkanRenderer.h"
#include "FastNoiseLite.h"
#include "BlockAtlas.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include "BlockIds.h"

Chunk::Chunk(glm::ivec3 pos) : m_Pos(pos)
{

    m_Blocks.resize(WIDTH * HEIGHT * DEPTH, BlockID::AIR);
    m_ModelMatrix = glm::translate(glm::mat4(1.f),
                                   glm::vec3(pos.x * WIDTH, pos.y * HEIGHT, pos.z * DEPTH));
    m_State.store(State::INITIAL, std::memory_order_release);
}

Chunk::~Chunk() {}

AABB Chunk::getAABB() const
{
    glm::vec3 min = glm::vec3(m_Pos.x * WIDTH, m_Pos.y * HEIGHT, m_Pos.z * DEPTH);
    glm::vec3 max = min + glm::vec3(WIDTH, HEIGHT, DEPTH);
    return {min, max};
}

void Chunk::setBlock(int x, int y, int z, BlockID id)
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return;

    m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x] = id;
}

BlockID Chunk::getBlock(int x, int y, int z) const
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return BlockID::AIR;
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
                m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x] = BlockID::STONE;
        }
    m_State.store(State::TERRAIN_READY, std::memory_order_release);
}

void Chunk::buildMeshGreedy()
{
    if (m_State.load() != State::TERRAIN_READY)
        return;

    m_Vertices.clear();
    m_Indices.clear();

    const int dsz[3] = {WIDTH, HEIGHT, DEPTH};
    std::vector<int8_t> mask(WIDTH * HEIGHT);

    constexpr float TILE = 1.0f / 16.0f;

    auto push = [&](glm::vec3 p0, glm::vec3 du, glm::vec3 dv,
                    int w, int h, bool back, int faceDir, BlockID id)
    {
        uint32_t base = uint32_t(m_Vertices.size());

        glm::vec3 p1 = p0 + du * float(w);
        glm::vec3 p2 = p1 + dv * float(h);
        glm::vec3 p3 = p0 + dv * float(h);
        if (back)
            std::swap(p1, p3);

        auto blockUV = [&](glm::vec3 p) -> glm::vec2
        {
            switch (faceDir)
            {
            case 0:
                return {p.z, p.y};
            case 1:
                return {-p.z, p.y};
            case 2:
                return {p.x, p.z};
            case 3:
                return {p.x, -p.z};
            case 4:
                return {-p.x, p.y};
            default:
                return {p.x, p.y};
            }
        };

        const uint32_t idVal = static_cast<uint32_t>(id);
        glm::vec2 tileOrigin{(idVal % 16) * TILE,
                             (idVal / 16) * TILE};

        auto makeV = [&](glm::vec3 pos) -> Vertex
        {
            glm::vec2 uv = blockUV(pos);
            return {pos,
                    {tileOrigin.x, tileOrigin.y, 0.0f},
                    uv};
        };

        m_Vertices.push_back(makeV(p0));
        m_Vertices.push_back(makeV(p1));
        m_Vertices.push_back(makeV(p2));
        m_Vertices.push_back(makeV(p3));

        m_Indices.insert(m_Indices.end(),
                         {base, base + 1, base + 2,
                          base, base + 2, base + 3});
    };

    for (int d = 0; d < 3; ++d)
    {
        int U = (d + 1) % 3, V = (d + 2) % 3;
        glm::vec3 du(0), dv(0);
        du[U] = 1;
        dv[V] = 1;

        mask.assign(dsz[U] * dsz[V], 0);

        for (int slice = 0; slice <= dsz[d]; ++slice)
        {

            for (int v = 0; v < dsz[V]; ++v)
                for (int u = 0; u < dsz[U]; ++u)
                {
                    glm::ivec3 a{0}, b{0};
                    a[d] = slice;
                    b[d] = slice - 1;
                    a[U] = b[U] = u;
                    a[V] = b[V] = v;

                    BlockID idA = (slice < dsz[d]) ? getBlock(a.x, a.y, a.z)
                                                   : BlockID::AIR;
                    BlockID idB = (slice > 0) ? getBlock(b.x, b.y, b.z)
                                              : BlockID::AIR;

                    int8_t entry = 0;
                    if (idA != BlockID::AIR && idB == BlockID::AIR)
                        entry = static_cast<int8_t>(idA);
                    else if (idA == BlockID::AIR && idB != BlockID::AIR)
                        entry = -static_cast<int8_t>(idB);

                    mask[v * dsz[U] + u] = entry;
                }

            for (int v = 0; v < dsz[V]; ++v)
                for (int u = 0; u < dsz[U];)
                {
                    int8_t m = mask[v * dsz[U] + u];
                    if (!m)
                    {
                        ++u;
                        continue;
                    }

                    bool back = m < 0;
                    BlockID id = static_cast<BlockID>(back ? -m : m);

                    int w = 1;
                    while (u + w < dsz[U] &&
                           mask[v * dsz[U] + u + w] == m)
                        ++w;

                    int h = 1;
                    bool stop = false;
                    while (v + h < dsz[V] && !stop)
                    {
                        for (int k = 0; k < w; ++k)
                            if (mask[(v + h) * dsz[U] + u + k] != m)
                            {
                                stop = true;
                                break;
                            }
                        if (!stop)
                            ++h;
                    }

                    for (int y = 0; y < h; ++y)
                        for (int x = 0; x < w; ++x)
                            mask[(v + y) * dsz[U] + u + x] = 0;

                    glm::vec3 p(0);
                    p[d] = slice;
                    p[U] = u;
                    p[V] = v;
                    int faceDir = d * 2 + (back ? 1 : 0);

                    push(p, du, dv, w, h, back, faceDir, id);
                    u += w;
                }
        }
    }

    m_State.store(State::CPU_MESH_READY, std::memory_order_release);
}

void Chunk::buildMeshCpu() { buildMeshGreedy(); }

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

        vmaDestroyBuffer(renderer.getAllocator(), m_Upload.stagingVB, m_Upload.stagingVbAlloc);
        vmaDestroyBuffer(renderer.getAllocator(), m_Upload.stagingIB, m_Upload.stagingIbAlloc);

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