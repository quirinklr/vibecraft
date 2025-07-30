#include "TerrainGenerator.h"
#include "../Block.h"
#include <cmath>

TerrainGenerator::TerrainGenerator()
{
    m_continent.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_continent.SetFrequency(0.004f);

    m_erosion.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_erosion.SetFrequency(0.01f);

    m_terrainType.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_terrainType.SetFrequency(0.001f);

    m_domainWarp.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_domainWarp.SetFrequency(0.005f);
    m_domainWarp.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
    m_domainWarp.SetDomainWarpAmp(50.0f);

    m_temperature.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_temperature.SetFrequency(0.001f);
    m_humidity.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_humidity.SetFrequency(0.001f);
    m_caves.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_caves.SetFrequency(0.07f);
    m_caveShape.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_caveShape.SetFrequency(0.02f);

    m_bedrockNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_bedrockNoise.SetFrequency(0.1f);

    m_biomes[BiomeType::Plains] = std::make_unique<PlainsBiome>();
    m_biomes[BiomeType::Desert] = std::make_unique<DesertBiome>();
    m_biomes[BiomeType::Ocean] = std::make_unique<OceanBiome>();
}

float TerrainGenerator::heightAt(int gx, int gz) const
{
    float terrain_val = (m_terrainType.GetNoise((float)gx, (float)gz) + 1.f) * 0.5f;
    terrain_val = pow(terrain_val, 2.5f);

    float amplitude = 10.f + terrain_val * 150.f;

    float warpedX = (float)gx;
    float warpedZ = (float)gz;
    m_domainWarp.DomainWarp(warpedX, warpedZ);

    float continent_h = m_continent.GetNoise(warpedX, warpedZ) * amplitude;
    float erosion_h = m_erosion.GetNoise((float)gx * 2.f, (float)gz * 2.f) * 5.f;

    return continent_h + erosion_h;
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
    {
        for (int z = 0; z < Chunk::DEPTH; ++z)
        {
            int gx = cp.x * Chunk::WIDTH + x;
            int gz = cp.z * Chunk::DEPTH + z;

            float h_center = heightAt(gx, gz);
            int ih = static_cast<int>(std::floor(SEA_LEVEL + h_center));

            for (int y = 0; y < Chunk::HEIGHT; ++y)
            {
                Block block;

                if (y == 0)
                {
                    block.id = BlockId::BEDROCK;
                }
                else if (y < 5)
                {
                    float bedrock_n = m_bedrockNoise.GetNoise((float)gx, (float)y, (float)gz);
                    if (bedrock_n > (y * -0.1f + 0.2f))
                    {
                        block.id = BlockId::BEDROCK;
                    }
                    else
                    {
                        block.id = BlockId::STONE;
                    }
                }
                else if (y > ih)
                {

                    block.id = BlockId::AIR;
                }
                else
                {

                    float h_dx = heightAt(gx + 1, gz);
                    float h_dz = heightAt(gx, gz + 1);
                    float steepness = std::sqrt(pow(h_dx - h_center, 2) + pow(h_dz - h_center, 2));
                    BiomeType bt = biomeAt(gx, gz);

                    if (bt == BiomeType::Desert)
                    {
                        block.id = (steepness > 1.5f) ? BlockId::STONE : BlockId::SAND;
                    }
                    else
                    {
                        if (steepness > 1.5f)
                        {
                            block.id = BlockId::STONE;
                        }
                        else if (y == ih)
                        {
                            block.id = BlockId::GRASS;
                        }
                        else if (y > ih - 4)
                        {
                            block.id = BlockId::DIRT;
                        }
                        else
                        {
                            block.id = BlockId::STONE;
                        }
                    }
                }

                if (block.id != BlockId::AIR && isCave(static_cast<float>(gx), static_cast<float>(y), static_cast<float>(gz)))
                {
                    block.id = BlockId::AIR;
                }

                if (block.id == BlockId::AIR && y < SEA_LEVEL)
                {
                    block.id = BlockId::WATER;
                }

                c.setBlock(x, y, z, block);
            }
        }
    }
    c.m_State.store(Chunk::State::TERRAIN_READY, std::memory_order_release);
}