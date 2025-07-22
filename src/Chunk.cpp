#include "Chunk.h"
#include "FastNoiseLite.h"              // KORREKTER INCLUDE
#include "VulkanRenderer.h"             // Gibt uns die Vertex-Definition
#include <glm/gtc/matrix_transform.hpp> // Für glm::translate

Chunk::Chunk(glm::ivec3 position) : m_Position(position)
{
    m_Blocks.resize(WIDTH * HEIGHT * DEPTH, 0);
    m_ModelMatrix = glm::translate(glm::mat4(1.f), glm::vec3(position.x * WIDTH, position.y * HEIGHT, position.z * DEPTH));
}

Chunk::~Chunk() {}

BlockID Chunk::getBlock(int x, int y, int z) const
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
    {
        return 0;
    }
    return m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x];
}

void Chunk::generateTerrain(FastNoiseLite &noise)
{
    for (int x = 0; x < WIDTH; ++x)
    {
        for (int z = 0; z < DEPTH; ++z)
        {
            float globalX = (float)(m_Position.x * WIDTH + x);
            float globalZ = (float)(m_Position.z * DEPTH + z);

            float heightNoise = noise.GetNoise(globalX, globalZ);
            int groundHeight = 64 + static_cast<int>(heightNoise * 30.f);

            for (int y = 0; y < groundHeight; ++y)
            {
                m_Blocks[y * WIDTH * DEPTH + z * WIDTH + x] = 1; // Stein
            }
        }
    }
}

// ────────────────────────────────────────────────────────────────────
//  Erzeugt für **jeden** sichtbaren Block eine einfache Quad‑Fläche.
//  (Naiver Mesher – zuerst Korrektheit, Optimierung kommt später.)
// ────────────────────────────────────────────────────────────────────
void Chunk::generateMesh(VulkanRenderer &renderer)
{
    // vorhandene GPU‑Resourcen aus vorherigem Lauf aufräumen
    if (m_VertexBuffer != VK_NULL_HANDLE)
        cleanup(renderer.getDevice());

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    auto addQuad = [&](const glm::vec3 &v0,
                       const glm::vec3 &v1,
                       const glm::vec3 &v2,
                       const glm::vec3 &v3)
    {
        uint32_t start = static_cast<uint32_t>(vertices.size());

        vertices.push_back({v0, {1, 1, 1}, {0, 0}});
        vertices.push_back({v1, {1, 1, 1}, {1, 0}});
        vertices.push_back({v2, {1, 1, 1}, {1, 1}});
        vertices.push_back({v3, {1, 1, 1}, {0, 1}});

        // Front‑Face: clockwise ➜ VkFrontFace::VK_FRONT_FACE_CLOCKWISE
        indices.push_back(start + 0);
        indices.push_back(start + 1);
        indices.push_back(start + 2);

        indices.push_back(start + 0);
        indices.push_back(start + 2);
        indices.push_back(start + 3);
    };

    // 6 Richtungen (x+, x‑, y+, y‑, z+, z‑)
    const glm::ivec3 dirs[6] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    for (int z = 0; z < DEPTH; ++z)
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x)
            {
                if (getBlock(x, y, z) == 0)
                    continue; // Luft → nichts zu tun

                glm::vec3 p = {x, y, z}; // linke‑untere Ecke d. Blocks

                // ► für jede der sechs Seiten prüfen, ob Nachbar Luft ist
                for (int d = 0; d < 6; ++d)
                {
                    glm::ivec3 n = {x + dirs[d].x,
                                    y + dirs[d].y,
                                    z + dirs[d].z};

                    if (getBlock(n.x, n.y, n.z) != 0)
                        continue; // verdeckt

                    switch (d)
                    {
                    case 0: // +X
                        addQuad(p + glm::vec3(1, 0, 0),
                                p + glm::vec3(1, 0, 1),
                                p + glm::vec3(1, 1, 1),
                                p + glm::vec3(1, 1, 0));
                        break;
                    case 1: // -X
                        addQuad(p + glm::vec3(0, 0, 1),
                                p + glm::vec3(0, 0, 0),
                                p + glm::vec3(0, 1, 0),
                                p + glm::vec3(0, 1, 1));
                        break;
                    case 2: // +Y (Top)
                        addQuad(p + glm::vec3(0, 1, 1),
                                p + glm::vec3(1, 1, 1),
                                p + glm::vec3(1, 1, 0),
                                p + glm::vec3(0, 1, 0));
                        break;
                    case 3: // -Y (Bottom)
                        addQuad(p + glm::vec3(0, 0, 0),
                                p + glm::vec3(1, 0, 0),
                                p + glm::vec3(1, 0, 1),
                                p + glm::vec3(0, 0, 1));
                        break;
                    case 4: // +Z
                        addQuad(p + glm::vec3(0, 0, 1),
                                p + glm::vec3(1, 0, 1),
                                p + glm::vec3(1, 1, 1),
                                p + glm::vec3(0, 1, 1));
                        break;
                    case 5: // -Z
                        addQuad(p + glm::vec3(1, 0, 0),
                                p + glm::vec3(0, 0, 0),
                                p + glm::vec3(0, 1, 0),
                                p + glm::vec3(1, 1, 0));
                        break;
                    }
                }
            }

    if (indices.empty())
        return; // komplett leer

    m_IndexCount = static_cast<uint32_t>(indices.size());

    renderer.createChunkMeshBuffers(
        vertices, indices,
        m_VertexBuffer, m_VertexBufferMemory,
        m_IndexBuffer, m_IndexBufferMemory);
}

void Chunk::cleanup(VkDevice device)
{
    vkDestroyBuffer(device, m_IndexBuffer, nullptr);
    vkFreeMemory(device, m_IndexBufferMemory, nullptr);
    vkDestroyBuffer(device, m_VertexBuffer, nullptr);
    vkFreeMemory(device, m_VertexBufferMemory, nullptr);
}