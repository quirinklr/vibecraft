#pragma once
#include "Biome.h"
class OceanBiome : public Biome
{
public:
    void decorate(Chunk &c, FastNoiseLite &n) const override;
};
