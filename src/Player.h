#pragma once

#include "Entity.h"
#include "Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <GLFW/glfw3.h>

struct Settings;

class Player : public Entity
{
public:
    Player(Engine *engine, glm::vec3 position, const Settings &settings);

    void update(float dt) override;

    void process_mouse_movement(float dx, float dy);
    void process_keyboard(GLFWwindow *window);
    bool raycast(glm::vec3& out_block_pos) const;

    Camera &get_camera() { return m_camera; }

private:
    void update_camera(Engine *engine);

    Camera m_camera;
    float m_yaw = -glm::half_pi<float>();
    float m_pitch = 0.f;
    bool m_is_sprinting = false;

    const Settings &m_settings;
    const float MOUSE_SENSITIVITY = 0.002f;
    const float WALK_SPEED = 5.0f;
    const float SPRINT_SPEED = 8.0f;
    const float JUMP_FORCE = 10.0f;
};