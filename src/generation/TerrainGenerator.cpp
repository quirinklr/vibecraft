#include "TerrainGenerator.h"
#include "../Block.h"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

TerrainGenerator::TerrainGenerator()
{
    m_continent.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_continent.SetFrequency(0.004f);

    m_erosion.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_erosion.SetFrequency(0.01f);

    m_terrainRoughness.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_terrainRoughness.SetFrequency(0.001f);

    m_domainWarp.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_domainWarp.SetFrequency(0.005f);
    m_domainWarp.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
    m_domainWarp.SetDomainWarpAmp(50.0f);

    m_temperature.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_temperature.SetFrequency(0.001f);
    m_humidity.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_humidity.SetFrequency(0.001f);

    m_cavernNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_cavernNoise.SetFrequency(0.03f);

    m_tunnelNoise1.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_tunnelNoise1.SetFrequency(0.015f);
    m_tunnelNoise2.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_tunnelNoise2.SetFrequency(0.015f);
    m_tunnelNoise2.SetSeed(1337);

    m_bedrockNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_bedrockNoise.SetFrequency(0.1f);

    m_biomes[BiomeType::Plains] = std::make_unique<PlainsBiome>();
    m_biomes[BiomeType::Desert] = std::make_unique<DesertBiome>();
    m_biomes[BiomeType::Ocean] = std::make_unique<OceanBiome>();
}

float TerrainGenerator::heightAt(int gx, int gz) const
{
    float temp_val = (m_temperature.GetNoise((float)gx, (float)gz) + 1.f) * 0.5f;
    float biome_blend_alpha = glm::smoothstep(0.4f, 0.6f, temp_val);

    float roughness = (m_terrainRoughness.GetNoise((float)gx, (float)gz) + 1.f) * 0.5f;
    roughness = pow(roughness, 2.5f);

    float plains_amplitude = 10.f + roughness * 150.f;
    float plains_erosion_factor = roughness;

    float desert_amplitude = plains_amplitude * 0.4f;
    float desert_erosion_factor = roughness * 0.1f;

    float final_amplitude = glm::mix(plains_amplitude, desert_amplitude, biome_blend_alpha);
    float final_erosion_factor = glm::mix(plains_erosion_factor, desert_erosion_factor, biome_blend_alpha);

    float warpedX = (float)gx;
    float warpedZ = (float)gz;
    m_domainWarp.DomainWarp(warpedX, warpedZ);

    float continent_h = m_continent.GetNoise(warpedX, warpedZ) * final_amplitude;
    float erosion_h = m_erosion.GetNoise((float)gx * 2.f, (float)gz * 2.f) * 5.f * final_erosion_factor;

    return continent_h + erosion_h;
}

bool TerrainGenerator::isCave(float x, float y, float z) const
{
    if (y > SEA_LEVEL + 20 || y < 5)
        return false;

    float tunnel_val_1 = abs(m_tunnelNoise1.GetNoise(x, y * 2.0f, z));
    float tunnel_val_2 = abs(m_tunnelNoise2.GetNoise(x, y * 2.0f, z));
    const float tunnel_threshold = 0.025f;
    bool in_tunnel = (tunnel_val_1 < tunnel_threshold || tunnel_val_2 < tunnel_threshold);

    float cavern_val = m_cavernNoise.GetNoise(x, y, z);
    const float cavern_threshold = 0.65f;
    bool in_cavern = (cavern_val > cavern_threshold);

    return in_tunnel || in_cavern;
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

            float temp_val = (m_temperature.GetNoise((float)gx, (float)gz) + 1.f) * 0.5f;
            float biome_blend_alpha = glm::smoothstep(0.4f, 0.6f, temp_val);

            for (int y = 0; y < Chunk::HEIGHT; ++y)
            {
                Block block;

                if (y > ih)
                {
                    block.id = BlockId::AIR;
                }
                else
                {
                    if (y == ih && y >= SEA_LEVEL - 1)
                    {
                        if (biome_blend_alpha > 0.5f)
                        {
                            block.id = BlockId::SAND;
                        }
                        else
                        {
                            if (ih < SEA_LEVEL + 2)
                            {
                                block.id = BlockId::SAND;
                            }
                            else
                            {
                                block.id = BlockId::GRASS;
                            }
                        }
                    }
                    else if (y > ih - 4)
                    {
                        if (biome_blend_alpha > 0.5f)
                        {
                            block.id = BlockId::SAND;
                        }
                        else
                        {
                            block.id = BlockId::DIRT;
                        }
                    }
                    else
                    {
                        block.id = BlockId::STONE;
                    }
                }

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
                    else if (block.id != BlockId::BEDROCK)
                    {
                        block.id = BlockId::STONE;
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