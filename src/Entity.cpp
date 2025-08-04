#include "Entity.h"
#include "Engine.h"
#include "Block.h"
#include <cmath>

Entity::Entity(Engine *engine, glm::vec3 position)
    : m_engine(engine),
      m_position(position),
      m_previousPosition(position),
      m_renderPosition(position)
{
    const float width = 0.6f;
    const float height = 1.8f;
    m_hitbox.min = {-width / 2.0f, 0.0f, -width / 2.0f};
    m_hitbox.max = {width / 2.0f, height, width / 2.0f};
}

void Entity::check_for_water()
{
    m_is_in_water = false;
    AABB entity_aabb = get_world_aabb();

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
                if (block.id == BlockId::WATER)
                {

                    m_is_in_water = true;
                    return;
                }
            }
        }
    }
}

void Entity::update(float dt)
{
    m_previousPosition = m_position;

    check_for_water();

    if (m_is_flying)
    {
        m_position += m_velocity * dt;

        return;
    }

    m_velocity.y += GRAVITY * dt;
    if (m_is_in_water)
    {
        m_velocity.y += BUOYANCY_FORCE * dt;
        m_velocity -= m_velocity * WATER_DRAG_FACTOR * dt;
    }

    m_is_on_ground = false;

    m_position.y += m_velocity.y * dt;
    AABB entityAABB = get_world_aabb();
    int min_by = static_cast<int>(floor(entityAABB.min.y));
    int max_by = static_cast<int>(ceil(entityAABB.max.y));

    int min_bx = static_cast<int>(floor(entityAABB.min.x));
    int max_bx = static_cast<int>(ceil(entityAABB.max.x));
    int min_bz = static_cast<int>(floor(entityAABB.min.z));
    int max_bz = static_cast<int>(ceil(entityAABB.max.z));

    for (int by = min_by; by < max_by; ++by)
    {
        for (int bx = min_bx; bx < max_bx; ++bx)
        {
            for (int bz = min_bz; bz < max_bz; ++bz)
            {
                Block block = m_engine->get_block(bx, by, bz);
                if (!BlockDatabase::get().get_block_data(block.id).is_solid)
                    continue;

                AABB blockAABB = {glm::vec3(bx, by, bz), glm::vec3(bx + 1, by + 1, bz + 1)};
                if (get_world_aabb().intersects(blockAABB))
                {
                    if (m_velocity.y > 0)
                    {
                        m_position.y = blockAABB.min.y - m_hitbox.max.y;
                    }
                    else
                    {
                        m_position.y = blockAABB.max.y - m_hitbox.min.y;
                        m_is_on_ground = true;
                    }
                    m_velocity.y = 0;
                }
            }
        }
    }

    m_position.x += m_velocity.x * dt;
    entityAABB = get_world_aabb();
    min_by = static_cast<int>(floor(entityAABB.min.y));
    max_by = static_cast<int>(ceil(entityAABB.max.y));
    min_bx = static_cast<int>(floor(entityAABB.min.x));
    max_bx = static_cast<int>(ceil(entityAABB.max.x));
    min_bz = static_cast<int>(floor(entityAABB.min.z));
    max_bz = static_cast<int>(ceil(entityAABB.max.z));

    for (int by = min_by; by < max_by; ++by)
    {
        for (int bx = min_bx; bx < max_bx; ++bx)
        {
            for (int bz = min_bz; bz < max_bz; ++bz)
            {
                Block block = m_engine->get_block(bx, by, bz);
                if (!BlockDatabase::get().get_block_data(block.id).is_solid)
                    continue;

                AABB blockAABB = {glm::vec3(bx, by, bz), glm::vec3(bx + 1, by + 1, bz + 1)};
                if (get_world_aabb().intersects(blockAABB))
                {
                    if (m_velocity.x > 0)
                    {
                        m_position.x = blockAABB.min.x - m_hitbox.max.x;
                    }
                    else
                    {
                        m_position.x = blockAABB.max.x - m_hitbox.min.x;
                    }
                    m_velocity.x = 0;
                }
            }
        }
    }

    m_position.z += m_velocity.z * dt;
    entityAABB = get_world_aabb();
    min_by = static_cast<int>(floor(entityAABB.min.y));
    max_by = static_cast<int>(ceil(entityAABB.max.y));
    min_bx = static_cast<int>(floor(entityAABB.min.x));
    max_bx = static_cast<int>(ceil(entityAABB.max.x));
    min_bz = static_cast<int>(floor(entityAABB.min.z));
    max_bz = static_cast<int>(ceil(entityAABB.max.z));

    for (int by = min_by; by < max_by; ++by)
    {
        for (int bx = min_bx; bx < max_bx; ++bx)
        {
            for (int bz = min_bz; bz < max_bz; ++bz)
            {
                Block block = m_engine->get_block(bx, by, bz);
                if (!BlockDatabase::get().get_block_data(block.id).is_solid)
                    continue;

                AABB blockAABB = {glm::vec3(bx, by, bz), glm::vec3(bx + 1, by + 1, bz + 1)};
                if (get_world_aabb().intersects(blockAABB))
                {
                    if (m_velocity.z > 0)
                    {
                        m_position.z = blockAABB.min.z - m_hitbox.max.z;
                    }
                    else
                    {
                        m_position.z = blockAABB.max.z - m_hitbox.min.z;
                    }
                    m_velocity.z = 0;
                }
            }
        }
    }

    if (m_is_on_ground && !m_is_in_water)
    {
        float ground_friction = 10.0f;
        m_velocity.x *= std::max(0.0f, 1.0f - dt * ground_friction);
        m_velocity.z *= std::max(0.0f, 1.0f - dt * ground_friction);
    }
    else if (!m_is_in_water)
    {
        float air_drag = 1.0f;
        m_velocity.x *= std::max(0.0f, 1.0f - dt * air_drag);
        m_velocity.z *= std::max(0.0f, 1.0f - dt * air_drag);
    }
}