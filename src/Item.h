#pragma once

#include "Entity.h"
#include "Block.h"

class Item : public Entity
{
public:
    Item(Engine* engine, glm::vec3 position, BlockId blockId);

    void update(float dt) override;

    BlockId get_block_id() const { return m_blockId; }
    float get_rotation() const { return m_rotation; }

private:
    BlockId m_blockId;
    float m_rotation = 0.0f;
    float m_time = 0.0f;
};
