#include "Chunk.h"
#include "VulkanRenderer.h"
#include <FastNoiseLite.h>
#include "BlockAtlas.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include "Block.h"
#include "renderer/resources/UploadHelpers.h"

Chunk::Chunk(glm::ivec3 pos) : m_Pos(pos)
{
    m_Blocks.resize(WIDTH * HEIGHT * DEPTH, {BlockId::AIR});
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

void Chunk::setBlock(int x, int y, int z, Block block)
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return;
    m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x] = block;
}

Block Chunk::getBlock(int x, int y, int z) const
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return {BlockId::AIR};
    return m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x];
}

#include <array>

std::vector<Block> downsample(const std::vector<Block> &original, int factor)
{
    const int newWidth = Chunk::WIDTH / factor;
    const int newHeight = Chunk::HEIGHT / factor;
    const int newDepth = Chunk::DEPTH / factor;
    std::vector<Block> downsampled(newWidth * newHeight * newDepth, {BlockId::AIR});

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

                            Block currentBlock = original[originalY * Chunk::WIDTH * Chunk::DEPTH + originalZ * Chunk::WIDTH + originalX];

                            if (currentBlock.id != BlockId::AIR)
                            {

                                counts[static_cast<uint8_t>(currentBlock.id)]++;
                            }
                        }
                    }
                }

                BlockId mostCommonBlock = BlockId::AIR;
                int maxCount = 0;

                for (int i = 1; i < 256; ++i)
                {
                    if (counts[i] > maxCount)
                    {
                        maxCount = counts[i];
                        mostCommonBlock = static_cast<BlockId>(i);
                    }
                }

                downsampled[(y * newDepth + z) * newWidth + x] = {mostCommonBlock};
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
                m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x] = {BlockId::STONE};
        }
    m_State.store(State::TERRAIN_READY, std::memory_order_release);
}

#include <cstdio>

void Chunk::buildMeshGreedy(int lodLevel, std::vector<Vertex> &outVertices, std::vector<uint32_t> &outIndices)
{
    printf("Chunk::buildMeshGreedy called for LOD %d\n", lodLevel);
    const int factor = 1 << lodLevel;

    const int w = WIDTH / factor;
    const int h = HEIGHT / factor;
    const int d = DEPTH / factor;

    std::vector<Block> voxels;
    if (lodLevel == 0)
    {
        voxels = m_Blocks;
    }
    else
    {
        voxels = downsample(m_Blocks, factor);
    }

    auto getVoxel = [&](int x, int y, int z) -> Block
    {
        if (x < 0 || x >= w || y < 0 || y >= h || z < 0 || z >= d)
            return {BlockId::AIR};
        return voxels[y * d * w + z * w + x];
    };

    outVertices.clear();
    outIndices.clear();

    auto &db = BlockDatabase::get();

    for (int dim = 0; dim < 3; ++dim)
    {
        int u = (dim + 1) % 3, v = (dim + 2) % 3;
        int dsz[] = {w, h, d};

        for (int slice = 0; slice <= dsz[dim]; ++slice)
        {
            std::vector<int8_t> mask(dsz[u] * dsz[v], 0);

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

                    Block blockA = (slice < dsz[dim]) ? getVoxel(a.x, a.y, a.z) : Block{BlockId::AIR};
                    Block blockB = (slice > 0) ? getVoxel(b.x, b.y, b.z) : Block{BlockId::AIR};

                    const auto &dataA = db.get_block_data(blockA.id);
                    const auto &dataB = db.get_block_data(blockB.id);

                    if (dataA.is_solid != dataB.is_solid)
                    {
                        if (dataA.is_solid)
                            mask[j * dsz[u] + i] = static_cast<int8_t>(blockA.id);
                        else
                            mask[j * dsz[u] + i] = -static_cast<int8_t>(blockB.id);
                    }
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

                    bool back_face = m < 0;
                    BlockId id = static_cast<BlockId>(back_face ? -m : m);

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
                    const auto &data = db.get_block_data(id);

                    int texture_index;
                    if (dim == 0)
                    {
                        texture_index = back_face ? data.texture_indices[4] : data.texture_indices[5];
                    }
                    else if (dim == 1)
                    {
                        texture_index = back_face ? data.texture_indices[0] : data.texture_indices[1];
                    }
                    else
                    {
                        texture_index = back_face ? data.texture_indices[3] : data.texture_indices[2];
                    }

                    uint32_t idVal = texture_index;
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

                    if (back_face)
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
    std::scoped_lock lock(m_PendingMutex);
    for (auto it = m_PendingUploads.begin(); it != m_PendingUploads.end();)
    {
        if (it->second.fence == VK_NULL_HANDLE)
        {
            it = m_PendingUploads.erase(it);
            continue;
        }
        if (vkGetFenceStatus(renderer.getDevice(), it->second.fence) == VK_SUCCESS)
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
    UploadJob job;
    {
        std::scoped_lock lock(m_PendingMutex);
        if (!m_PendingUploads.count(lodLevel))
            return false;
        job = std::move(m_PendingUploads.at(lodLevel));
        m_PendingUploads.erase(lodLevel);
    }
    if (job.stagingVB == VK_NULL_HANDLE)
    {
        std::scoped_lock lock(m_MeshesMutex);
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
    {
        std::scoped_lock lock(m_MeshesMutex);
        m_Meshes[lodLevel] = std::move(mesh);
    }
    {
        std::scoped_lock lock(m_PendingMutex);
        m_PendingUploads[lodLevel] = std::move(job);
    }
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
    {
        std::scoped_lock lock(m_PendingMutex);
        m_PendingUploads[lodLevel] = std::move(job);
    }
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
    std::scoped_lock lock(m_MeshesMutex);
    return m_Meshes.count(lodLevel);
}

const ChunkMesh *Chunk::getMesh(int lodLevel) const
{
    std::scoped_lock lock(m_MeshesMutex);
    auto it = m_Meshes.find(lodLevel);
    return it == m_Meshes.end() ? nullptr : &it->second;
}

int Chunk::getBestAvailableLOD(int requiredLod) const
{
    std::scoped_lock lock(m_MeshesMutex);
    for (int lod = requiredLod; lod >= 0; --lod)
        if (m_Meshes.count(lod))
            return lod;
    return m_Meshes.empty() ? -1 : m_Meshes.rbegin()->first;
}
