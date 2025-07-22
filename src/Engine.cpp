#include "Engine.h"
#include "FastNoiseLite.h"
#include <iostream>
#include <glm/gtc/constants.hpp>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include "math/Ivec3Less.h"
#include <iomanip>

Engine::Engine()
{
    glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(0.01f);

    const int worldSize = 8;
    for (int x = -worldSize / 2; x < worldSize / 2; ++x)
    {
        for (int z = -worldSize / 2; z < worldSize / 2; ++z)
        {
            glm::ivec3 pos = {x, 0, z};
            auto chunk = std::make_unique<Chunk>(pos);
            chunk->generateTerrain(noise);
            chunk->generateMesh(m_Renderer);
            m_Chunks[pos] = std::move(chunk);
        }
    }
}

Engine::~Engine()
{
    for (auto &pair : m_Chunks)
    {
        pair.second->cleanup(m_Renderer.getDevice());
    }
}
void Engine::run()
{
    // ────────────────────────────
    // Setup
    // ────────────────────────────
    glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    bool mouseCaptured = true;

    float lastFrameTime = static_cast<float>(glfwGetTime());

    double lastX, lastY;
    glfwGetCursorPos(m_Window.getGLFWwindow(), &lastX, &lastY);

    float yaw = -glm::half_pi<float>(); // -90°, Blick -Z
    float pitch = 0.0f;

    glm::vec3 cameraPos = {0.0f, 100.0f, 3.0f}; // ► Start über Terrain

    // Bewegungsparameter
    float baseSpeed = 10.0f;      // m/s
    const float boostMul = 4.0f;  // Sprint (Shift hält)
    const float speedStep = 2.0f; // Q/E‑Änderung pro Sek.

    // FPS‑Anzeige
    float lastFPSTime = lastFrameTime;
    int frameCount = 0;

    constexpr float mouseScale = 0.0005f;

    // ────────────────────────────
    // Haupt‑Loop
    // ────────────────────────────
    while (!m_Window.shouldClose())
    {
        float current = static_cast<float>(glfwGetTime());
        float dt = current - lastFrameTime;

        glfwPollEvents();

        /* --- Maus‑Capture toggeln --- */
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            mouseCaptured = false;
        }
        if (!mouseCaptured &&
            glfwGetMouseButton(m_Window.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            mouseCaptured = true;
            glfwGetCursorPos(m_Window.getGLFWwindow(), &lastX, &lastY);
        }

        /* --- Blickrichtung --- */
        if (mouseCaptured)
        {
            double mx, my;
            glfwGetCursorPos(m_Window.getGLFWwindow(), &mx, &my);

            float dX = static_cast<float>(mx - lastX);
            float dY = static_cast<float>(my - lastY);
            lastX = mx;
            lastY = my;

            yaw -= dX * m_Settings.mouseSensitivityX * mouseScale;
            pitch += (m_Settings.invertMouseY ? dY : -dY) * m_Settings.mouseSensitivityY * mouseScale;

            pitch = glm::clamp(pitch,
                               -glm::half_pi<float>() + 0.01f,
                               glm::half_pi<float>() - 0.01f);
        }

        /* --- Richtungs‑Vektoren --- */
        glm::vec3 forward{
            cos(yaw) * cos(pitch),
            sin(pitch),
            sin(yaw) * cos(pitch)};
        forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(glm::cross(forward, {0.f, 1.f, 0.f}));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        /* --- Laufgeschwindigkeit anpassen (E/Q) --- */
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_E) == GLFW_PRESS)
            baseSpeed += speedStep * dt * 10.f; // schneller
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_Q) == GLFW_PRESS)
            baseSpeed = glm::max(1.f, baseSpeed - speedStep * dt * 10.f); // langsamer

        float speed = baseSpeed;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            speed *= boostMul; // Sprint

        /* --- WASD Bewegung --- */
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_W) == GLFW_PRESS)
            cameraPos += forward * speed * dt;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_S) == GLFW_PRESS)
            cameraPos -= forward * speed * dt;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_A) == GLFW_PRESS)
            cameraPos -= right * speed * dt;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_D) == GLFW_PRESS)
            cameraPos += right * speed * dt;

        /* --- Kamera setzen --- */
        m_Camera.setViewDirection(cameraPos, forward, up);

        auto ext = m_Window.getExtent();
        m_Camera.setPerspectiveProjection(glm::radians(45.f),
                                          static_cast<float>(ext.width) / ext.height,
                                          0.1f, 1000.f); // Far‑Plane 1000
        
        // ----- NEU: Streaming auf Basis Kamera -----
        updateChunks(cameraPos);

        /* --- Rendern --- */
        m_Renderer.drawFrame(m_Camera, m_Chunks);

        /* --- FPS‑Anzeige --- */
        frameCount++;
        if (current - lastFPSTime >= 1.0f)
        {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(1)
               << "Minecraft Vibe Engine | FPS: " << frameCount
               << " | Yaw: " << glm::degrees(yaw)
               << " | Pitch: " << glm::degrees(pitch)
               << " | Speed: " << baseSpeed;
            glfwSetWindowTitle(m_Window.getGLFWwindow(), ss.str().c_str());
            frameCount = 0;
            lastFPSTime = current;
        }

        lastFrameTime = current;
    }
}

void Engine::generateChunk(const glm::ivec3& pos)
{
    // Doppelte vermeiden
    if (m_Chunks.find(pos) != m_Chunks.end()) return;

    static FastNoiseLite noise;            // einmalig initialisiert
    static bool noiseInit = false;
    if (!noiseInit) {
        noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        noise.SetFrequency(0.01f);
        noiseInit = true;
    }

    auto chunk = std::make_unique<Chunk>(pos);
    chunk->generateTerrain(noise);
    chunk->generateMesh(m_Renderer);
    m_Chunks[pos] = std::move(chunk);
}


// ------------------------------------------------------------
// Laufend aufrufen: lädt neue Chunks nach & räumt alte weg
// ------------------------------------------------------------
void Engine::updateChunks(const glm::vec3& cameraPos)
{
    // In welchen Chunk befindet sich die Kamera?
    glm::ivec3 camChunk = {
        static_cast<int>(std::floor(cameraPos.x / Chunk::WIDTH)),
        0,
        static_cast<int>(std::floor(cameraPos.z / Chunk::DEPTH))
    };

    // --------- 1) Chunks **nachladen** ---------
    for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; ++dz)
        for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; ++dx)
        {
            glm::ivec3 pos = camChunk + glm::ivec3{dx, 0, dz};
            generateChunk(pos);
        }

    // --------- 2) Chunks **entfernen**, die zu weit weg sind ---------
    std::vector<glm::ivec3> toRemove;
    for (auto& pair : m_Chunks)
    {
        glm::ivec3 pos = pair.first;
        int dx = pos.x - camChunk.x;
        int dz = pos.z - camChunk.z;
        if (std::max(std::abs(dx), std::abs(dz)) > RENDER_DISTANCE)
            toRemove.push_back(pos);
    }

    for (auto& pos : toRemove)
    {
        m_Chunks[pos]->cleanup(m_Renderer.getDevice());
        m_Chunks.erase(pos);
    }
}