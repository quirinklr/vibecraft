#include "PlainsBiome.h"
#include "../Block.h"

Block PlainsBiome::surface(int y) const
{
    if(y == 0) {
        return {BlockId::GRASS};
    }
    else if (y <= 3) 
    {
        return {BlockId::DIRT};
    }
    else 
    {
        return {BlockId::STONE};
    }
}

void PlainsBiome::decorate(Chunk &, FastNoiseLite &) const {}
