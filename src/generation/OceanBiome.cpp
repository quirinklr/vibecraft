#include "DesertBiome.h"
uint8_t DesertBiome::surface(uint8_t beneath, int y) const { return 2; }
void DesertBiome::decorate(Chunk &c, FastNoiseLite &n) const {}
