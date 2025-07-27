#include "TerrainGenerator.h"
#include "../Block.h"
#include <cmath>

TerrainGenerator::TerrainGenerator()
{
    m_continent.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_continent.SetFrequency(0.004f);
    m_erosion.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_erosion.SetFrequency(0.02f);
    m_temperature.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_temperature.SetFrequency(0.001f);
    m_humidity.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_humidity.SetFrequency(0.001f);
    m_caves.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_caves.SetFrequency(0.07f);
    m_caveShape.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_caveShape.SetFrequency(0.02f);

    m_biomes[BiomeType::Plains] = std::make_unique<PlainsBiome>();
    m_biomes[BiomeType::Desert] = std::make_unique<DesertBiome>();
    m_biomes[BiomeType::Ocean] = std::make_unique<OceanBiome>();
}
float TerrainGenerator::heightAt(int gx, int gz) const
{
    float h = m_continent.GetNoise((float)gx, (float)gz) * 50.f;
    h += m_erosion.GetNoise((float)gx, (float)gz) * 10.f;
    return h;
}
BiomeType TerrainGenerator::biomeAt(int gx, int gz) const
{
    float t = (m_temperature.GetNoise((float)gx, (float)gz) + 1.f) * 0.5f;
    if (t > 0.7f)
        return BiomeType::Desert;
    return BiomeType::Plains;
}

bool TerrainGenerator::isCave(float x, float y, float z) const
{

    if (y > SEA_LEVEL + 20)
        return false;

    float caveRegion = m_caveShape.GetNoise(x, z);
    if (caveRegion < 0.4f)
    {
        return false;
    }

    float n = m_caves.GetNoise(x, y * 0.6f, z);
    return n > 0.6f;

}
void TerrainGenerator::populateChunk(Chunk &c)
{
    glm::ivec3 cp = c.getPos();

    for (int x = 0; x < Chunk::WIDTH; ++x)
        for (int z = 0; z < Chunk::DEPTH; ++z)
        {
            int gx = cp.x * Chunk::WIDTH + x;
            int gz = cp.z * Chunk::DEPTH + z;
            float h = SEA_LEVEL + heightAt(gx, gz);
            int ih = static_cast<int>(std::floor(h));

            BiomeType bt = biomeAt(gx, gz);
            Biome &b = *m_biomes.at(bt);

            for (int y = 0; y < Chunk::HEIGHT; ++y)
            {

                if (y > ih)
                {
                    if (y < SEA_LEVEL)
                        c.setBlock(x, y, z, {BlockId::WATER});
                    continue;
                }

                Block block = b.surface(ih - y);

                if (isCave(static_cast<float>(gx),
                           static_cast<float>(y),
                           static_cast<float>(gz)))
                    block.id = BlockId::AIR;

                c.setBlock(x, y, z, block);
            }
        }

    c.m_State.store(Chunk::State::TERRAIN_READY,
                    std::memory_order_release);
}
