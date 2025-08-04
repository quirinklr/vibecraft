#include "Player.h"
#include "Engine.h"
#include "Block.h"
#include "Settings.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cfloat>

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

void Player::cycle_camera_mode()
{
    m_cameraMode = static_cast<CameraMode>((static_cast<int>(m_cameraMode) + 1) % 3);
}

void Player::update(float dt)
{

    Entity::update(dt);
}

void Player::process_keyboard(GLFWwindow *window, float dt)
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
        m_velocity += move_direction * SWIM_ACCELERATION * dt;

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            m_velocity.y += SWIM_UP_ACCELERATION * dt;
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

        if (glm::length(move_direction) > 0.0f)
        {
            move_direction = glm::normalize(move_direction);
        }

        const float GROUND_ACCELERATION = 80.0f;
        const float AIR_ACCELERATION = 15.0f;
        float acceleration = m_is_on_ground ? GROUND_ACCELERATION : AIR_ACCELERATION;

        m_velocity.x += move_direction.x * acceleration * dt;
        m_velocity.z += move_direction.z * acceleration * dt;

        m_is_sprinting = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
        float current_speed_limit = m_is_sprinting ? SPRINT_SPEED : WALK_SPEED;

        glm::vec3 horizontal_velocity = {m_velocity.x, 0.0f, m_velocity.z};
        if (glm::length(horizontal_velocity) > current_speed_limit)
        {
            glm::vec3 limited_velocity = glm::normalize(horizontal_velocity) * current_speed_limit;
            m_velocity.x = limited_velocity.x;
            m_velocity.z = limited_velocity.z;
        }

        if (m_is_on_ground && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            m_velocity.y = JUMP_FORCE;
        }
    }
}

bool Player::raycast(glm::vec3 &out_block_pos) const
{
    const float reach = 5.0f;

    glm::vec3 position = m_position + glm::vec3(0.f, m_hitbox.max.y * 0.9f, 0.f);
    glm::vec3 direction = {
        cos(m_yaw) * cos(m_pitch),
        sin(m_pitch),
        sin(m_yaw) * cos(m_pitch)};

    int blockX = static_cast<int>(floor(position.x));
    int blockY = static_cast<int>(floor(position.y));
    int blockZ = static_cast<int>(floor(position.z));

    const int stepX = (direction.x > 0) ? 1 : -1;
    const int stepY = (direction.y > 0) ? 1 : -1;
    const int stepZ = (direction.z > 0) ? 1 : -1;

    const float tDeltaX = (direction.x == 0) ? FLT_MAX : abs(1.0f / direction.x);
    const float tDeltaY = (direction.y == 0) ? FLT_MAX : abs(1.0f / direction.y);
    const float tDeltaZ = (direction.z == 0) ? FLT_MAX : abs(1.0f / direction.z);

    float tMaxX = (stepX > 0) ? (floor(position.x) + 1 - position.x) * tDeltaX : (position.x - floor(position.x)) * tDeltaX;
    float tMaxY = (stepY > 0) ? (floor(position.y) + 1 - position.y) * tDeltaY : (position.y - floor(position.y)) * tDeltaY;
    float tMaxZ = (stepZ > 0) ? (floor(position.z) + 1 - position.z) * tDeltaZ : (position.z - floor(position.z)) * tDeltaZ;

    float t = 0.0f;

    while (t <= reach)
    {
        Block block = m_engine->get_block(blockX, blockY, blockZ);
        if (BlockDatabase::get().get_block_data(block.id).is_solid)
        {
            out_block_pos = {blockX, blockY, blockZ};
            return true;
        }

        if (tMaxX < tMaxY)
        {
            if (tMaxX < tMaxZ)
            {
                t = tMaxX;
                tMaxX += tDeltaX;
                blockX += stepX;
            }
            else
            {
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                blockZ += stepZ;
            }
        }
        else
        {
            if (tMaxY < tMaxZ)
            {
                t = tMaxY;
                tMaxY += tDeltaY;
                blockY += stepY;
            }
            else
            {
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                blockZ += stepZ;
            }
        }
    }

    return false;
}

void Player::get_orientation(float &yaw, float &pitch) const
{
    yaw = m_yaw;
    pitch = m_pitch;
}

void Player::update_camera_interpolated(Engine *engine, float alpha)
{
    glm::vec3 interpolated_pos = glm::mix(m_previousPosition, m_position, alpha);
    m_renderPosition = interpolated_pos;
    glm::vec3 eye_pos = m_renderPosition + glm::vec3(0.f, m_hitbox.max.y * 0.9f, 0.f);
    glm::vec3 look_direction{
        cos(m_yaw) * cos(m_pitch),
        sin(m_pitch),
        sin(m_yaw) * cos(m_pitch)};

    glm::vec3 final_cam_pos;
    glm::vec3 final_look_dir;

    switch (m_cameraMode)
    {
    case CameraMode::FIRST_PERSON:
        final_cam_pos = eye_pos;
        final_look_dir = look_direction;
        break;

    case CameraMode::THIRD_PERSON:
    {
        glm::vec3 ideal_cam_pos = eye_pos - look_direction * THIRD_PERSON_DISTANCE;
        final_cam_pos = ideal_cam_pos;
        engine->raycast_camera_occlusion(eye_pos, final_cam_pos);
        final_look_dir = glm::normalize(eye_pos - final_cam_pos);
        break;
    }

    case CameraMode::FOURTH_PERSON:
    {
        glm::vec3 ideal_cam_pos = eye_pos + look_direction * THIRD_PERSON_DISTANCE;
        final_cam_pos = ideal_cam_pos;
        engine->raycast_camera_occlusion(eye_pos, final_cam_pos);
        final_look_dir = glm::normalize(eye_pos - final_cam_pos);
        break;
    }
    }

    m_camera.setViewDirection(final_cam_pos, final_look_dir);

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

void Player::process_mouse_movement(float dx, float dy)
{
    float processed_dy = m_settings.invertMouseY ? dy : -dy;
    m_yaw += dx * m_settings.mouseSensitivityX * MOUSE_SENSITIVITY;
    m_pitch += processed_dy * m_settings.mouseSensitivityY * MOUSE_SENSITIVITY;
    m_pitch = glm::clamp(m_pitch, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
}