#pragma once

struct Settings {
    // Maus-Einstellungen
    float mouseSensitivityX = 2.0f;
    float mouseSensitivityY = 2.0f;
    bool invertMouseY = true; // Standardmäßig invertiert, um das Problem zu beheben

    bool vsync = true;          // V-Sync standardmäßig an
    int fpsCap = 480;           // FPS-Limit, wenn V-Sync aus ist
};