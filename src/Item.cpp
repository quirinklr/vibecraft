#include "Item.h"
#include <random>

Item::Item(Engine *engine, glm::vec3 position, BlockId blockId)
    : Entity(engine, position), m_blockId(blockId)
{
    const float size = 0.2f;
    m_hitbox.min = {-size / 2.0f, 0.0f, -size / 2.0f};
    m_hitbox.max = {size / 2.0f, size, size / 2.0f};

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    float random_x = dis(gen);
    float random_z = dis(gen);

    m_velocity = glm::vec3(random_x, 3.0f, random_z);
}

void Item::update(float dt)
{
    m_rotation += dt * 90.0f;
    m_time += dt;

    Entity::update(dt);
}

glm::vec3 Item::get_render_position() const
{

    glm::vec3 renderPos = m_position;

    renderPos.y += m_hitbox.max.y / 2.0f;

    if (m_is_on_ground)
    {
        const float bobble_amplitude = 0.05f;
        const float bobble_speed = 2.0f;

        float bobbleOffset = (sin(m_time * bobble_speed) + 1.0f) * 0.5f * bobble_amplitude;

        renderPos.y += bobbleOffset;
    }
    return renderPos;
}