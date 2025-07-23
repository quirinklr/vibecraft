#pragma once

struct Settings
{
    // Maus-Einstellungen
    float mouseSensitivityX = 0.8f;
    float mouseSensitivityY = 0.8f;
    bool invertMouseY = true; // Standardmäßig invertiert, um das Problem zu beheben

    bool vsync = true; // V-Sync standardmäßig an
    int fpsCap = 960;
    float fov = 100.0f;
    bool fullscreen = true;
};