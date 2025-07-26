#include "Block.h"

BlockDatabase& BlockDatabase::get()
{
    static BlockDatabase instance;
    return instance;
}

const BlockData& BlockDatabase::get_block_data(BlockId id) const
{
    return m_block_data[static_cast<size_t>(id)];
}

#include <cstdio>

void BlockDatabase::init()
{
    printf("BlockDatabase::init() called\n");
    // Air
    m_block_data[static_cast<size_t>(BlockId::AIR)].is_solid = false;


    // Stone
    m_block_data[static_cast<size_t>(BlockId::STONE)].is_solid = true;
    m_block_data[static_cast<size_t>(BlockId::STONE)].texture_indices.fill(1);

    // Dirt
    m_block_data[static_cast<size_t>(BlockId::DIRT)].is_solid = true;
    m_block_data[static_cast<size_t>(BlockId::DIRT)].texture_indices.fill(2);

    // Water
    m_block_data[static_cast<size_t>(BlockId::WATER)].is_solid = false; // Or true for collision, but not for rendering back faces
    m_block_data[static_cast<size_t>(BlockId::WATER)].texture_indices.fill(3);

    // Sand
    m_block_data[static_cast<size_t>(BlockId::SAND)].is_solid = true;
    m_block_data[static_cast<size_t>(BlockId::SAND)].texture_indices.fill(4);

    // Grass
    m_block_data[static_cast<size_t>(BlockId::GRASS)].is_solid = true;
    m_block_data[static_cast<size_t>(BlockId::GRASS)].texture_indices = {5, 2, 6, 6, 6, 6}; // Top, Bottom, Sides...
}
