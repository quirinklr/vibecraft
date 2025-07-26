#pragma once
#include "Biome.h"

class PlainsBiome : public Biome
{
public:
    Block surface(int y) const override;
    void decorate(Chunk &c, FastNoiseLite &n) const override;
};
