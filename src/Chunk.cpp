#include "Chunk.h"
#include "VulkanRenderer.h"
#include <FastNoiseLite.h>
#include "BlockAtlas.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include "Block.h"
#include <array>
#include "renderer/resources/UploadHelpers.h"
#include <chrono>

using hrc = std::chrono::high_resolution_clock;
using milli = std::chrono::duration<double, std::milli>;

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

    m_is_dirty.store(true, std::memory_order_release);
    m_blas_dirty.store(true, std::memory_order_release);
}

Block Chunk::getBlock(int x, int y, int z) const
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return {BlockId::AIR};
    return m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x];
}

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

void Chunk::markReady(VulkanRenderer &renderer)
{
    std::scoped_lock lock(m_PendingMutex);
    for (auto it = m_PendingUploads.begin(); it != m_PendingUploads.end();)
    {
        auto &job = it->second;
        if (job.fence != VK_NULL_HANDLE &&
            vkGetFenceStatus(renderer.getDevice(), job.fence) == VK_SUCCESS)
        {
            vkWaitForFences(renderer.getDevice(), 1, &job.fence, VK_TRUE, UINT64_MAX);
            if (job.cmdBuffer != VK_NULL_HANDLE)
                vkFreeCommandBuffers(renderer.getDevice(),
                                     renderer.getTransferCommandPool()
                                         ? renderer.getTransferCommandPool()
                                         : renderer.getCommandManager()->getCommandPool(),
                                     1, &job.cmdBuffer);
            vkDestroyFence(renderer.getDevice(), job.fence, nullptr);
            it = m_PendingUploads.erase(it);
        }
        else
            ++it;
    }
    for (auto it = m_PendingTransparentUploads.begin(); it != m_PendingTransparentUploads.end();)
    {
        auto &job = it->second;
        if (job.fence != VK_NULL_HANDLE &&
            vkGetFenceStatus(renderer.getDevice(), job.fence) == VK_SUCCESS)
        {
            vkWaitForFences(renderer.getDevice(), 1, &job.fence, VK_TRUE, UINT64_MAX);
            if (job.cmdBuffer != VK_NULL_HANDLE)
                vkFreeCommandBuffers(renderer.getDevice(),
                                     renderer.getTransferCommandPool()
                                         ? renderer.getTransferCommandPool()
                                         : renderer.getCommandManager()->getCommandPool(),
                                     1, &job.cmdBuffer);
            vkDestroyFence(renderer.getDevice(), job.fence, nullptr);
            it = m_PendingTransparentUploads.erase(it);
        }
        else
            ++it;
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
        ChunkMesh oldMesh;
        {
            std::scoped_lock lock(m_MeshesMutex);
            if (m_Meshes.count(lodLevel))
            {
                oldMesh = std::move(m_Meshes.at(lodLevel));
                m_Meshes.erase(lodLevel);
            }
        }

        if (oldMesh.vertexBuffer != VK_NULL_HANDLE)
        {
            renderer.enqueueDestroy(oldMesh.vertexBuffer, oldMesh.vertexBufferAllocation);
            oldMesh.blas.destroy(renderer.getDevice());
        }

        if (oldMesh.indexBuffer != VK_NULL_HANDLE)
            renderer.enqueueDestroy(oldMesh.indexBuffer, oldMesh.indexBufferAllocation);
        m_State.store(State::GPU_READY);
        return true;
    }

    m_State.store(State::UPLOADING);

    ChunkMesh newMesh;
    VkCommandPool pool = renderer.getTransferCommandPool() ? renderer.getTransferCommandPool() : renderer.getCommandManager()->getCommandPool();
    UploadHelpers::submitChunkMeshUpload(*renderer.getDeviceContext(), pool, job, newMesh.vertexBuffer, newMesh.vertexBufferAllocation, newMesh.indexBuffer, newMesh.indexBufferAllocation);

    newMesh.indexCount = static_cast<uint32_t>(job.stagingIbSize / sizeof(uint32_t));

    ChunkMesh oldMesh;
    {
        std::scoped_lock lock(m_MeshesMutex);
        if (m_Meshes.count(lodLevel))
        {
            oldMesh = std::move(m_Meshes.at(lodLevel));
        }
        m_Meshes[lodLevel] = std::move(newMesh);
    }

    if (oldMesh.vertexBuffer != VK_NULL_HANDLE)
    {
        renderer.enqueueDestroy(oldMesh.vertexBuffer, oldMesh.vertexBufferAllocation);
    }
    if (oldMesh.indexBuffer != VK_NULL_HANDLE)
    {
        renderer.enqueueDestroy(oldMesh.indexBuffer, oldMesh.indexBufferAllocation);
    }

    {
        std::scoped_lock lock(m_PendingMutex);
        m_PendingUploads[lodLevel] = std::move(job);
    }
    m_State.store(State::GPU_READY);
    return true;
}

