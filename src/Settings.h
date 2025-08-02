#pragma once

#include <vector>
#include <cstdint>

namespace SettingsEnums
{
    enum RayTracingFlags : uint32_t
    {
        SHADOWS = 1 << 0,
        REFLECTIONS = 1 << 1,
        GI = 1 << 2
    };
}

struct Settings
{
    float mouseSensitivityX = 0.4f;
    float mouseSensitivityY = 0.4f;
    bool invertMouseY = false;
    float fov = 100.0f;

    bool vsync = false;
    bool fullscreen = false;

    bool wireframe = false;
    bool showCollisionBoxes = false;
    uint32_t rayTracingFlags = 1;

    int renderDistance = 12;
    std::vector<int> lodDistances = {8, 16, 24};

    int chunksToCreatePerFrame = 8;
    int chunksToUploadPerFrame = 4;

    int maxMeshJobsInFlight = 2;
    int maxMeshJobsBurst = 6;
};