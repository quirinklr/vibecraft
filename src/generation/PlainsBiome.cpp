#include "PlainsBiome.h"
#include "../BlockIds.h"

uint8_t PlainsBiome::surface(uint8_t, int y) const
{
    if (y == 0)
        return DIRT;
    else if (y <= 3)
        return DIRT;
    else
        return STONE;
}
void PlainsBiome::decorate(Chunk &, FastNoiseLite &) const {}