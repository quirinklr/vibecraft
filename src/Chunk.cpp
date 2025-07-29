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

static constexpr uint8_t AO_TABLE[8] = {3, 2, 1, 0, 1, 0, 0, 0};

inline uint8_t calcAOInt(bool s1, bool s2, bool c)
{
    return AO_TABLE[(s1 << 2) | (s2 << 1) | c];
}

static uint8_t calculateAO(bool side1, bool side2, bool corner)
{
    if (side1 && side2)
        return 0;

    const int occ = int(side1) + int(side2) + int(corner);
    switch (occ)
    {
    case 0:
        return 3;
    case 1:
        return 2;
    case 2:
        return 1;
    default:
        return 0;
    }
}

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
                                     renderer.getCommandManager()->getCommandPool(),
                                     1, &job.cmdBuffer);

            vkDestroyFence(renderer.getDevice(), job.fence, nullptr);

            vmaDestroyBuffer(renderer.getAllocator(), job.stagingVB, job.stagingVbAlloc);
            vmaDestroyBuffer(renderer.getAllocator(), job.stagingIB, job.stagingIbAlloc);

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
    UploadHelpers::submitChunkMeshUpload(*renderer.getDeviceContext(),
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

void Chunk::buildMeshGreedy(int lodLevel,
                            std::vector<Vertex> &outVertices,
                            std::vector<uint32_t> &outIndices,
                            ChunkMeshInput &meshInput)
{
    struct MaskCell
    {
        int8_t block_id = 0;
        uint8_t ao[4] = {3, 3, 3, 3};
        bool operator==(const MaskCell &r) const
        {
            return block_id == r.block_id &&
                   ao[0] == r.ao[0] && ao[1] == r.ao[1] &&
                   ao[2] == r.ao[2] && ao[3] == r.ao[3];
        }
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
                meshInput.cachedBlocks[y * (DEPTH + 2) * (WIDTH + 2) + z * (WIDTH + 2) + x] =
                    getBlockFromSource(x - 1, y, z - 1);

    const int W = WIDTH, H = HEIGHT, D = DEPTH;
    const int PAD_W = W + 2, PAD_D = D + 2;

    static thread_local std::vector<uint8_t> solidLUT;
    {
        auto &db = BlockDatabase::get();
        const size_t cnt = db.blockCount();
        if (solidLUT.size() != cnt)
        {
            solidLUT.assign(cnt, 0);
            for (size_t id = 0; id < cnt; ++id)
                solidLUT[id] = db.get_block_data(static_cast<BlockId>(id)).is_solid;
        }
    }

    static thread_local std::vector<uint8_t> solidCache;
    solidCache.resize(static_cast<size_t>(H) * PAD_W * PAD_D);

    auto sidx = [&](int x, int y, int z) -> size_t
    {
        return static_cast<size_t>(y) * PAD_W * PAD_D +
               static_cast<size_t>(z) * PAD_W +
               static_cast<size_t>(x);
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
        if (x < -1 || x > WIDTH ||
            y < 0 || y >= HEIGHT ||
            z < -1 || z > DEPTH)
            return {BlockId::AIR};

        return meshInput.cachedBlocks[sidx(x + 1, y, z + 1)];
    };

    outVertices.clear();
    outIndices.clear();
    auto &db = BlockDatabase::get();

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

            mask.resize(static_cast<size_t>(U) * V);
            for (auto &m : mask)
                m.block_id = 0;

            for (int j = 0; j < V; ++j)
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

                    if (!(sa != sb || (ba.id == BlockId::WATER && bb.id == BlockId::AIR) ||
                          (ba.id == BlockId::AIR && bb.id == BlockId::WATER)))
                        continue;

                    MaskCell c;
                    bool back = !sa;
                    BlockId id = sa ? ba.id : (sb ? bb.id : BlockId::WATER);
                    if (!sa && !sb)
                        back = (ba.id == BlockId::AIR);
                    c.block_id = static_cast<int8_t>(id) * (back ? -1 : 1);

                    glm::ivec3 p = sa ? a : b;

                    for (int k = 0; k < 4; ++k)
                    {
                        int su = (k & 1) ? 1 : -1;
                        int sv = (k < 2) ? -1 : 1;
                        glm::ivec3 off1 = du_i * su;
                        glm::ivec3 off2 = dv_i * sv;
                        bool s1 = isSolid(p.x + off1.x, p.y + off1.y, p.z + off1.z);
                        bool s2 = isSolid(p.x + off2.x, p.y + off2.y, p.z + off2.z);
                        bool cr = isSolid(p.x + off1.x + off2.x,
                                          p.y + off1.y + off2.y,
                                          p.z + off1.z + off2.z);

                        if (s1 && s2)
                            c.ao[k] = 0;
                        else
                        {
                            const int occ = int(s1) + int(s2) + int(cr);
                            c.ao[k] = (occ == 0) ? 3 : (occ == 1) ? 2
                                                   : (occ == 2)   ? 1
                                                                  : 0;
                        }
                    }

                    mask[static_cast<size_t>(j) * U + i] = c;
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
                int i = 0;
                while (i < U)
                {
                    MaskCell mc = mask[static_cast<size_t>(j) * U + i];
                    if (mc.block_id == 0)
                    {
                        ++i;
                        continue;
                    }

                    int quadW = 1;
                    while (i + quadW < U &&
                           canMergeX(mask[static_cast<size_t>(j) * U + i + quadW - 1],
                                     mask[static_cast<size_t>(j) * U + i + quadW]))
                        ++quadW;

                    int quadH = 1;
                    while (j + quadH < V &&
                           canMergeY(j + quadH - 1, j + quadH, i, quadW))
                        ++quadH;

                    bool back = mc.block_id < 0;
                    BlockId id = static_cast<BlockId>(back ? -mc.block_id : mc.block_id);

                    glm::vec3 p0(0);
                    p0[dim] = static_cast<float>(slice);
                    p0[u] = static_cast<float>(i);
                    p0[v] = static_cast<float>(j);

                    glm::vec3 duv(0), dvv(0);
                    duv[u] = static_cast<float>(quadW);
                    dvv[v] = static_cast<float>(quadH);

                    const auto &bd = db.get_block_data(id);
                    int tex = (dim == 0)   ? (back ? bd.texture_indices[4] : bd.texture_indices[5])
                              : (dim == 1) ? (back ? bd.texture_indices[0] : bd.texture_indices[1])
                                           : (back ? bd.texture_indices[3] : bd.texture_indices[2]);

                    glm::vec3 tileO{(tex % 16) * ATLAS_INV_SIZE,
                                    (tex / 16) * ATLAS_INV_SIZE, 0.f};

                    uint8_t aoV[4];
                    glm::ivec3 solidV = back ? glm::ivec3(p0) - dn_i : glm::ivec3(p0);
                    for (int k = 0; k < 4; ++k)
                    {
                        int su = (k & 1) ? 1 : -1;
                        int sv = (k < 2) ? -1 : 1;
                        glm::ivec3 off1 = du_i * su;
                        glm::ivec3 off2 = dv_i * sv;
                        bool s1 = isSolid(solidV.x + off1.x, solidV.y + off1.y, solidV.z + off1.z);
                        bool s2 = isSolid(solidV.x + off2.x, solidV.y + off2.y, solidV.z + off2.z);
                        bool cr = isSolid(solidV.x + off1.x + off2.x,
                                          solidV.y + off1.y + off2.y,
                                          solidV.z + off1.z + off2.z);
                        if (s1 && s2)
                            aoV[k] = 0;
                        else
                        {
                            const int occ = int(s1) + int(s2) + int(cr);
                            aoV[k] = (occ == 0) ? 3 : (occ == 1) ? 2
                                                  : (occ == 2)   ? 1
                                                                 : 0;
                        }
                    }

                    float aoF[4] = {
                        static_cast<float>(aoV[0]),
                        static_cast<float>(aoV[1]),
                        static_cast<float>(aoV[2]),
                        static_cast<float>(aoV[3])};

                    auto uv = [&](const glm::vec3 &p) -> glm::vec2
                    {
                        return (dim == 0)   ? glm::vec2(p.z, p.y)
                               : (dim == 1) ? glm::vec2(p.x, p.z)
                                            : glm::vec2(p.x, p.y);
                    };

                    glm::vec3 v0 = p0;
                    glm::vec3 v1 = p0 + duv;
                    glm::vec3 v2 = p0 + duv + dvv;
                    glm::vec3 v3 = p0 + dvv;

                    uint32_t base = static_cast<uint32_t>(outVertices.size());
                    outVertices.push_back({v0, tileO, uv(v0), aoF[0]});
                    outVertices.push_back({v1, tileO, uv(v1), aoF[1]});
                    outVertices.push_back({v2, tileO, uv(v2), aoF[2]});
                    outVertices.push_back({v3, tileO, uv(v3), aoF[3]});

                    bool flip = (aoF[0] + aoF[2]) > (aoF[1] + aoF[3]);
                    if (flip)
                    {
                        if (back)
                            outIndices.insert(outIndices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
                        else
                            outIndices.insert(outIndices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
                    }
                    else
                    {
                        if (back)
                            outIndices.insert(outIndices.end(), {base + 1, base + 3, base, base + 1, base + 2, base + 3});
                        else
                            outIndices.insert(outIndices.end(), {base, base + 1, base + 3, base + 1, base + 2, base + 3});
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

#include <chrono>
using hrc = std::chrono::high_resolution_clock;
using milli = std::chrono::duration<double, std::milli>;

void Chunk::buildAndStageMesh(VmaAllocator allocator, int lodLevel,
                              ChunkMeshInput &meshInput)
{
    const auto t0 = hrc::now();

    if (m_State.load() == State::INITIAL)
        return;
    m_State.store(State::MESHING);

    const auto tBuild0 = hrc::now();

    static thread_local std::vector<Vertex> verticesTLS;
    static thread_local std::vector<uint32_t> indicesTLS;
    auto &vertices = verticesTLS;
    auto &indices = indicesTLS;

    vertices.clear();
    indices.clear();
    vertices.reserve(6 * 16 * 16 * 16);
    indices.reserve(6 * 6 * 16 * 16 * 16);

    buildMeshGreedy(lodLevel, vertices, indices, meshInput);

    const auto tBuild1 = hrc::now();

    const auto tStage0 = hrc::now();
    UploadJob job;
    UploadHelpers::stageChunkMesh(allocator, vertices, indices, job);
    const auto tStage1 = hrc::now();

    {
        std::scoped_lock lock(m_PendingMutex);
        m_PendingUploads[lodLevel] = std::move(job);
    }
    m_State.store(State::STAGING_READY);

    const auto tEnd = hrc::now();

    double msBuild = milli(tBuild1 - tBuild0).count();
    double msStage = milli(tStage1 - tStage0).count();
    double msTotal = milli(tEnd - t0).count();

    if (msTotal > 8.0)
    {
        printf("meshBuild %.2f ms  [build %.2f | stage %.2f | vtx %zu | idx %zu]\n",
               msTotal, msBuild, msStage,
               vertices.size(), indices.size());
    }
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
