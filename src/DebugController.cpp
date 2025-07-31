#include "DebugController.h"
#include "Engine.h"
#include "Window.h"
#include "Settings.h"

DebugController::DebugController(Engine *e) : m_engine(e) {}

void DebugController::handleInput(GLFWwindow *window)
{

    bool l_now = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (l_now && !m_key_L_last_state)
    {
        uint32_t &flags = m_engine->getSettings().rayTracingFlags;
        if (flags & SettingsEnums::SHADOWS)
        {
            flags &= ~SettingsEnums::SHADOWS;
        }
        else
        {
            flags |= SettingsEnums::SHADOWS;
        }
    }
    m_key_L_last_state = l_now;

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        m_engine->advanceTime(100);
    }

    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
    {
        m_engine->advanceTime(-100);
    }
}