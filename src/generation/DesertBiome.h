#pragma once
#include "Biome.h"
class DesertBiome : public Biome
{
public:
        void decorate(Chunk &c, FastNoiseLite &n) const override;
};