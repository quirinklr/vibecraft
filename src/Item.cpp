#include "Item.h"
#include <random>

Item::Item(Engine* engine, glm::vec3 position, BlockId blockId)
    : Entity(engine, position), m_blockId(blockId)
{
    m_hitbox.min = {-0.25f, 0.0f, -0.25f};
    m_hitbox.max = {0.25f, 0.5f, 0.25f};

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    float random_x = dis(gen);
    float random_z = dis(gen);

    m_velocity = glm::vec3(random_x, 5.0f, random_z);
}

void Item::update(float dt)
{
    m_rotation += dt * 180.0f;
    m_time += dt;

    m_position.y += sin(m_time * 5.0f) * 0.005f;

    Entity::update(dt);
}
