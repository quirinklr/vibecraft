#pragma once

#include <cstdint>
#include <array>

enum class BlockId : uint8_t
{
    AIR = 0,
    STONE,
    DIRT,
    GRASS,
    SAND,
    WATER,
    BEDROCK,
    LAST
};

struct BlockData
{
    bool is_solid;
    float hardness = 1.0f;

    std::array<uint8_t, 6> texture_indices;
};

struct Block
{
    BlockId id = BlockId::AIR;
};

class BlockDatabase
{
public:
    static BlockDatabase &get();

    const BlockData &get_block_data(BlockId id) const;
    size_t blockCount() const { return m_block_data.size(); }
    void init();

private:
    BlockDatabase() = default;
    std::array<BlockData, static_cast<size_t>(BlockId::LAST)> m_block_data;
};