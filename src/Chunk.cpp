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

float calculateAO(bool side1, bool side2, bool corner)
{
    if (side1 && side2)
    {
        return 0.5f;
    }
    int obstructions = (side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0);
    switch (obstructions)
    {
    case 0:
        return 1.0f;
    case 1:
        return 0.7f;
    case 2:
        return 0.6f;
    default:
        return 0.5f;
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

void Chunk::buildMeshGreedy(int lodLevel, std::vector<Vertex> &outVertices, std::vector<uint32_t> &outIndices, ChunkMeshInput &meshInput)
{

    struct MaskCell
    {
        int8_t block_id = 0;
        float ao[4] = {1.0f, 1.0f, 1.0f, 1.0f};

        bool operator==(const MaskCell &other) const
        {
            if (block_id != other.block_id)
                return false;
            for (int i = 0; i < 4; ++i)
            {

                if (std::abs(ao[i] - other.ao[i]) > 0.001f)
                    return false;
            }
            return true;
        }

        bool operator!=(const MaskCell &other) const
        {
            return !(*this == other);
        }
    };

    auto getBlockFromSource = [&](int relX, int relY, int relZ) -> Block
    {
        if (relY < 0 || relY >= HEIGHT)
            return {BlockId::AIR};

        int chunkX = (relX < 0) ? -1 : (relX >= WIDTH ? 1 : 0);
        int chunkZ = (relZ < 0) ? -1 : (relZ >= DEPTH ? 1 : 0);
        int localX = relX - chunkX * WIDTH;
        int localZ = relZ - chunkZ * DEPTH;

        if (chunkX == 0 && chunkZ == 0)
        {
            return meshInput.selfChunk->getBlock(localX, relY, localZ);
        }

        int neighborIndex = -1;
        if (chunkX == -1 && chunkZ == 0)
            neighborIndex = 0;
        if (chunkX == 1 && chunkZ == 0)
            neighborIndex = 1;
        if (chunkX == 0 && chunkZ == -1)
            neighborIndex = 2;
        if (chunkX == 0 && chunkZ == 1)
            neighborIndex = 3;
        if (chunkX == -1 && chunkZ == -1)
            neighborIndex = 4;
        if (chunkX == 1 && chunkZ == -1)
            neighborIndex = 5;
        if (chunkX == -1 && chunkZ == 1)
            neighborIndex = 6;
        if (chunkX == 1 && chunkZ == 1)
            neighborIndex = 7;

        if (neighborIndex != -1 && meshInput.neighborChunks[neighborIndex])
        {
            return meshInput.neighborChunks[neighborIndex]->getBlock(localX, relY, localZ);
        }

        return {BlockId::AIR};
    };

    for (int y = 0; y < HEIGHT; ++y)
    {
        for (int z = 0; z < DEPTH + 2; ++z)
        {
            for (int x = 0; x < WIDTH + 2; ++x)
            {
                meshInput.cachedBlocks[y * (DEPTH + 2) * (WIDTH + 2) + z * (WIDTH + 2) + x] = getBlockFromSource(x - 1, y, z - 1);
            }
        }
    }

    const int w = WIDTH, h = HEIGHT, d = DEPTH;

    auto isSolid = [&](int x, int y, int z) -> bool
    {
        Block block = meshInput.getBlock(x + 1, y, z + 1);
        return BlockDatabase::get().get_block_data(block.id).is_solid;
    };

    auto getBlockFromCache = [&](int x, int y, int z) -> Block
    {
        return meshInput.getBlock(x + 1, y, z + 1);
    };

    outVertices.clear();
    outIndices.clear();
    auto &db = BlockDatabase::get();

    for (int dim = 0; dim < 3; ++dim)
    {
        int u = (dim + 1) % 3;
        int v = (dim + 2) % 3;
        int dsz[] = {w, h, d};

        glm::ivec3 du_i(0), dv_i(0), dn_i(0);
        du_i[u] = 1;
        dv_i[v] = 1;
        dn_i[dim] = 1;

        for (int slice = 0; slice <= dsz[dim]; ++slice)
        {

            if (dim == 1 && slice == 0)
                continue;

            std::vector<MaskCell> mask(dsz[u] * dsz[v]);

            for (int j = 0; j < dsz[v]; ++j)
            {
                for (int i = 0; i < dsz[u]; ++i)
                {
                    glm::ivec3 a_local{0}, b_local{0};
                    a_local[dim] = slice;
                    b_local[dim] = slice - 1;
                    a_local[u] = i;
                    b_local[u] = i;
                    a_local[v] = j;
                    b_local[v] = j;

                    Block blockA = getBlockFromCache(a_local.x, a_local.y, a_local.z);
                    Block blockB = getBlockFromCache(b_local.x, b_local.y, b_local.z);
                    const auto &dataA = db.get_block_data(blockA.id);
                    const auto &dataB = db.get_block_data(blockB.id);

                    bool solidA = dataA.is_solid;
                    bool solidB = dataB.is_solid;

                    if (solidA != solidB || (blockA.id == BlockId::WATER && blockB.id == BlockId::AIR) || (blockA.id == BlockId::AIR && blockB.id == BlockId::WATER))
                    {
                        MaskCell cell;
                        bool back_face = !solidA;
                        BlockId id;
                        
                        if (solidA)
                        {
                            id = blockA.id;
                        }
                        else if (solidB)
                        {
                            id = blockB.id;
                        }
                        else
                        {

                            id = BlockId::WATER;
                            back_face = (blockA.id == BlockId::AIR);
                        }

                        cell.block_id = static_cast<int8_t>(id) * (back_face ? -1 : 1);

                        glm::ivec3 p_local = a_local;

                        glm::ivec3 quad_corners_1x1[] = {
                            p_local,
                            p_local + du_i,
                            p_local + du_i + dv_i,
                            p_local + dv_i};

                        glm::ivec3 side1_offsets[] = {-du_i, du_i, du_i, -du_i};
                        glm::ivec3 side2_offsets[] = {-dv_i, -dv_i, dv_i, dv_i};

                        for (int k = 0; k < 4; ++k)
                        {
                            glm::ivec3 corner_pos = quad_corners_1x1[k];

                            bool s1 = isSolid(corner_pos.x + side1_offsets[k].x, corner_pos.y + side1_offsets[k].y, corner_pos.z + side1_offsets[k].z);
                            bool s2 = isSolid(corner_pos.x + side2_offsets[k].x, corner_pos.y + side2_offsets[k].y, corner_pos.z + side2_offsets[k].z);
                            bool corner = isSolid(corner_pos.x + side1_offsets[k].x + side2_offsets[k].x,
                                                  corner_pos.y + side1_offsets[k].y + side2_offsets[k].y,
                                                  corner_pos.z + side1_offsets[k].z + side2_offsets[k].z);
                            cell.ao[k] = calculateAO(s1, s2, corner);
                        }

                        mask[j * dsz[u] + i] = cell;
                    }
                }
            }

            for (int j = 0; j < dsz[v]; ++j)
            {
                for (int i = 0; i < dsz[u];)
                {

                    if (mask[j * dsz[u] + i].block_id == 0)
                    {
                        i++;
                        continue;
                    }

                    int quadWidth = 1;
                    while (i + quadWidth < dsz[u] && mask[j * dsz[u] + i + quadWidth] == mask[j * dsz[u] + i])
                        quadWidth++;

                    int quadHeight = 1;
                    bool stop = false;
                    while (j + quadHeight < dsz[v] && !stop)
                    {
                        for (int k = 0; k < quadWidth; ++k)
                        {
                            if (mask[(j + quadHeight) * dsz[u] + i + k] != mask[j * dsz[u] + i])
                            {
                                stop = true;
                                break;
                            }
                        }
                        if (!stop)
                            quadHeight++;
                    }

                    MaskCell start_cell = mask[j * dsz[u] + i];
                    bool back_face = start_cell.block_id < 0;
                    BlockId id = static_cast<BlockId>(back_face ? -start_cell.block_id : start_cell.block_id);

                    glm::vec3 p0(0);
                    p0[dim] = slice;
                    p0[u] = i;
                    p0[v] = j;

                    const auto &data = db.get_block_data(id);
                    glm::vec3 du_vec(0), dv_vec(0);
                    du_vec[u] = quadWidth;
                    dv_vec[v] = quadHeight;

                    uint32_t base = outVertices.size();
                    int texture_index;
                    if (dim == 0)
                        texture_index = back_face ? data.texture_indices[4] : data.texture_indices[5];
                    else if (dim == 1)
                        texture_index = back_face ? data.texture_indices[0] : data.texture_indices[1];
                    else
                        texture_index = back_face ? data.texture_indices[3] : data.texture_indices[2];

                    uint32_t idVal = texture_index;
                    glm::vec3 tileOrigin{(idVal % 16) * ATLAS_INV_SIZE, (idVal / 16) * ATLAS_INV_SIZE, 0.f};

                    float ao[4];
                    MaskCell c00 = mask[j * dsz[u] + i];
                    MaskCell c10 = mask[j * dsz[u] + i + quadWidth - 1];
                    MaskCell c11 = mask[(j + quadHeight - 1) * dsz[u] + i + quadWidth - 1];
                    MaskCell c01 = mask[(j + quadHeight - 1) * dsz[u] + i];

                    ao[0] = c00.ao[0];
                    ao[1] = c10.ao[1];
                    ao[2] = c11.ao[2];
                    ao[3] = c01.ao[3];

                    glm::vec3 v0 = p0, v1 = p0 + du_vec, v2 = p0 + du_vec + dv_vec, v3 = p0 + dv_vec;

                    auto getUV = [&](const glm::vec3 &pos) -> glm::vec2
                    {
                        switch (dim)
                        {
                        case 0:
                            return {pos.z, pos.y};
                        case 1:
                            return {pos.x, pos.z};
                        default:
                            return {pos.x, pos.y};
                        }
                    };

                    outVertices.push_back({v0, tileOrigin, getUV(v0), ao[0]});
                    outVertices.push_back({v1, tileOrigin, getUV(v1), ao[1]});
                    outVertices.push_back({v2, tileOrigin, getUV(v2), ao[2]});
                    outVertices.push_back({v3, tileOrigin, getUV(v3), ao[3]});

                    if (ao[0] + ao[2] > ao[1] + ao[3])
                    {
                        if (back_face)
                            outIndices.insert(outIndices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
                        else
                            outIndices.insert(outIndices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
                    }
                    else
                    {
                        if (back_face)
                            outIndices.insert(outIndices.end(), {base + 1, base + 3, base, base + 1, base + 2, base + 3});
                        else
                            outIndices.insert(outIndices.end(), {base, base + 1, base + 3, base + 1, base + 2, base + 3});
                    }

                    for (int y_ = 0; y_ < quadHeight; ++y_)
                        for (int x_ = 0; x_ < quadWidth; ++x_)

                            mask[(j + y_) * dsz[u] + i + x_].block_id = 0;

                    i += quadWidth;
                }
            }
        }
    }
}

void Chunk::buildAndStageMesh(VmaAllocator allocator, int lodLevel, ChunkMeshInput &meshInput)
{
    if (m_State.load() == State::INITIAL)
        return;
    m_State.store(State::MESHING);
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    buildMeshGreedy(lodLevel, vertices, indices, meshInput);
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
