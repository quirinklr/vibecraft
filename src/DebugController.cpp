#include "DebugController.h"
#include "Engine.h"
#include "Window.h"
#include "Settings.h"

DebugController::DebugController(Engine *e) : m_engine(e) {}

void DebugController::handleInput(GLFWwindow *window)
{
    bool p_now = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (p_now && !m_key_P_last_state)
    {
        m_engine->advanceTime(500);
    }
    m_key_P_last_state = p_now;

    bool o_now = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    if (o_now && !m_key_O_last_state)
    {
        m_engine->advanceTime(-500);
    }
    m_key_O_last_state = o_now;
}