bool Chunk::uploadTransparentMesh(VulkanRenderer &renderer, int lodLevel)
{
    UploadJob job;
    {
        std::scoped_lock lock(m_PendingMutex);
        if (!m_PendingTransparentUploads.count(lodLevel))
            return false;
        job = std::move(m_PendingTransparentUploads.at(lodLevel));
        m_PendingTransparentUploads.erase(lodLevel);
    }

    if (job.stagingVB == VK_NULL_HANDLE)
    {
        ChunkMesh oldMesh;
        {
            std::scoped_lock lock(m_MeshesMutex);
            if (m_TransparentMeshes.count(lodLevel))
            {
                oldMesh = std::move(m_TransparentMeshes.at(lodLevel));
                m_TransparentMeshes.erase(lodLevel);
            }
        }
        if (oldMesh.vertexBuffer != VK_NULL_HANDLE)
            renderer.enqueueDestroy(oldMesh.vertexBuffer, oldMesh.vertexBufferAllocation);
        if (oldMesh.indexBuffer != VK_NULL_HANDLE)
            renderer.enqueueDestroy(oldMesh.indexBuffer, oldMesh.indexBufferAllocation);
        m_State.store(State::GPU_READY);
        return true;
    }

    m_State.store(State::UPLOADING);

    ChunkMesh newMesh;
    VkCommandPool pool = renderer.getTransferCommandPool() ? renderer.getTransferCommandPool() : renderer.getCommandManager()->getCommandPool();
    UploadHelpers::submitChunkMeshUpload(*renderer.getDeviceContext(), pool, job, newMesh.vertexBuffer, newMesh.vertexBufferAllocation, newMesh.indexBuffer, newMesh.indexBufferAllocation);

    newMesh.indexCount = static_cast<uint32_t>(job.stagingIbSize / sizeof(uint32_t));

    ChunkMesh oldMesh;
    {
        std::scoped_lock lock(m_MeshesMutex);
        if (m_TransparentMeshes.count(lodLevel))
        {
            oldMesh = std::move(m_TransparentMeshes.at(lodLevel));
        }
        m_TransparentMeshes[lodLevel] = std::move(newMesh);
    }

    if (oldMesh.vertexBuffer != VK_NULL_HANDLE)
    {
        renderer.enqueueDestroy(oldMesh.vertexBuffer, oldMesh.vertexBufferAllocation);
    }
    if (oldMesh.indexBuffer != VK_NULL_HANDLE)
    {
        renderer.enqueueDestroy(oldMesh.indexBuffer, oldMesh.indexBufferAllocation);
    }

    {
        std::scoped_lock lock(m_PendingMutex);
        m_PendingTransparentUploads[lodLevel] = std::move(job);
    }
    m_State.store(State::GPU_READY);
    return true;
}

