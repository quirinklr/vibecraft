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

inline float wrapDegrees(float degrees)
{
    float result = fmod(degrees + 180.0f, 360.0f);
    if (result < 0.0f)
    {
        result += 360.0f;
    }
    return result - 180.0f;
}

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
    CameraMode getCameraMode() const { return m_cameraMode; }

    Camera &get_camera() { return m_camera; }

    void update_camera_interpolated(Engine *engine, float alpha);

    void updateModelRotations();

    float getNetHeadYaw() const { return m_netHeadYaw; }
    float getHeadPitch() const { return m_headPitch; }
    float getRenderYawOffset() const { return m_renderYawOffset; }

private:
    CameraMode m_cameraMode = CameraMode::FIRST_PERSON;
    Camera m_camera;

    float m_yaw = -glm::half_pi<float>();
    float m_pitch = 0.f;

    float m_cameraYaw;
    float m_cameraPitch;

    float m_netHeadYaw;
    float m_headPitch;
    float m_renderYawOffset;

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