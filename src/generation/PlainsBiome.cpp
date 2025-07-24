#include "PlainsBiome.h"
uint8_t PlainsBiome::surface(uint8_t beneath, int y) const { return y < 2 ? 1 : 2; }
void PlainsBiome::decorate(Chunk &, FastNoiseLite &) const {}
