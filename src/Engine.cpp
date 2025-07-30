#include "Engine.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>
#include <glm/gtc/constants.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include <algorithm>

Engine::Engine() : m_Window(WIDTH, HEIGHT, "Vibecraft", m_Settings)
{
    glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    const int ws = 8;
    for (int x = -ws / 2; x < ws / 2; ++x)
        for (int z = -ws / 2; z < ws / 2; ++z)
            m_ChunksToGenerate.insert({x, 0, z});

    auto player = std::make_unique<Player>(this, glm::vec3(0.f, 120.f, 0.f), m_Settings);
    m_player_ptr = player.get();
    m_entities.push_back(std::move(player));
}

Engine::~Engine()
{
    for (auto &[p, c] : m_Chunks)
        c->cleanup(m_Renderer);
    for (auto &c : m_Garbage)
        c->cleanup(m_Renderer);
}

Block Engine::get_block(int x, int y, int z)
{
    if (y < 0 || y >= Chunk::HEIGHT)
    {
        return {BlockId::AIR};
    }

    int chunk_x = static_cast<int>(floor(static_cast<float>(x) / Chunk::WIDTH));
    int chunk_z = static_cast<int>(floor(static_cast<float>(z) / Chunk::DEPTH));

    auto it = m_Chunks.find({chunk_x, 0, chunk_z});
    if (it != m_Chunks.end() && it->second->getState() >= Chunk::State::TERRAIN_READY)
    {
        int local_x = x - chunk_x * Chunk::WIDTH;
        int local_z = z - chunk_z * Chunk::DEPTH;
        return it->second->getBlock(local_x, y, local_z);
    }

    return {BlockId::AIR};
}

void Engine::run()
{
    bool mouse_enabled = true;
    float last_time = static_cast<float>(glfwGetTime());
    double last_cursor_x, last_cursor_y;
    glfwGetCursorPos(m_Window.getGLFWwindow(), &last_cursor_x, &last_cursor_y);
    float fps_time = last_time;
    int frames = 0;

    while (!m_Window.shouldClose())
    {
        float now = static_cast<float>(glfwGetTime());
        float dt = now - last_time;
        m_FrameEMA = 0.9 * m_FrameEMA + 0.1 * dt;
        last_time = now;

        glfwPollEvents();

        processInput(dt, mouse_enabled, last_cursor_x, last_cursor_y);

        for (auto &entity : m_entities)
        {
            entity->update(dt);
        }

        std::vector<AABB> debug_aabbs;
        if (m_Settings.showCollisionBoxes)
        {

            debug_aabbs.push_back(m_player_ptr->get_world_aabb());

            glm::vec3 p_pos = m_player_ptr->get_position();
            const int radius = 4;
            for (int y = static_cast<int>(p_pos.y) - radius; y < static_cast<int>(p_pos.y) + radius; ++y)
            {
                for (int x = static_cast<int>(p_pos.x) - radius; x < static_cast<int>(p_pos.x) + radius; ++x)
                {
                    for (int z = static_cast<int>(p_pos.z) - radius; z < static_cast<int>(p_pos.z) + radius; ++z)
                    {
                        Block block = get_block(x, y, z);
                        if (BlockDatabase::get().get_block_data(block.id).is_solid)
                        {
                            debug_aabbs.push_back({glm::vec3(x, y, z), glm::vec3(x + 1, y + 1, z + 1)});
                        }
                    }
                }
            }
        }

        glm::vec3 player_pos = m_player_ptr->get_position();
        updateChunks(player_pos);

        glm::ivec3 playerChunkPos{
            static_cast<int>(std::floor(player_pos.x / Chunk::WIDTH)), 0,
            static_cast<int>(std::floor(player_pos.z / Chunk::DEPTH))};

        m_Renderer.drawFrame(m_player_ptr->get_camera(), m_Chunks, playerChunkPos, m_Settings, debug_aabbs);

        updateWindowTitle(now, fps_time, frames, m_player_ptr->get_position());
    }
}

