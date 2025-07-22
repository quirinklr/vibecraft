#pragma once

struct Settings
{
    // Maus-Einstellungen
    float mouseSensitivityX = 0.8f;
    float mouseSensitivityY = 0.8f;
    bool invertMouseY = true; // Standardmäßig invertiert, um das Problem zu beheben

    bool vsync = true; // V-Sync standardmäßig an
    int fpsCap = 960;
};