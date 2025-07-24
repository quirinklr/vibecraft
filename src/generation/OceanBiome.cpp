
#include "OceanBiome.h"
#include "../BlockIds.h"

uint8_t OceanBiome::surface(uint8_t beneath, int) const
{
    return beneath ? STONE : SAND;
}
void OceanBiome::decorate(Chunk &, FastNoiseLite &) const {}
