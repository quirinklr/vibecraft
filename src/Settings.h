#pragma once

struct Settings {
    // Maus-Einstellungen
    float mouseSensitivityX = 0.8f;
    float mouseSensitivityY = 0.8f;
    bool invertMouseY = true; // Standardmäßig invertiert, um das Problem zu beheben

    bool vsync = false;          // V-Sync standardmäßig an
    int fpsCap = 4000;           // FPS-Limit, wenn V-Sync aus ist
};