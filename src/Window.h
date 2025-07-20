#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

class Window {
public:
    Window(int width, int height, std::string title);
    ~Window();

    // Verhindert das Kopieren des Fensters
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() { return glfwWindowShouldClose(m_Window); }
    VkExtent2D getExtent() { return { static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height) }; }
    GLFWwindow* getGLFWwindow() const { return m_Window; }
    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);


private:
    void initWindow();
    
    int m_Width;
    int m_Height;
    std::string m_Title;
    GLFWwindow* m_Window;
};