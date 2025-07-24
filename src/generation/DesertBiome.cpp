#include "DesertBiome.h"
#include "../BlockIds.h"

uint8_t DesertBiome::surface(uint8_t, int) const
{
    return SAND;
}

void DesertBiome::decorate(Chunk &, FastNoiseLite &) const {}