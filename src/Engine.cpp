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
        glm::ivec3 playerChunkPos{
            static_cast<int>(std::floor(cam.x / Chunk::WIDTH)), 0,
            static_cast<int>(std::floor(cam.z / Chunk::DEPTH))};

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

        m_Renderer.drawFrame(m_Camera, m_Chunks, playerChunkPos, m_Settings.lod0Distance);

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

void Engine::createChunkContainer(const glm::ivec3 &pos)
{
    if (m_Chunks.count(pos))
        return;

    auto ch = std::make_unique<Chunk>(pos);
    Chunk *raw = ch.get();
    m_Chunks[pos] = std::move(ch);

    m_Pool.submit([this, raw](std::stop_token st)
                  {
        if (st.stop_requested()) return;
        m_TerrainGen.populateChunk(*raw); });
}

void Engine::updateChunks(const glm::vec3 &cam)
{
    glm::ivec3 playerChunkPos{
        static_cast<int>(std::floor(cam.x / Chunk::WIDTH)), 0,
        static_cast<int>(std::floor(cam.z / Chunk::DEPTH))};

    std::vector<glm::ivec3> chunksToUnload;
    for (auto &[pos, chunk] : m_Chunks)
    {
        if (std::max(std::abs(pos.x - playerChunkPos.x), std::abs(pos.z - playerChunkPos.z)) > m_Settings.renderDistance)
        {
            chunksToUnload.push_back(pos);
        }
    }

    for (auto &pos : chunksToUnload)
    {
        m_Garbage.push_back(std::move(m_Chunks.at(pos)));
        m_Chunks.erase(pos);
    }

    m_Garbage.erase(
        std::remove_if(m_Garbage.begin(), m_Garbage.end(),
                       [this](const std::unique_ptr<Chunk> &chunk)
                       {
                           bool isReadyForCleanup = (chunk->getState() == Chunk::State::GPU_READY || chunk->getState() == Chunk::State::TERRAIN_READY);
                           if (!isReadyForCleanup)
                           {
                               return false;
                           }

                           bool isJobInProgress = false;
                           {
                               std::scoped_lock lock(m_MeshJobMutex);

                               for (int lod = 0; lod <= 1; ++lod)
                               {
                                   if (m_MeshJobsInProgress.count({chunk->getPos(), lod}))
                                   {
                                       isJobInProgress = true;
                                       break;
                                   }
                               }
                           }

                           if (!isJobInProgress)
                           {
                               chunk->cleanup(m_Renderer);
                               return true;
                           }

                           return false;
                       }),
        m_Garbage.end());

    for (int z = -m_Settings.renderDistance; z <= m_Settings.renderDistance; ++z)
    {
        for (int x = -m_Settings.renderDistance; x <= m_Settings.renderDistance; ++x)
        {
            glm::ivec3 chunkPos = playerChunkPos + glm::ivec3(x, 0, z);

            if (!m_Chunks.count(chunkPos))
            {
                createChunkContainer(chunkPos);
            }

            Chunk *chunk = m_Chunks.at(chunkPos).get();

            if (chunk->getState() == Chunk::State::INITIAL)
                continue;

            float dist = glm::distance(glm::vec2(x, z), glm::vec2(0.f));
            int requiredLod = (dist <= m_Settings.lod0Distance) ? 0 : 1;

            if (!chunk->hasLOD(requiredLod))
            {
                std::scoped_lock lock(m_MeshJobMutex);
                auto job = std::make_pair(chunkPos, requiredLod);

                if (m_MeshJobsInProgress.find(job) == m_MeshJobsInProgress.end() &&
                    m_MeshJobsToCreate.find(job) == m_MeshJobsToCreate.end())
                {
                    m_MeshJobsToCreate.insert(job);
                }
            }
        }
    }

    int jobsCreated = 0;
    while (jobsCreated < m_Settings.chunksToCreatePerFrame && !m_MeshJobsToCreate.empty())
    {
        std::pair<glm::ivec3, int> job;
        {
            std::scoped_lock lock(m_MeshJobMutex);
            job = *m_MeshJobsToCreate.begin();
            m_MeshJobsToCreate.erase(m_MeshJobsToCreate.begin());
        }

        if (!m_Chunks.count(job.first))
        {
            continue;
        }

        m_MeshJobsInProgress.insert(job);
        Chunk *chunk = m_Chunks.at(job.first).get();

        m_Pool.submit([this, chunk, job](std::stop_token st)
                      {
            if (st.stop_requested()) {
                std::scoped_lock lock(m_MeshJobMutex);
                m_MeshJobsInProgress.erase(job);
                return;
            }
            chunk->buildAndStageMesh(m_Renderer.getAllocator(), job.second);
            std::scoped_lock lock(m_MeshJobMutex);
            m_MeshJobsInProgress.erase(job); });
        jobsCreated++;
    }

    int uploaded = 0;
    for (auto &[pos, chunk] : m_Chunks)
    {
        if (uploaded >= m_Settings.chunksToUploadPerFrame)
            break;
        if (chunk->getState() == Chunk::State::STAGING_READY)
        {

            if (chunk->uploadMesh(m_Renderer, 0) || chunk->uploadMesh(m_Renderer, 1))
            {
                uploaded++;
            }
        }
    }
}