#include "Player.h"
#include "Engine.h"
#include "Block.h"
#include "Settings.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>

Player::Player(Engine *engine, glm::vec3 position, const Settings &settings)
    : Entity(engine, position), m_settings(settings)
{
}

void Player::toggle_flight()
{
    m_is_flying = !m_is_flying;
    if (!m_is_flying)
    {
        m_velocity.y = 0;
    }
}

void Player::update(float dt)
{
    Entity::update(dt);
    update_camera(m_engine);
}

void Player::process_keyboard(GLFWwindow *window)
{
    if (m_is_flying)
    {

        glm::vec3 forward{cos(m_yaw) * cos(m_pitch), sin(m_pitch), sin(m_yaw) * cos(m_pitch)};
        glm::vec3 right = glm::normalize(glm::cross(forward, {0.f, 1.f, 0.f}));
        glm::vec3 up = {0.f, 1.f, 0.f};
        glm::vec3 move_direction{0.f};

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            move_direction += forward;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            move_direction -= forward;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            move_direction -= right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            move_direction += right;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            move_direction += up;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            move_direction -= up;

        if (glm::length(move_direction) > 0.0f)
        {
            move_direction = glm::normalize(move_direction);
        }
        m_velocity = move_direction * FLY_SPEED;
    }
    else if (m_is_in_water)
    {

        glm::vec3 forward{cos(m_yaw), 0, sin(m_yaw)};
        glm::vec3 right = glm::normalize(glm::cross(forward, {0.f, 1.f, 0.f}));
        glm::vec3 move_direction{0.f};

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            move_direction += forward;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            move_direction -= forward;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            move_direction -= right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            move_direction += right;

        if (glm::length(move_direction) > 0.0f)
        {
            move_direction = glm::normalize(move_direction);
        }

        m_velocity += move_direction * SWIM_ACCELERATION * (1.0f / 60.0f);

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            m_velocity.y += SWIM_UP_ACCELERATION * (1.0f / 60.0f);
        }
    }
    else
    {

        glm::vec3 forward{cos(m_yaw), 0, sin(m_yaw)};
        glm::vec3 right = glm::normalize(glm::cross(forward, {0.f, 1.f, 0.f}));
        glm::vec3 move_direction{0.f};

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            move_direction += forward;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            move_direction -= forward;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            move_direction -= right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            move_direction += right;

        m_is_sprinting = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
        float current_speed = m_is_sprinting ? SPRINT_SPEED : WALK_SPEED;

        if (glm::length(move_direction) > 0.0f)
        {
            move_direction = glm::normalize(move_direction);
        }

        m_velocity.x = move_direction.x * current_speed;
        m_velocity.z = move_direction.z * current_speed;

        if (m_is_on_ground && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            m_velocity.y = JUMP_FORCE;
        }
    }
}

bool Player::raycast(glm::vec3 &out_block_pos) const
{
    const float reach = 5.0f;
    const float step = 0.05f;

    glm::vec3 position = m_position + glm::vec3(0.f, m_hitbox.max.y * 0.9f, 0.f);
    glm::vec3 direction{
        cos(m_yaw) * cos(m_pitch),
        sin(m_pitch),
        sin(m_yaw) * cos(m_pitch)};

    for (float t = 0.0f; t < reach; t += step)
    {
        glm::vec3 current_pos = position + direction * t;
        int block_x = static_cast<int>(floor(current_pos.x));
        int block_y = static_cast<int>(floor(current_pos.y));
        int block_z = static_cast<int>(floor(current_pos.z));

        Block block = m_engine->get_block(block_x, block_y, block_z);
        if (BlockDatabase::get().get_block_data(block.id).is_solid)
        {
            out_block_pos = {block_x, block_y, block_z};
            return true;
        }
    }

    return false;
}

void Player::get_orientation(float &yaw, float &pitch) const
{
    yaw = m_yaw;
    pitch = m_pitch;
}

void Player::process_mouse_movement(float dx, float dy)
{
    float processed_dy = m_settings.invertMouseY ? dy : -dy;
    m_yaw += dx * m_settings.mouseSensitivityX * MOUSE_SENSITIVITY;
    m_pitch += processed_dy * m_settings.mouseSensitivityY * MOUSE_SENSITIVITY;
    m_pitch = glm::clamp(m_pitch, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
}

void Player::update_camera(Engine *engine)
{
    glm::vec3 eye_position = m_position + glm::vec3(0.f, m_hitbox.max.y * 0.9f, 0.f);
    glm::vec3 look_direction{
        cos(m_yaw) * cos(m_pitch),
        sin(m_pitch),
        sin(m_yaw) * cos(m_pitch)};
    m_camera.setViewDirection(eye_position, look_direction);

    auto ext = engine->get_window().getExtent();
    if (ext.height > 0)
    {
        m_camera.setPerspectiveProjection(
            glm::radians(m_settings.fov),
            static_cast<float>(ext.width) / ext.height,
            0.1f,
            1000.f);
    }
}