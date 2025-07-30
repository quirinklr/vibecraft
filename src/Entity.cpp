#include "Entity.h"
#include "Engine.h"
#include "Block.h"
#include <cmath>

Entity::Entity(Engine *engine, glm::vec3 position)
    : m_engine(engine),
m_position(position)
{
    const float width = 0.6f;
    const float height = 1.8f;
    m_hitbox.min = {-width / 2.0f, 0.0f, -width / 2.0f};
    m_hitbox.max = {width / 2.0f, height, width / 2.0f};
}

void Entity::update(float dt)
{

    m_velocity.y += GRAVITY * dt;

    m_position += m_velocity * dt;

    resolve_collisions();
}

void Entity::resolve_collisions()
{
    m_is_on_ground = false;
    AABB entity_aabb = {m_position + m_hitbox.min, m_position + m_hitbox.max};

    int min_bx = static_cast<int>(floor(entity_aabb.min.x));
    int max_bx = static_cast<int>(ceil(entity_aabb.max.x));
    int min_by = static_cast<int>(floor(entity_aabb.min.y));
    int max_by = static_cast<int>(ceil(entity_aabb.max.y));
    int min_bz = static_cast<int>(floor(entity_aabb.min.z));
    int max_bz = static_cast<int>(ceil(entity_aabb.max.z));

    for (int by = min_by; by < max_by; ++by)
    {
        for (int bx = min_bx; bx < max_bx; ++bx)
        {
            for (int bz = min_bz; bz < max_bz; ++bz)
            {
                Block block = m_engine->get_block(bx, by, bz);
                if (!BlockDatabase::get().get_block_data(block.id).is_solid)
                {
                    continue;
                }

                AABB block_aabb = {glm::vec3(bx, by, bz), glm::vec3(bx + 1, by + 1, bz + 1)};
                AABB current_entity_aabb = {m_position + m_hitbox.min, m_position + m_hitbox.max};

                if (!current_entity_aabb.intersects(block_aabb))
                {
                    continue;
                }

                glm::vec3 overlap;
                overlap.x = (m_velocity.x > 0) ? (current_entity_aabb.max.x - block_aabb.min.x) : (block_aabb.max.x - current_entity_aabb.min.x);
                overlap.y = (m_velocity.y > 0) ? (current_entity_aabb.max.y - block_aabb.min.y) : (block_aabb.max.y - current_entity_aabb.min.y);
                overlap.z = (m_velocity.z > 0) ? (current_entity_aabb.max.z - block_aabb.min.z) : (block_aabb.max.z - current_entity_aabb.min.z);

                if (overlap.x < overlap.y && overlap.x < overlap.z)
                {
                    if (m_velocity.x > 0)
                        m_position.x -= overlap.x;
                    else
                        m_position.x += overlap.x;
                    m_velocity.x = 0;
                }
                else if (overlap.y < overlap.z)
                {
                    if (m_velocity.y > 0)
                        m_position.y -= overlap.y;
                    else
                        m_position.y += overlap.y;
                    if (m_velocity.y < 0)
                        m_is_on_ground = true;
                    m_velocity.y = 0;
                }
                else
                {
                    if (m_velocity.z > 0)
                        m_position.z -= overlap.z;
                    else
                        m_position.z += overlap.z;
                    m_velocity.z = 0;
                }
            }
        }
    }
}