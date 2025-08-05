#include "Block.h"

BlockDatabase &BlockDatabase::get()
{
    static BlockDatabase instance;
    return instance;
}

const BlockData &BlockDatabase::get_block_data(BlockId id) const
{
    return m_block_data[static_cast<size_t>(id)];
}

#include <cstdio>

void BlockDatabase::init()
{
    m_block_data[static_cast<size_t>(BlockId::AIR)].is_solid = false;
    m_block_data[static_cast<size_t>(BlockId::AIR)].hardness = 0.0f;

    m_block_data[static_cast<size_t>(BlockId::STONE)].is_solid = true;
    m_block_data[static_cast<size_t>(BlockId::STONE)].texture_indices.fill(1);
    m_block_data[static_cast<size_t>(BlockId::STONE)].hardness = 1.5f;

    m_block_data[static_cast<size_t>(BlockId::DIRT)].is_solid = true;
    m_block_data[static_cast<size_t>(BlockId::DIRT)].texture_indices.fill(2);
    m_block_data[static_cast<size_t>(BlockId::DIRT)].hardness = 0.5f;

    m_block_data[static_cast<size_t>(BlockId::WATER)].is_solid = false;
    m_block_data[static_cast<size_t>(BlockId::WATER)].texture_indices.fill(3);
    m_block_data[static_cast<size_t>(BlockId::WATER)].hardness = 0.0f;

    m_block_data[static_cast<size_t>(BlockId::SAND)].is_solid = true;
    m_block_data[static_cast<size_t>(BlockId::SAND)].texture_indices.fill(4);
    m_block_data[static_cast<size_t>(BlockId::SAND)].hardness = 0.5f;

    m_block_data[static_cast<size_t>(BlockId::GRASS)].is_solid = true;
    m_block_data[static_cast<size_t>(BlockId::GRASS)].texture_indices = {5, 2, 6, 6, 6, 6};
    m_block_data[static_cast<size_t>(BlockId::GRASS)].hardness = 0.6f;

    m_block_data[static_cast<size_t>(BlockId::BEDROCK)].is_solid = true;
    m_block_data[static_cast<size_t>(BlockId::BEDROCK)].texture_indices.fill(7);
    m_block_data[static_cast<size_t>(BlockId::BEDROCK)].hardness = -1.0f;
}