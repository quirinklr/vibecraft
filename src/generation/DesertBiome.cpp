#include "DesertBiome.h"
#include "../Block.h"

Block DesertBiome::surface(int) const
{
    return {BlockId::SAND};
}

void DesertBiome::decorate(Chunk &, FastNoiseLite &) const {}