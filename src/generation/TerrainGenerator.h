#pragma once
#include <unordered_map>
#include <memory>
#include "../Chunk.h"
#include <FastNoiseLite.h>

#include "Biome.h"
#include "PlainsBiome.h"
#include "DesertBiome.h"
#include "OceanBiome.h"

class TerrainGenerator
{
public:
    TerrainGenerator();
    void populateChunk(Chunk &c);
    static constexpr int SEA_LEVEL = 80;

private:
    FastNoiseLite m_continent;
    FastNoiseLite m_erosion;
    FastNoiseLite m_terrainRoughness;

    FastNoiseLite m_terrainType;
    FastNoiseLite m_domainWarp;
    FastNoiseLite m_temperature;
    FastNoiseLite m_humidity;

    FastNoiseLite m_cavernNoise;
    FastNoiseLite m_tunnelNoise1;
    FastNoiseLite m_tunnelNoise2;

    FastNoiseLite m_bedrockNoise;

    std::unordered_map<BiomeType, std::unique_ptr<Biome>> m_biomes;
    float heightAt(int gx, int gz) const;
    bool isCave(float x, float y, float z) const;
};