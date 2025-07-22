#include "Window.h"
#include <stdexcept>

Window::Window(int width, int height, std::string title)
    : m_Width{width}, m_Height{height}, m_Title{title}
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
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Kein OpenGL Kontext
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // Größenänderung vorerst deaktiviert

    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
}

void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface)
{
    if (glfwCreateWindowSurface(instance, m_Window, nullptr, surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}