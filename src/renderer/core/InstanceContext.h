#pragma once

#include "../../VulkanWrappers.h"
#include "../../Window.h"

class InstanceContext {
public:
    InstanceContext(Window& window);
    ~InstanceContext();

    VkInstance getInstance() const { return m_Instance.get(); }
    VkSurfaceKHR getSurface() const { return m_Surface.get(); }

private:
    void createInstance();
    void createSurface();

    Window& m_Window;
    VulkanHandle<VkInstance, InstanceDeleter> m_Instance;
    VulkanHandle<VkSurfaceKHR, SurfaceDeleter> m_Surface;
    
    const std::vector<const char *> m_ValidationLayers{"VK_LAYER_KHRONOS_validation"};
    const bool m_EnableValidationLayers = true;
};
