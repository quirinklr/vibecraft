#pragma once

#include "Entity.h"
#include "Camera.h"
#include <glm/gtc/constants.hpp>
#include <GLFW/glfw3.h>

struct Settings;

enum class CameraMode
{
    FIRST_PERSON,
    THIRD_PERSON,
    FOURTH_PERSON
};

class Player : public Entity
{
public:
    Player(Engine *engine, glm::vec3 position, const Settings &settings);

    void update(float dt) override;

    bool raycast(glm::vec3 &out_block_pos) const;
    void process_mouse_movement(float dx, float dy);
    void get_orientation(float &yaw, float &pitch) const;
    void process_keyboard(GLFWwindow *window, float dt);
    void toggle_flight();
    void cycle_camera_mode();

    Camera &get_camera() { return m_camera; }

    void update_camera_interpolated(Engine *engine, float alpha);

private:
    CameraMode m_cameraMode = CameraMode::FIRST_PERSON;

    Camera m_camera;
    float m_yaw = -glm::half_pi<float>();
    float m_pitch = 0.f;
    bool m_is_sprinting = false;

    const Settings &m_settings;
    const float THIRD_PERSON_DISTANCE = 4.0f;

    const float MOUSE_SENSITIVITY = 0.002f;
    const float WALK_SPEED = 5.0f;
    const float SPRINT_SPEED = 8.0f;
    const float JUMP_FORCE = 8.5f;

    const float FLY_SPEED = 30.0f;
    const float SWIM_ACCELERATION = 5.0f;
    const float SWIM_UP_ACCELERATION = 16.0f;
};