void Chunk::buildMeshGreedy(int lodLevel,
                            std::vector<Vertex> &outOpaqueVertices, std::vector<uint32_t> &outOpaqueIndices,
                            std::vector<Vertex> &outTransparentVertices, std::vector<uint32_t> &outTransparentIndices,
                            ChunkMeshInput &meshInput)
{
    constexpr size_t kMaxFaces = WIDTH * HEIGHT * DEPTH;
    constexpr size_t kMaxVerts = kMaxFaces * 4;
    constexpr size_t kMaxIdx = kMaxFaces * 6;

    outOpaqueVertices.clear();
    outTransparentVertices.clear();
    outOpaqueIndices.clear();
    outTransparentIndices.clear();

    if (outOpaqueVertices.capacity() < kMaxVerts)
        outOpaqueVertices.reserve(kMaxVerts);
    if (outTransparentVertices.capacity() < kMaxVerts)
        outTransparentVertices.reserve(kMaxVerts >> 4);
    if (outOpaqueIndices.capacity() < kMaxIdx)
        outOpaqueIndices.reserve(kMaxIdx);
    if (outTransparentIndices.capacity() < kMaxIdx)
        outTransparentIndices.reserve(kMaxIdx >> 4);

    struct MaskCell
    {
        int8_t block_id = 0;
        bool operator==(const MaskCell &r) const { return block_id == r.block_id; }
    };

    auto getBlockFromSource = [&](int rx, int ry, int rz) -> Block
    {
        if (ry < 0 || ry >= HEIGHT)
            return {BlockId::AIR};
        int cx = (rx < 0) ? -1 : (rx >= WIDTH ? 1 : 0);
        int cz = (rz < 0) ? -1 : (rz >= DEPTH ? 1 : 0);
        int lx = rx - cx * WIDTH;
        int lz = rz - cz * DEPTH;
        if (cx == 0 && cz == 0)
            return meshInput.selfChunk->getBlock(lx, ry, lz);
        static const int map[3][3] = {{4, 2, 5}, {0, -1, 1}, {6, 3, 7}};
        int idx = map[cz + 1][cx + 1];
        if (idx != -1 && meshInput.neighborChunks[idx])
            return meshInput.neighborChunks[idx]->getBlock(lx, ry, lz);
        return {BlockId::AIR};
    };

    for (int y = 0; y < HEIGHT; ++y)
        for (int z = 0; z < DEPTH + 2; ++z)
            for (int x = 0; x < WIDTH + 2; ++x)
                meshInput.cachedBlocks[y * (DEPTH + 2) * (WIDTH + 2) + z * (WIDTH + 2) + x] = getBlockFromSource(x - 1, y, z - 1);

    const int W = WIDTH, H = HEIGHT, D = DEPTH;
    const int PAD_W = W + 2, PAD_D = D + 2;

    static thread_local std::vector<uint8_t> solidLUT;
    auto &db = BlockDatabase::get();
    {
        const size_t cnt = db.blockCount();
        if (solidLUT.size() != cnt)
        {
            solidLUT.assign(cnt, 0);
            for (size_t id = 0; id < cnt; ++id)
                solidLUT[id] = db.get_block_data(static_cast<BlockId>(id)).is_solid;
        }
    }

    static thread_local std::vector<uint8_t> solidCache;
    const size_t scSize = static_cast<size_t>(H) * PAD_W * PAD_D;

    if (solidCache.size() < scSize)
        solidCache.resize(scSize);

    auto sidx = [&](int x, int y, int z) -> size_t
    {
        return static_cast<size_t>(y) * PAD_W * PAD_D + static_cast<size_t>(z) * PAD_W + static_cast<size_t>(x);
    };
    for (int y = 0; y < H; ++y)
        for (int z = 0; z < PAD_D; ++z)
            for (int x = 0; x < PAD_W; ++x)
            {
                const Block b = meshInput.cachedBlocks[sidx(x, y, z)];
                solidCache[sidx(x, y, z)] = solidLUT[static_cast<uint8_t>(b.id)];
            }

    auto isSolid = [&](int x, int y, int z) -> bool
    {
        int px = x + 1;
        int pz = z + 1;
        if (y < 0 || y >= H || px < 0 || px >= PAD_W || pz < 0 || pz >= PAD_D)
            return false;
        return solidCache[sidx(px, y, pz)] != 0;
    };
    auto getCache = [&](int x, int y, int z) -> Block
    {
        if (x < -1 || x > WIDTH || y < 0 || y >= HEIGHT || z < -1 || z > DEPTH)
            return {BlockId::AIR};
        return meshInput.cachedBlocks[sidx(x + 1, y, z + 1)];
    };
    auto isNeighborMissing = [&](int x, int z)
    {
        int cx = (x < 0) ? -1 : (x >= W ? 1 : 0);
        int cz = (z < 0) ? -1 : (z >= D ? 1 : 0);
        if (cx == 0 && cz == 0)
            return false;
        static const int map[3][3] = {{4, 2, 5}, {0, -1, 1}, {6, 3, 7}};
        int idx = map[cz + 1][cx + 1];
        return idx != -1 && meshInput.neighborChunks[idx] == nullptr;
    };

    static thread_local std::vector<MaskCell> mask;

    for (int dim = 0; dim < 3; ++dim)
    {
        int u = (dim + 1) % 3;
        int v = (dim + 2) % 3;
        int dsz[3] = {W, H, D};
        glm::ivec3 du_i(0), dv_i(0), dn_i(0);
        du_i[u] = 1;
        dv_i[v] = 1;
        dn_i[dim] = 1;
        const int U = dsz[u];
        const int V = dsz[v];

        for (int slice = 0; slice <= dsz[dim]; ++slice)
        {
            if (dim == 1 && slice == 0)
                continue;

            const size_t cells = static_cast<size_t>(U) * V;

            if (mask.size() < cells)
                mask.resize(cells);

            std::fill_n(mask.data(), cells, MaskCell{});

            for (int j = 0; j < V; ++j)
            {
                for (int i = 0; i < U; ++i)
                {
                    glm::ivec3 a(0), b(0);
                    a[dim] = slice;
                    b[dim] = slice - 1;
                    a[u] = i;
                    b[u] = i;
                    a[v] = j;
                    b[v] = j;
                    const Block ba = getCache(a.x, a.y, a.z);
                    const Block bb = getCache(b.x, b.y, b.z);
                    const bool sa = isSolid(a.x, a.y, a.z);
                    const bool sb = isSolid(b.x, b.y, b.z);

                    if (ba.id == bb.id || (sa && sb))
                        continue;
                    if (!sa && ba.id == BlockId::AIR && isNeighborMissing(a.x, a.z))
                        continue;
                    if (!sb && bb.id == BlockId::AIR && isNeighborMissing(b.x, b.z))
                        continue;

                    MaskCell c;
                    bool back = false;
                    BlockId id = BlockId::AIR;
                    if (sa && !sb)
                    {
                        id = ba.id;
                        back = false;
                    }
                    else if (!sa && sb)
                    {
                        id = bb.id;
                        back = true;
                    }
                    else
                    {
                        if (ba.id != BlockId::AIR)
                        {
                            id = ba.id;
                            back = false;
                        }
                        else
                        {
                            id = bb.id;
                            back = true;
                        }
                    }
                    c.block_id = static_cast<int8_t>(id) * (back ? -1 : 1);
                    mask[static_cast<size_t>(j) * U + i] = c;
                }
            }

            auto canMergeX = [&](const MaskCell &l, const MaskCell &r) -> bool
            { return l == r; };
            auto canMergeY = [&](int rt, int rb, int sx, int w) -> bool
            {
                const size_t rowTop = static_cast<size_t>(rt) * U;
                const size_t rowBot = static_cast<size_t>(rb) * U;
                for (int k = 0; k < w; ++k)
                    if (mask[rowTop + sx + k] != mask[rowBot + sx + k])
                        return false;
                return true;
            };

            for (int j = 0; j < V; ++j)
            {
                for (int i = 0; i < U;)
                {
                    MaskCell mc = mask[static_cast<size_t>(j) * U + i];
                    if (mc.block_id == 0)
                    {
                        ++i;
                        continue;
                    }
                    int quadW = 1;
                    while (i + quadW < U && canMergeX(mask[static_cast<size_t>(j) * U + i + quadW - 1], mask[static_cast<size_t>(j) * U + i + quadW]))
                        ++quadW;
                    int quadH = 1;
                    while (j + quadH < V && canMergeY(j + quadH - 1, j + quadH, i, quadW))
                        ++quadH;

                    bool back = mc.block_id < 0;
                    BlockId id = static_cast<BlockId>(back ? -mc.block_id : mc.block_id);
                    const auto &blockData = db.get_block_data(id);

                    bool is_transparent = !blockData.is_solid;
                    auto &vertices = is_transparent ? outTransparentVertices : outOpaqueVertices;
                    auto &indices = is_transparent ? outTransparentIndices : outOpaqueIndices;

                    if (is_transparent && id == BlockId::WATER)
                    {
                        const auto &bd = db.get_block_data(id);
                        int tex = (dim == 0) ? (back ? bd.texture_indices[4] : bd.texture_indices[5]) : (dim == 1) ? (back ? bd.texture_indices[0] : bd.texture_indices[1])
                                                                                                                   : (back ? bd.texture_indices[3] : bd.texture_indices[2]);
                        glm::vec3 tileO{(tex % 16) * ATLAS_INV_SIZE, (tex / 16) * ATLAS_INV_SIZE, 0.f};
                        auto uv = [&](const glm::vec3 &p) -> glm::vec2
                        { return (dim == 0) ? glm::vec2(p.z, p.y) : (dim == 1) ? glm::vec2(p.x, p.z)
                                                                               : glm::vec2(p.x, p.y); };

                        for (int qh = 0; qh < quadH; ++qh)
                        {
                            for (int qw = 0; qw < quadW; ++qw)
                            {
                                glm::vec3 p0(0);
                                p0[dim] = static_cast<float>(slice);
                                p0[u] = static_cast<float>(i + qw);
                                p0[v] = static_cast<float>(j + qh);

                                glm::vec3 duv(0), dvv(0);
                                duv[u] = 1.0f;
                                dvv[v] = 1.0f;

                                glm::vec3 v0 = p0;
                                glm::vec3 v1 = p0 + duv;
                                glm::vec3 v2 = p0 + duv + dvv;
                                glm::vec3 v3 = p0 + dvv;

                                if (id == BlockId::WATER && dim == 1 && !back)
                                {
                                    float top_y = v0.y;
                                    if (std::abs(v0.y - top_y) < 0.01f)
                                        v0.y -= 0.1f;
                                    if (std::abs(v1.y - top_y) < 0.01f)
                                        v1.y -= 0.1f;
                                    if (std::abs(v2.y - top_y) < 0.01f)
                                        v2.y -= 0.1f;
                                    if (std::abs(v3.y - top_y) < 0.01f)
                                        v3.y -= 0.1f;
                                }

                                uint32_t base = static_cast<uint32_t>(vertices.size());
                                vertices.push_back({v0, tileO, uv(v0)});
                                vertices.push_back({v1, tileO, uv(v1)});
                                vertices.push_back({v2, tileO, uv(v2)});
                                vertices.push_back({v3, tileO, uv(v3)});

                                if (back)
                                    indices.insert(indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
                                else
                                    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
                            }
                        }
                    }
                    else
                    {
                        glm::vec3 p0(0);
                        p0[dim] = static_cast<float>(slice);
                        p0[u] = static_cast<float>(i);
                        p0[v] = static_cast<float>(j);

                        glm::vec3 duv(0), dvv(0);
                        duv[u] = static_cast<float>(quadW);
                        dvv[v] = static_cast<float>(quadH);

                        const auto &bd = db.get_block_data(id);
                        int tex = (dim == 0) ? (back ? bd.texture_indices[4] : bd.texture_indices[5]) : (dim == 1) ? (back ? bd.texture_indices[0] : bd.texture_indices[1])
                                                                                                                   : (back ? bd.texture_indices[3] : bd.texture_indices[2]);
                        glm::vec3 tileO{(tex % 16) * ATLAS_INV_SIZE, (tex / 16) * ATLAS_INV_SIZE, 0.f};
                        auto uv = [&](const glm::vec3 &p) -> glm::vec2
                        { return (dim == 0) ? glm::vec2(p.z, p.y) : (dim == 1) ? glm::vec2(p.x, p.z)
                                                                               : glm::vec2(p.x, p.y); };

                        glm::vec3 v0 = p0;
                        glm::vec3 v1 = p0 + duv;
                        glm::vec3 v2 = p0 + duv + dvv;
                        glm::vec3 v3 = p0 + dvv;

                        uint32_t base = static_cast<uint32_t>(vertices.size());
                        vertices.push_back({v0, tileO, uv(v0)});
                        vertices.push_back({v1, tileO, uv(v1)});
                        vertices.push_back({v2, tileO, uv(v2)});
                        vertices.push_back({v3, tileO, uv(v3)});

                        if (back)
                            indices.insert(indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
                        else
                            indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
                    }

                    for (int y = 0; y < quadH; ++y)
                        for (int x = 0; x < quadW; ++x)
                            mask[static_cast<size_t>(j + y) * U + (i + x)].block_id = 0;
                    i += quadW;
                }
            }
        }
    }
}

void Chunk::buildAndStageMesh(VmaAllocator allocator, RingStagingArena &arena,
                              int lodLevel, ChunkMeshInput &meshInput)
{
    const auto t0 = hrc::now();
    if (m_State.load() == State::INITIAL)
        return;
    m_State.store(State::MESHING);

    static thread_local std::vector<Vertex> opaqueVertices, transparentVertices;
    static thread_local std::vector<uint32_t> opaqueIndices, transparentIndices;

    buildMeshGreedy(lodLevel, opaqueVertices, opaqueIndices, transparentVertices, transparentIndices, meshInput);

    UploadJob opaqueJob;
    UploadHelpers::stageChunkMesh(arena, opaqueVertices, opaqueIndices, opaqueJob);

    UploadJob transparentJob;
    UploadHelpers::stageChunkMesh(arena, transparentVertices, transparentIndices, transparentJob);

    {
        std::scoped_lock lock(m_PendingMutex);
        m_PendingUploads[lodLevel] = std::move(opaqueJob);
        m_PendingTransparentUploads[lodLevel] = std::move(transparentJob);
    }
    m_State.store(State::STAGING_READY);
}

void Chunk::cleanup(VulkanRenderer &renderer)
{
    markReady(renderer);
    for (auto &[lod, mesh] : m_Meshes)
    {
        if (mesh.vertexBuffer != VK_NULL_HANDLE)
            renderer.enqueueDestroy(mesh.vertexBuffer, mesh.vertexBufferAllocation);
        if (mesh.indexBuffer != VK_NULL_HANDLE)
            renderer.enqueueDestroy(mesh.indexBuffer, mesh.indexBufferAllocation);

        mesh.blas.destroy(renderer.getDevice());
    }

    for (auto &[lod, mesh] : m_TransparentMeshes)
    {
        if (mesh.vertexBuffer != VK_NULL_HANDLE)
            renderer.enqueueDestroy(mesh.vertexBuffer, mesh.vertexBufferAllocation);
        if (mesh.indexBuffer != VK_NULL_HANDLE)
            renderer.enqueueDestroy(mesh.indexBuffer, mesh.indexBufferAllocation);
    }
    m_Meshes.clear();
    m_TransparentMeshes.clear();
    m_State.store(State::INITIAL);
}

bool Chunk::hasLOD(int lodLevel) const
{
    std::scoped_lock lock(m_MeshesMutex);
    return m_Meshes.count(lodLevel) || m_TransparentMeshes.count(lodLevel);
}

const ChunkMesh *Chunk::getMesh(int lodLevel) const
{
    std::scoped_lock lock(m_MeshesMutex);
    auto it = m_Meshes.find(lodLevel);
    return it == m_Meshes.end() ? nullptr : &it->second;
}

ChunkMesh *Chunk::getMesh(int lodLevel)
{
    std::scoped_lock lock(m_MeshesMutex);
    auto it = m_Meshes.find(lodLevel);
    return it == m_Meshes.end() ? nullptr : &it->second;
}

const ChunkMesh *Chunk::getTransparentMesh(int lodLevel) const
{
    std::scoped_lock lock(m_MeshesMutex);
    auto it = m_TransparentMeshes.find(lodLevel);
    return it == m_TransparentMeshes.end() ? nullptr : &it->second;
}

int Chunk::getBestAvailableLOD(int requiredLod) const
{
    std::scoped_lock lock(m_MeshesMutex);
    for (int lod = requiredLod; lod >= 0; --lod)
        if (m_Meshes.count(lod) || m_TransparentMeshes.count(lod))
            return lod;
    return m_Meshes.empty() && m_TransparentMeshes.empty() ? -1 : m_Meshes.rbegin()->first;
}

void Chunk::buildAndStageDebugMesh(VmaAllocator allocator, RingStagingArena &arena)
{
    static thread_local std::vector<glm::vec3> vertices;
    static thread_local std::vector<uint32_t> indices;
    vertices.clear();
    indices.clear();
    for (int y = 0; y < HEIGHT; ++y)
    {
        for (int x = 0; x < WIDTH; ++x)
        {
            for (int z = 0; z < DEPTH; ++z)
            {
                if (getBlock(x, y, z).id != BlockId::AIR)
                {
                    uint32_t base_vertex = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(glm::vec3(x, y, z));
                    vertices.push_back(glm::vec3(x + 1, y, z));
                    vertices.push_back(glm::vec3(x + 1, y + 1, z));
                    vertices.push_back(glm::vec3(x, y + 1, z));
                    vertices.push_back(glm::vec3(x, y, z + 1));
                    vertices.push_back(glm::vec3(x + 1, y, z + 1));
                    vertices.push_back(glm::vec3(x + 1, y + 1, z + 1));
                    vertices.push_back(glm::vec3(x, y + 1, z + 1));
                    std::vector<uint32_t> cube_indices = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
                    for (uint32_t index : cube_indices)
                    {
                        indices.push_back(base_vertex + index);
                    }
                }
            }
        }
    }
}