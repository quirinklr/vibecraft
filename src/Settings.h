#pragma once

#include <vector>

struct Settings
{

    float mouseSensitivityX = 0.8f;
    float mouseSensitivityY = 0.8f;
    bool invertMouseY = true;
    float fov = 90.0f;

    bool vsync = true;
    int fpsCap = 960;
    bool fullscreen = false;
    bool wireframe = false;

    int renderDistance = 24;
    std::vector<int> lodDistances = { 8, 16, 24 };

    int lod0Distance = 8;
    int chunksToCreatePerFrame = 8;
    int chunksToUploadPerFrame = 4;
};
