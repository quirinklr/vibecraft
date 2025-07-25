#include "Chunk.h"
#include "VulkanRenderer.h"
#include <FastNoiseLite.h>
#include "BlockAtlas.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include "BlockIds.h"
#include "renderer/resources/UploadHelpers.h"

Chunk::Chunk(glm::ivec3 pos) : m_Pos(pos)
{
    m_Blocks.resize(WIDTH * HEIGHT * DEPTH, BlockID::AIR);
    m_ModelMatrix = glm::translate(glm::mat4(1.f), glm::vec3(pos.x * WIDTH, pos.y * HEIGHT, pos.z * DEPTH));
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

#include <array>

std::vector<BlockID> downsample(const std::vector<BlockID> &original, int factor)
{
    const int newWidth = Chunk::WIDTH / factor;
    const int newHeight = Chunk::HEIGHT / factor;
    const int newDepth = Chunk::DEPTH / factor;
    std::vector<BlockID> downsampled(newWidth * newHeight * newDepth, BlockID::AIR);

    for (int y = 0; y < newHeight; ++y)
    {
        for (int z = 0; z < newDepth; ++z)
        {
            for (int x = 0; x < newWidth; ++x)
            {

                std::array<int, 256> counts{};

                for (int oy = 0; oy < factor; ++oy)
                {
                    for (int oz = 0; oz < factor; ++oz)
                    {
                        for (int ox = 0; ox < factor; ++ox)
                        {
                            int originalX = x * factor + ox;
                            int originalY = y * factor + oy;
                            int originalZ = z * factor + oz;

                            BlockID currentBlock = original[originalY * Chunk::WIDTH * Chunk::DEPTH + originalZ * Chunk::WIDTH + originalX];

                            if (currentBlock != BlockID::AIR)
                            {

                                counts[static_cast<uint8_t>(currentBlock)]++;
                            }
                        }
                    }
                }

                BlockID mostCommonBlock = BlockID::AIR;
                int maxCount = 0;

                for (int i = 1; i < 256; ++i)
                {
                    if (counts[i] > maxCount)
                    {
                        maxCount = counts[i];
                        mostCommonBlock = static_cast<BlockID>(i);
                    }
                }

                downsampled[(y * newDepth + z) * newWidth + x] = mostCommonBlock;
            }
        }
    }
    return downsampled;
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

void Chunk::buildMeshGreedy(int lodLevel, std::vector<Vertex> &outVertices, std::vector<uint32_t> &outIndices)
{
    const int factor = 1 << lodLevel;
    const int w = WIDTH / factor;
    const int h = HEIGHT / factor;
    const int d = DEPTH / factor;

    std::vector<BlockID> voxels;
    if (lodLevel == 0)
    {
        voxels = m_Blocks;
    }
    else
    {
        voxels = downsample(m_Blocks, factor);
    }

    auto getVoxel = [&](int x, int y, int z) -> BlockID
    {
        if (x < 0 || x >= w || y < 0 || y >= h || z < 0 || z >= d)
            return BlockID::AIR;
        return voxels[y * d * w + z * w + x];
    };

    outVertices.clear();
    outIndices.clear();

    std::vector<int8_t> mask(w * h);

    for (int dim = 0; dim < 3; ++dim)
    {
        int u = (dim + 1) % 3, v = (dim + 2) % 3;
        int dsz[] = {w, h, d};

        for (int slice = 0; slice <= dsz[dim]; ++slice)
        {
            mask.assign(dsz[u] * dsz[v], 0);

            for (int j = 0; j < dsz[v]; ++j)
            {
                for (int i = 0; i < dsz[u]; ++i)
                {
                    glm::ivec3 a{0}, b{0};
                    a[dim] = slice;
                    b[dim] = slice - 1;
                    a[u] = i;
                    b[u] = i;
                    a[v] = j;
                    b[v] = j;

                    BlockID idA = (slice < dsz[dim]) ? getVoxel(a.x, a.y, a.z) : BlockID::AIR;
                    BlockID idB = (slice > 0) ? getVoxel(b.x, b.y, b.z) : BlockID::AIR;

                    int8_t entry = 0;
                    if ((idA != BlockID::AIR) != (idB != BlockID::AIR))
                    {
                        entry = (idA != BlockID::AIR) ? static_cast<int8_t>(idA) : -static_cast<int8_t>(idB);
                    }
                    mask[j * dsz[u] + i] = entry;
                }
            }

            for (int j = 0; j < dsz[v]; ++j)
            {
                for (int i = 0; i < dsz[u];)
                {
                    int8_t m = mask[j * dsz[u] + i];
                    if (!m)
                    {
                        ++i;
                        continue;
                    }

                    bool back = m < 0;
                    BlockID id = static_cast<BlockID>(back ? -m : m);

                    int quadWidth = 1;
                    while (i + quadWidth < dsz[u] && mask[j * dsz[u] + i + quadWidth] == m)
                        ++quadWidth;

                    int quadHeight = 1;
                    bool stop = false;
                    while (j + quadHeight < dsz[v] && !stop)
                    {
                        for (int k = 0; k < quadWidth; ++k)
                        {
                            if (mask[(j + quadHeight) * dsz[u] + i + k] != m)
                            {
                                stop = true;
                                break;
                            }
                        }
                        if (!stop)
                            ++quadHeight;
                    }

                    glm::vec3 p0(0);
                    p0[dim] = slice;
                    p0[u] = i;
                    p0[v] = j;

                    glm::vec3 du(0), dv(0);
                    du[u] = quadWidth;
                    dv[v] = quadHeight;

                    uint32_t base = outVertices.size();
                    const uint32_t idVal = static_cast<uint32_t>(id);
                    glm::vec3 tileOrigin{(idVal % 16) * ATLAS_INV_SIZE, (idVal / 16) * ATLAS_INV_SIZE, 0.f};

                    glm::vec3 v0 = p0 * (float)factor;
                    glm::vec3 v1 = (p0 + du) * (float)factor;
                    glm::vec3 v2 = (p0 + du + dv) * (float)factor;
                    glm::vec3 v3 = (p0 + dv) * (float)factor;

                    auto getUV = [&](const glm::vec3 &pos) -> glm::vec2
                    {
                        switch (dim)
                        {
                        case 0:
                            return {pos.z, pos.y};
                        case 1:
                            return {pos.x, pos.z};
                        case 2:
                            return {pos.x, pos.y};
                        default:
                            return {0, 0};
                        }
                    };

                    if (back)
                    {
                        outIndices.insert(outIndices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
                    }
                    else
                    {
                        outIndices.insert(outIndices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
                    }

                    outVertices.push_back({v0, tileOrigin, getUV(v0)});
                    outVertices.push_back({v1, tileOrigin, getUV(v1)});
                    outVertices.push_back({v2, tileOrigin, getUV(v2)});
                    outVertices.push_back({v3, tileOrigin, getUV(v3)});

                    for (int y = 0; y < quadHeight; ++y)
                    {
                        for (int x = 0; x < quadWidth; ++x)
                        {
                            mask[(j + y) * dsz[u] + i + x] = 0;
                        }
                    }

                    i += quadWidth;
                }
            }
        }
    }
}

void Chunk::markReady(VulkanRenderer &renderer)
{

    for (auto it = m_PendingUploads.begin(); it != m_PendingUploads.end();)
    {
        if (it->second.fence == VK_NULL_HANDLE)
        {
            it = m_PendingUploads.erase(it);
            continue;
        }

        VkResult st = vkGetFenceStatus(renderer.getDevice(), it->second.fence);
        if (st == VK_SUCCESS)
        {
            vkDestroyFence(renderer.getDevice(), it->second.fence, nullptr);
            vmaDestroyBuffer(renderer.getAllocator(), it->second.stagingVB, it->second.stagingVbAlloc);
            vmaDestroyBuffer(renderer.getAllocator(), it->second.stagingIB, it->second.stagingIbAlloc);
            it = m_PendingUploads.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool Chunk::uploadMesh(VulkanRenderer &renderer, int lodLevel)
{
    if (!m_PendingUploads.count(lodLevel))
        return false;

    UploadJob job = std::move(m_PendingUploads.at(lodLevel));
    m_PendingUploads.erase(lodLevel);

    if (job.stagingVB == VK_NULL_HANDLE)
    {
        m_Meshes[lodLevel] = {};
        m_State.store(State::GPU_READY);
        return true;
    }

    m_State.store(State::UPLOADING);

    ChunkMesh mesh;
    UploadHelpers::submitChunkMeshUpload(
        *renderer.getDeviceContext(),
        renderer.getCommandManager()->getCommandPool(),
        job,
        mesh.vertexBuffer, mesh.vertexBufferAllocation,
        mesh.indexBuffer, mesh.indexBufferAllocation);

    VmaAllocationInfo ibInfo;
    vmaGetAllocationInfo(renderer.getAllocator(), mesh.indexBufferAllocation, &ibInfo);
    mesh.indexCount = static_cast<uint32_t>(ibInfo.size / sizeof(uint32_t));

    m_Meshes[lodLevel] = std::move(mesh);

    m_PendingUploads[lodLevel] = std::move(job);

    m_State.store(State::GPU_READY);
    return true;
}

void Chunk::buildAndStageMesh(VmaAllocator allocator, int lodLevel)
{
    if (m_State.load() == State::INITIAL)
        return;

    m_State.store(State::MESHING);

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    buildMeshGreedy(lodLevel, vertices, indices);

    UploadJob job;
    UploadHelpers::stageChunkMesh(allocator, vertices, indices, job);

    m_PendingUploads[lodLevel] = std::move(job);
    m_State.store(State::STAGING_READY);
}

void Chunk::cleanup(VulkanRenderer &renderer)
{
    markReady(renderer);

    for (auto &[lod, mesh] : m_Meshes)
    {
        if (mesh.vertexBuffer != VK_NULL_HANDLE)
        {
            renderer.enqueueDestroy(mesh.vertexBuffer, mesh.vertexBufferAllocation);
        }
        if (mesh.indexBuffer != VK_NULL_HANDLE)
        {
            renderer.enqueueDestroy(mesh.indexBuffer, mesh.indexBufferAllocation);
        }
    }
    m_Meshes.clear();
    m_State.store(State::INITIAL);
}

bool Chunk::hasLOD(int lodLevel) const
{
    return m_Meshes.count(lodLevel);
}

const ChunkMesh *Chunk::getMesh(int lodLevel) const
{
    auto it = m_Meshes.find(lodLevel);
    if (it != m_Meshes.end())
    {
        return &it->second;
    }
    return nullptr;
}

int Chunk::getBestAvailableLOD(int requiredLod) const
{

    for (int lod = requiredLod; lod >= 0; --lod)
    {
        if (hasLOD(lod))
            return lod;
    }

    if (!m_Meshes.empty())
    {
        return m_Meshes.rbegin()->first;
    }
    return -1;
}