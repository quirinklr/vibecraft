#pragma once

#include <GLFW/glfw3.h>

class Engine;
class Window;

class DebugController
{
public:
    DebugController(Engine *engine);
    void handleInput(GLFWwindow *window);

private:
    Engine *m_engine;

    bool m_key_L_last_state = false;
    bool m_key_P_pressed = false;
    bool m_key_O_pressed = false;
};