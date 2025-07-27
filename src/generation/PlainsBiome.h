#pragma once
#include "Biome.h"

class PlainsBiome : public Biome
{
public:
    void decorate(Chunk &c, FastNoiseLite &n) const override;
};
