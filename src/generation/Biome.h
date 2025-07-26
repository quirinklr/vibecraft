#pragma once
#include <cstdint>

class Chunk;
class FastNoiseLite;
struct Block;

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
    virtual Block surface(int y) const = 0;
    virtual void decorate(Chunk &c, FastNoiseLite &n) const = 0;
};
