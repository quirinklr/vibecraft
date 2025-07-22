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

void Chunk::generateMesh(VulkanRenderer &renderer)
{
    // ──────────────────────────────────────────────────────────────
    // Hilfs‑Lambdas, damit wir nicht mehr außerhalb des Vektors lesen
    // ──────────────────────────────────────────────────────────────
    auto inside = [](int X, int Y, int Z) noexcept
    {
        return X >= 0 && X < WIDTH &&
               Y >= 0 && Y < HEIGHT &&
               Z >= 0 && Z < DEPTH;
    };
    auto idx = [](int X, int Y, int Z) noexcept -> size_t
    {
        return static_cast<size_t>(Y) * WIDTH * DEPTH + Z * WIDTH + X;
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<bool> visited(WIDTH * HEIGHT * DEPTH, false);

    // ──────────────────────────────────────────────────────────────
    // 6 Durchläufe = 6 Flächenrichtungen
    // ──────────────────────────────────────────────────────────────
    for (int face = 0; face < 6; ++face)
    {
        int u = (face + 1) % 3;
        int v = (face + 2) % 3;
        int d = face / 2;

        glm::ivec3 dir(0);
        dir[d] = 1 - 2 * (face % 2);

        glm::ivec3 x(0);
        glm::ivec3 q(0); // Schrittvektor in d‑Richtung
        q[d] = 1;

        for (x[d] = -1; x[d] < WIDTH;)
        { // Sweeping‑Ebene
            ++x[d];

            for (x[v] = 0; x[v] < HEIGHT; ++x[v])
            {
                for (x[u] = 0; x[u] < DEPTH; ++x[u])
                {

                    BlockID cur = inside(x[0], x[1], x[2]) ? getBlock(x[0], x[1], x[2]) : 0;
                    BlockID adj = inside(x[0] + dir[0],
                                         x[1] + dir[1],
                                         x[2] + dir[2])
                                      ? getBlock(x[0] + dir[0],
                                                 x[1] + dir[1],
                                                 x[2] + dir[2])
                                      : 0;

                    bool isOpaque = (cur != 0);
                    bool isAdjacentOpaque = (adj != 0);
                    bool alreadyVisited = inside(x[0], x[1], x[2]) &&
                                          visited[idx(x[0], x[1], x[2])];

                    if (isOpaque == isAdjacentOpaque || alreadyVisited)
                        continue;

                    // ───────── Greedy‑Ausdehnung in u‑ und v‑Richtung ────────
                    int w = 1, h = 1;

                    // Breite
                    while (x[u] + w < DEPTH &&
                           inside(x[0] + w * q[u], x[1], x[2] + w * q[u]) &&
                           !visited[idx(x[0] + w * q[u], x[1], x[2] + w * q[u])] &&
                           getBlock(x[0] + w * q[u], x[1], x[2] + w * q[u]) == cur)
                        ++w;

                    // Höhe
                    bool done = false;
                    while (!done && x[v] + h < HEIGHT)
                    {
                        for (int k = 0; k < w; ++k)
                        {
                            int X = x[0] + k * q[u], Y = x[1] + h, Z = x[2] + k * q[u];
                            if (!inside(X, Y, Z) ||
                                visited[idx(X, Y, Z)] ||
                                getBlock(X, Y, Z) != cur)
                            {
                                done = true;
                                break;
                            }
                        }
                        if (!done)
                            ++h;
                    }

                    // ───────── Quad erzeugen ─────────
                    glm::vec3 du(0), dv(0);
                    du[u] = 1;
                    dv[v] = 1;

                    glm::vec3 v1 = glm::vec3(x);
                    glm::vec3 v2 = v1 + du * (float)w;
                    glm::vec3 v3 = v1 + du * (float)w + dv * (float)h;
                    glm::vec3 v4 = v1 + dv * (float)h;

                    // Indices (CW / CCW je nach Normale)
                    if (dir[d] > 0)
                    {
                        indices.insert(indices.end(),
                                       {uint32_t(vertices.size() + 0),
                                        uint32_t(vertices.size() + 2),
                                        uint32_t(vertices.size() + 1),
                                        uint32_t(vertices.size() + 0),
                                        uint32_t(vertices.size() + 3),
                                        uint32_t(vertices.size() + 2)});
                    }
                    else
                    {
                        indices.insert(indices.end(),
                                       {uint32_t(vertices.size() + 0),
                                        uint32_t(vertices.size() + 1),
                                        uint32_t(vertices.size() + 2),
                                        uint32_t(vertices.size() + 2),
                                        uint32_t(vertices.size() + 3),
                                        uint32_t(vertices.size() + 0)});
                    }

                    vertices.push_back({v1, {1, 1, 1}, {0, 0}});
                    vertices.push_back({v2, {1, 1, 1}, {(float)w, 0}});
                    vertices.push_back({v3, {1, 1, 1}, {(float)w, (float)h}});
                    vertices.push_back({v4, {1, 1, 1}, {0, (float)h}});

                    // Markiere alle zusammengefassten Blöcke als besucht
                    for (int yy = 0; yy < h; ++yy)
                        for (int xx = 0; xx < w; ++xx)
                        {
                            int X = x[0] + xx * q[u], Y = x[1] + yy, Z = x[2] + xx * q[u];
                            if (inside(X, Y, Z))
                                visited[idx(X, Y, Z)] = true;
                        }
                }
            }
        }
    }

    if (indices.empty())
        return;

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