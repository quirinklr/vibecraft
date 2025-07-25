#include "Engine.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>
#include <glm/gtc/constants.hpp>
#include <algorithm>

Engine::Engine() : m_Window(WIDTH, HEIGHT, "Vibecraft", m_Settings)
{
    glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    const int ws = 8;
    for (int x = -ws / 2; x < ws / 2; ++x)
        for (int z = -ws / 2; z < ws / 2; ++z)
            m_ChunksToGenerate.insert({x, 0, z});
}

Engine::~Engine()
{
    for (auto &[p, c] : m_Chunks)
        c->cleanup(m_Renderer);
    for (auto &c : m_Garbage)
        c->cleanup(m_Renderer);
}

void Engine::run()
{
    glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    bool mouse = true;
    float last = static_cast<float>(glfwGetTime());
    double lx, ly;
    glfwGetCursorPos(m_Window.getGLFWwindow(), &lx, &ly);
    float yaw = -glm::half_pi<float>();
    float pitch = 0.f;
    glm::vec3 cam = {0.f, 100.f, 3.f};
    float baseSpeed = 10.f;
    const float boost = 4.f;
    const float step = 2.f;
    float fpsTime = last;
    int frames = 0;
    const float mScale = 0.0005f;
    while (!m_Window.shouldClose())
    {
        float now = static_cast<float>(glfwGetTime());
        float dt = now - last;
        glfwPollEvents();
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            mouse = false;
        }
        if (!mouse && glfwGetMouseButton(m_Window.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            mouse = true;
            glfwGetCursorPos(m_Window.getGLFWwindow(), &lx, &ly);
        }
        static bool fLast = false;
        bool fNow = glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_F) == GLFW_PRESS;
        if (fNow && !fLast)
            m_Settings.wireframe = !m_Settings.wireframe;
        fLast = fNow;
        if (mouse)
        {
            double mx, my;
            glfwGetCursorPos(m_Window.getGLFWwindow(), &mx, &my);
            float dx = static_cast<float>(mx - lx);
            float dy = static_cast<float>(my - ly);
            lx = mx;
            ly = my;
            yaw -= dx * m_Settings.mouseSensitivityX * mScale;
            pitch += (m_Settings.invertMouseY ? dy : -dy) * m_Settings.mouseSensitivityY * mScale;
            pitch = glm::clamp(pitch, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
        }
        glm::vec3 f{cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch)};
        f = glm::normalize(f);
        glm::vec3 r = glm::normalize(glm::cross(f, {0.f, 1.f, 0.f}));
        glm::vec3 u = glm::normalize(glm::cross(r, f));
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_E) == GLFW_PRESS)
            baseSpeed += step * dt * 10.f;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_Q) == GLFW_PRESS)
            baseSpeed = glm::max(1.f, baseSpeed - step * dt * 10.f);
        float speed = baseSpeed;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            speed *= boost;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_W) == GLFW_PRESS)
            cam -= f * speed * dt;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_S) == GLFW_PRESS)
            cam += f * speed * dt;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_A) == GLFW_PRESS)
            cam -= r * speed * dt;
        if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_D) == GLFW_PRESS)
            cam += r * speed * dt;
        m_Camera.setViewDirection(cam, f, u);
        auto ext = m_Window.getExtent();
        m_Camera.setPerspectiveProjection(glm::radians(m_Settings.fov), static_cast<float>(ext.width) / ext.height, 0.1f, 1000.f);

        updateChunks(cam);

        m_Renderer.drawFrame(m_Camera, m_Chunks);
        frames++;
        if (now - fpsTime >= 1.f)
        {
            std::stringstream s;
            s << std::fixed << std::setprecision(1) << "Vibecraft | FPS: " << frames << " | Yaw: " << glm::degrees(yaw) << " | Pitch: " << glm::degrees(pitch) << " | Speed: " << baseSpeed;
            glfwSetWindowTitle(m_Window.getGLFWwindow(), s.str().c_str());
            frames = 0;
            fpsTime = now;
        }
        last = now;
    }
}

void Engine::generateChunk(const glm::ivec3 &pos)
{
    if (m_Chunks.count(pos))
        return;

    auto ch = std::make_unique<Chunk>(pos);
    Chunk *raw = ch.get();
    m_Chunks[pos] = std::move(ch);

    m_Pool.submit([this, raw](std::stop_token st)
                  {
        m_TerrainGen.populateChunk(*raw);
        if (st.stop_requested()) return;
        
        
        raw->buildAndStageMesh(m_Renderer.getAllocator()); });
}

void Engine::updateChunks(const glm::vec3 &cam)
{
    glm::ivec3 cc{static_cast<int>(std::floor(cam.x / Chunk::WIDTH)), 0, static_cast<int>(std::floor(cam.z / Chunk::DEPTH))};

    for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; ++dz)
    {
        for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; ++dx)
        {
            glm::ivec3 chunkPos = cc + glm::ivec3{dx, 0, dz};

            if (m_Chunks.find(chunkPos) == m_Chunks.end())
            {
                std::scoped_lock lock(m_ChunkGenerationQueueMtx);
                m_ChunksToGenerate.insert(chunkPos);
            }
        }
    }

    std::vector<glm::ivec3> out;
    for (auto &[p, c] : m_Chunks)
        if (std::max(std::abs(p.x - cc.x), std::abs(p.z - cc.z)) > RENDER_DISTANCE)
            out.push_back(p);

    for (auto &p : out)
    {
        m_Garbage.push_back(std::move(m_Chunks[p]));
        m_Chunks.erase(p);
    }

    int cleaned = 0;
    for (auto it = m_Garbage.begin(); it != m_Garbage.end() && cleaned < 1;)
    {
        if ((*it)->isReady())
        {
            (*it)->cleanup(m_Renderer);
            it = m_Garbage.erase(it);
            ++cleaned;
        }
        else
        {
            ++it;
        }
    }

    int createdThisFrame = 0;
    while (createdThisFrame < CHUNKS_TO_CREATE_PER_FRAME && !m_ChunksToGenerate.empty())
    {
        glm::ivec3 posToGen;
        {
            std::scoped_lock lock(m_ChunkGenerationQueueMtx);
            posToGen = *m_ChunksToGenerate.begin();
            m_ChunksToGenerate.erase(m_ChunksToGenerate.begin());
        }

        generateChunk(posToGen);
        createdThisFrame++;
    }

    int uploaded = 0;
    for (auto &[p, c] : m_Chunks)
    {
        if (uploaded >= 2)
            break;
        if (c->uploadMesh(m_Renderer))
            ++uploaded;
    }
}