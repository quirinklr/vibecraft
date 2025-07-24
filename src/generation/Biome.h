#pragma once
#include <cstdint>
class Chunk;
class FastNoiseLite;
enum class BiomeType : uint8_t
{
    Plains,
    Desert,
    Ocean
};
class Biome
{
public:
    virtual ~Biome() = default;
    virtual uint8_t surface(uint8_t beneath, int y) const = 0;
    virtual void decorate(Chunk &c, FastNoiseLite &n) const = 0;
};
