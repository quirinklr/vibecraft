
#include "OceanBiome.h"
#include "../Block.h"

Block OceanBiome::surface(int) const
{
    return {BlockId::SAND};
}
void OceanBiome::decorate(Chunk &, FastNoiseLite &) const {}
