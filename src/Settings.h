#pragma once

#include <vector>

struct Settings
{
    float mouseSensitivityX = 1.0f;
    float mouseSensitivityY = 1.0f;
    bool invertMouseY = true;
    float fov = 100.0f;

    bool vsync = true;
    int fpsCap = 960;
    bool fullscreen = false;
    bool wireframe = false;

    int renderDistance = 12;
    std::vector<int> lodDistances = {8, 16, 24};

    int lod0Distance = 8;

    int chunksToCreatePerFrame = 8;
    int chunksToUploadPerFrame = 4;

    int maxMeshJobsInFlight = 2;
    int maxMeshJobsBurst = 6;
};