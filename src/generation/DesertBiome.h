#pragma once
#include "Biome.h"
class DesertBiome : public Biome
{
public:
    uint8_t surface(uint8_t beneath, int y) const override;
    void decorate(Chunk &c, FastNoiseLite &n) const override;
};
