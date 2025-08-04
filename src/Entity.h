#pragma once

#include <glm/glm.hpp>
#include "math/AABB.h"

class Engine;
struct Block;
class BlockDatabase;

class Entity
{
public:
    virtual ~Entity() = default;

    Entity(Engine *engine, glm::vec3 position);

    virtual void update(float dt);

    glm::vec3 get_position() const { return m_position; }
    glm::vec3 get_render_position() const { return m_renderPosition; }

    glm::vec3 get_previous_position() const { return m_previousPosition; }
    AABB get_world_aabb() const { return {m_position + m_hitbox.min, m_position + m_hitbox.max}; }
    bool is_in_water() const { return m_is_in_water; }

    bool m_is_flying = false;

protected:
    void resolve_collisions();
    void check_for_water();

    Engine *m_engine;

    glm::vec3 m_position;
    glm::vec3 m_renderPosition;

    glm::vec3 m_previousPosition;
    glm::vec3 m_velocity{0.f};
    AABB m_hitbox;

    bool m_is_on_ground = false;
    bool m_is_in_water = false;

    const float GRAVITY = -32.0f;
    const float BUOYANCY_FORCE = 28.0f;
    const float WATER_DRAG_FACTOR = 3.5f;
};