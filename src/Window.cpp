#include "Window.h"
#include <stdexcept>

Window::Window(int width, int height, std::string title, const Settings &settings)
    : m_Width{width}, m_Height{height}, m_Title{title}, m_Settings(settings)
{
    initWindow();
}

Window::~Window()
{
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Window::initWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    if (m_Settings.fullscreen)
    {
        GLFWmonitor *primary = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(primary);
        m_Window = glfwCreateWindow(mode->width, mode->height, m_Title.c_str(), primary, nullptr);
        m_Width = mode->width;
        m_Height = mode->height;
    }
    else
    {
        m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
    }
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, framebufferResizeCallback);
}

void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface)
{
    if (glfwCreateWindowSurface(instance, m_Window, nullptr, surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}

void Window::framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto app = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    app->m_FramebufferResized = true;
    app->m_Width = width;
    app->m_Height = height;
}