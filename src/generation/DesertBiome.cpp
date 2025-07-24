#include "OceanBiome.h"
uint8_t OceanBiome::surface(uint8_t beneath, int y) const { return 1; }
void OceanBiome::decorate(Chunk &c, FastNoiseLite &n) const {}
