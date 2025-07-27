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

private:
    FastNoiseLite m_continent;
    FastNoiseLite m_erosion;
    FastNoiseLite m_terrainType;
    FastNoiseLite m_domainWarp;
    FastNoiseLite m_temperature;
    FastNoiseLite m_humidity;
    FastNoiseLite m_caves;
    FastNoiseLite m_caveShape;
    FastNoiseLite m_bedrockNoise;

    std::unordered_map<BiomeType, std::unique_ptr<Biome>> m_biomes;
    float heightAt(int gx, int gz) const;
    BiomeType biomeAt(int gx, int gz) const;
    bool isCave(float x, float y, float z) const;

    static constexpr int SEA_LEVEL = 80;
};