void Engine::processInput(float dt, bool &mouse_enabled, double &lx, double &ly)
{
    if (glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        mouse_enabled = false;
    }
    if (!mouse_enabled && glfwGetMouseButton(m_Window.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        glfwSetInputMode(m_Window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        mouse_enabled = true;
        glfwGetCursorPos(m_Window.getGLFWwindow(), &lx, &ly);
    }

    static bool fLast = false;
    bool fNow = glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_F) == GLFW_PRESS;
    if (fNow && !fLast)
        m_Settings.wireframe = !m_Settings.wireframe;
    fLast = fNow;

    static bool cLast = false;
    bool cNow = glfwGetKey(m_Window.getGLFWwindow(), GLFW_KEY_C) == GLFW_PRESS;
    if (cNow && !cLast)
        m_Settings.showCollisionBoxes = !m_Settings.showCollisionBoxes;
    cLast = cNow;

    if (mouse_enabled)
    {
        double mx, my;
        glfwGetCursorPos(m_Window.getGLFWwindow(), &mx, &my);
        float dx = static_cast<float>(mx - lx);
        float dy = static_cast<float>(my - ly);
        lx = mx;
        ly = my;

        m_player_ptr->process_mouse_movement(dx, dy);
    }

    static bool was_mouse_pressed = false;
    bool is_mouse_pressed = glfwGetMouseButton(m_Window.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (is_mouse_pressed && !was_mouse_pressed && mouse_enabled)
    {
        glm::vec3 block_pos;
        if (m_player_ptr->raycast(block_pos))
        {
            set_block(
                static_cast<int>(block_pos.x),
                static_cast<int>(block_pos.y),
                static_cast<int>(block_pos.z),
                BlockId::AIR);
        }
    }
    was_mouse_pressed = is_mouse_pressed;

    m_player_ptr->process_keyboard(m_Window.getGLFWwindow());
}

void Engine::set_block(int x, int y, int z, BlockId id)
{
    if (y < 0 || y >= Chunk::HEIGHT)
        return;

    auto set_chunk_dirty = [&](int block_x, int block_z)
    {
        int chunk_x = static_cast<int>(floor(static_cast<float>(block_x) / Chunk::WIDTH));
        int chunk_z = static_cast<int>(floor(static_cast<float>(block_z) / Chunk::DEPTH));
        auto it = m_Chunks.find({chunk_x, 0, chunk_z});
        if (it != m_Chunks.end())
        {
            it->second->setBlock(block_x - chunk_x * Chunk::WIDTH, y, block_z - chunk_z * Chunk::DEPTH, {id});
        }
    };

    set_chunk_dirty(x, z);

    int local_x = x % Chunk::WIDTH;
    if (local_x < 0)
        local_x += Chunk::WIDTH;
    int local_z = z % Chunk::DEPTH;
    if (local_z < 0)
        local_z += Chunk::DEPTH;

    auto mark_neighbor_dirty = [&](int nx, int nz)
    {
        int chunk_x = static_cast<int>(floor(static_cast<float>(nx) / Chunk::WIDTH));
        int chunk_z = static_cast<int>(floor(static_cast<float>(nz) / Chunk::DEPTH));
        auto it = m_Chunks.find({chunk_x, 0, chunk_z});
        if (it != m_Chunks.end())
            it->second->m_is_dirty.store(true);
    };

    if (local_x == 0)
        mark_neighbor_dirty(x - 1, z);
    if (local_x == Chunk::WIDTH - 1)
        mark_neighbor_dirty(x + 1, z);
    if (local_z == 0)
        mark_neighbor_dirty(x, z - 1);
    if (local_z == Chunk::DEPTH - 1)
        mark_neighbor_dirty(x, z + 1);
}

void Engine::updateWindowTitle(float now, float &fpsTime, int &frames, const glm::vec3 &player_pos)
{
    frames++;
    if (now - fpsTime >= 1.f)
    {
        std::stringstream s;
        s << std::fixed << std::setprecision(1) << "Vibecraft | FPS: " << frames
          << " | Pos: " << player_pos.x << ", " << player_pos.y << ", " << player_pos.z;
        glfwSetWindowTitle(m_Window.getGLFWwindow(), s.str().c_str());
        frames = 0;
        fpsTime = now;
    }
}

void Engine::createChunkContainer(const glm::ivec3 &pos)
{
    if (m_Chunks.count(pos))
        return;

    auto ch = std::make_shared<Chunk>(pos);
    Chunk *raw = ch.get();
    m_Chunks[pos] = std::move(ch);

    m_Pool.submit([this, raw](std::stop_token st)
                  {
        if (st.stop_requested()) return;
        m_TerrainGen.populateChunk(*raw); });
}

void Engine::updateChunks(const glm::vec3 &cam_pos)
{
    glm::ivec3 playerChunkPos{
        static_cast<int>(std::floor(cam_pos.x / Chunk::WIDTH)), 0,
        static_cast<int>(std::floor(cam_pos.z / Chunk::DEPTH))};

    unloadDistantChunks(playerChunkPos);
    processGarbage();
    loadVisibleChunks(playerChunkPos);
    createMeshJobs(playerChunkPos);
    submitMeshJobs();
    uploadReadyMeshes();
}

void Engine::unloadDistantChunks(const glm::ivec3 &playerChunkPos)
{
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
}

void Engine::processGarbage()
{
    std::lock_guard lockJobs(m_MeshJobsMutex);
    m_Garbage.erase(
        std::remove_if(m_Garbage.begin(), m_Garbage.end(),
                       [this](const std::shared_ptr<Chunk> &chunk)
                       {
                           if (chunk.use_count() > 1)
                               return false;
                           Chunk::State st = chunk->getState();
                           if (st != Chunk::State::GPU_READY && st != Chunk::State::TERRAIN_READY)
                               return false;
                           for (int lod = 0; lod <= 1; ++lod)
                               if (m_MeshJobsInProgress.count({chunk->getPos(), lod}))
                                   return false;
                           chunk->cleanup(m_Renderer);
                           return true;
                       }),
        m_Garbage.end());
}

void Engine::loadVisibleChunks(const glm::ivec3 &playerChunkPos)
{
    std::vector<glm::ivec3> chunk_positions_to_load;

    for (int z = -m_Settings.renderDistance; z <= m_Settings.renderDistance; ++z)
    {
        for (int x = -m_Settings.renderDistance; x <= m_Settings.renderDistance; ++x)
        {
            glm::ivec3 chunkPos = playerChunkPos + glm::ivec3(x, 0, z);
            if (!m_Chunks.count(chunkPos))
            {
                chunk_positions_to_load.push_back(chunkPos);
            }
        }
    }

    glm::vec2 player_pos_2d(playerChunkPos.x, playerChunkPos.z);
    std::sort(chunk_positions_to_load.begin(), chunk_positions_to_load.end(),
              [&](const glm::ivec3 &a, const glm::ivec3 &b)
              {
                  float dist_a = glm::distance2(glm::vec2(a.x, a.z), player_pos_2d);
                  float dist_b = glm::distance2(glm::vec2(b.x, b.z), player_pos_2d);
                  return dist_a < dist_b;
              });

    int chunks_created_this_frame = 0;
    for (const auto &pos : chunk_positions_to_load)
    {
        if (chunks_created_this_frame >= m_Settings.chunksToCreatePerFrame)
        {
            break;
        }
        createChunkContainer(pos);
        chunks_created_this_frame++;
    }
}

void Engine::createMeshJobs(const glm::ivec3 &playerChunkPos)
{
    std::vector<std::pair<glm::ivec3, int>> pending;

    for (int z = -m_Settings.renderDistance; z <= m_Settings.renderDistance; ++z)
    {
        for (int x = -m_Settings.renderDistance; x <= m_Settings.renderDistance; ++x)
        {
            glm::ivec3 pos = playerChunkPos + glm::ivec3(x, 0, z);
            auto it = m_Chunks.find(pos);
            if (it == m_Chunks.end())
                continue;

            Chunk *ch = it->second.get();
            if (ch->getState() == Chunk::State::INITIAL)
                continue;

            float dist = glm::distance(glm::vec2(x, z), glm::vec2(0.f));
            int reqLod = (!m_Settings.lodDistances.empty() && dist <= m_Settings.lodDistances[0]) ? 0 : 1;

            bool is_dirty = ch->m_is_dirty.load(std::memory_order_acquire);

            if (!ch->hasLOD(reqLod) || is_dirty)
            {
                pending.emplace_back(pos, reqLod);
            }
            if (is_dirty && !ch->hasLOD(1 - reqLod))
            {
                pending.emplace_back(pos, 1 - reqLod);
            }
        }
    }

    std::lock_guard lock(m_MeshJobsMutex);
    for (auto &job : pending)
    {
        if (!m_MeshJobsInProgress.count(job) && !m_MeshJobsToCreate.count(job))
        {
            m_MeshJobsToCreate.insert(job);

            auto it = m_Chunks.find(job.first);
            if (it != m_Chunks.end())
            {
                it->second->m_is_dirty.store(false, std::memory_order_release);
            }
        }
    }
}

void Engine::submitMeshJobs()
{
    int dynCap;
    if (m_FrameEMA < 0.0036f)
        dynCap = m_Settings.maxMeshJobsBurst;
    else if (m_FrameEMA < 0.0042f)
        dynCap = m_Settings.maxMeshJobsInFlight + 2;
    else
        dynCap = m_Settings.maxMeshJobsInFlight;

    std::scoped_lock lock(m_MeshJobsMutex);
    if (m_MeshJobsToCreate.empty() || static_cast<int>(m_MeshJobsInProgress.size()) >= dynCap)
    {
        return;
    }

    glm::vec3 player_pos = m_player_ptr->get_position();
    glm::vec2 player_chunk_pos_2d(
        floor(player_pos.x / Chunk::WIDTH),
        floor(player_pos.z / Chunk::DEPTH));

    std::vector<std::pair<glm::ivec3, int>> sorted_jobs;
    sorted_jobs.assign(m_MeshJobsToCreate.begin(), m_MeshJobsToCreate.end());

    std::sort(sorted_jobs.begin(), sorted_jobs.end(),
              [&](const auto &a, const auto &b)
              {
                  if (a.second != b.second)
                  {
                      return a.second < b.second;
                  }

                  float dist_a = glm::distance2(glm::vec2(a.first.x, a.first.z), player_chunk_pos_2d);
                  float dist_b = glm::distance2(glm::vec2(b.first.x, b.first.z), player_chunk_pos_2d);
                  return dist_a < dist_b;
              });

    int slots_to_fill = dynCap - static_cast<int>(m_MeshJobsInProgress.size());
    for (int i = 0; i < slots_to_fill && i < sorted_jobs.size(); ++i)
    {
        auto &job = sorted_jobs[i];

        m_MeshJobsToCreate.erase(job);
        m_MeshJobsInProgress.insert(job);

        auto it = m_Chunks.find(job.first);
        if (it == m_Chunks.end())
        {
            m_MeshJobsInProgress.erase(job);
            continue;
        }

        ChunkMeshInput in;
        in.selfChunk = it->second;
        glm::ivec3 p = it->second->getPos();

        glm::ivec3 off[8] = {{p.x - 1, 0, p.z}, {p.x + 1, 0, p.z}, {p.x, 0, p.z - 1}, {p.x, 0, p.z + 1}, {p.x - 1, 0, p.z - 1}, {p.x + 1, 0, p.z - 1}, {p.x - 1, 0, p.z + 1}, {p.x + 1, 0, p.z + 1}};
        for (int j = 0; j < 8; ++j)
        {
            auto n = m_Chunks.find(off[j]);
            if (n != m_Chunks.end() && n->second->getState() >= Chunk::State::TERRAIN_READY)
                in.neighborChunks[j] = n->second;
        }

        m_Pool.submit(
            [this, job, in = std::move(in)](std::stop_token st) mutable
            {
                if (st.stop_requested())
                {
                    std::lock_guard lk(m_MeshJobsMutex);
                    m_MeshJobsInProgress.erase(job);
                    return;
                }
                in.selfChunk->buildAndStageMesh(m_Renderer.getAllocator(), *m_Renderer.getArena(), job.second, in);
                std::lock_guard lk(m_MeshJobsMutex);
                m_MeshJobsInProgress.erase(job);
            });
    }
}

void Engine::uploadReadyMeshes()
{
    int uploaded = 0;
    for (auto &[pos, ch] : m_Chunks)
    {
        if (uploaded >= m_Settings.chunksToUploadPerFrame)
            break;
        if (ch->getState() != Chunk::State::STAGING_READY)
            continue;

        ch->uploadMesh(m_Renderer, 0);
        ch->uploadMesh(m_Renderer, 1);
        ++uploaded;
    }
}
