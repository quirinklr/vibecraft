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
    AABB get_world_aabb() const { return {m_position + m_hitbox.min, m_position + m_hitbox.max}; }

protected:
    void resolve_collisions();

    Engine *m_engine;

    glm::vec3 m_position;
    glm::vec3 m_velocity{0.f};
    AABB m_hitbox;

    bool m_is_on_ground = false;

    const float GRAVITY = -32.0f;
};