#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include "Settings.h"

class Window
{
public:
    Window(int width, int height, std::string title, const Settings &settings);
    ~Window();

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    bool shouldClose() { return glfwWindowShouldClose(m_Window); }
    VkExtent2D getExtent() { return {static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height)}; }
    GLFWwindow *getGLFWwindow() const { return m_Window; }
    void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);
    bool wasWindowResized() { return m_FramebufferResized; }
    void resetWindowResizedFlag() { m_FramebufferResized = false; }

private:
    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
    void initWindow();

    int m_Width;
    int m_Height;
    bool m_FramebufferResized = false;
    std::string m_Title;
    GLFWwindow *m_Window;
    const Settings &m_Settings;
};