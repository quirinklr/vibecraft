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
#include "Item.h"

Engine::Engine()
    : m_Window(WIDTH, HEIGHT, "Vibecraft", m_Settings),
      m_Renderer(m_Window, m_Settings, m_player_ptr, &m_TerrainGen),
      m_debugController(this)
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
    m_Pool.shutdown();
    vkDeviceWaitIdle(m_Renderer.getDeviceContext()->getDevice());
}

Block Engine::get_block(int x, int y, int z)
{
    if (y < 0 || y >= Chunk::HEIGHT)
    {
        return {BlockId::AIR};
    }

    int chunk_x = static_cast<int>(floor(static_cast<float>(x) / Chunk::WIDTH));
    int chunk_z = static_cast<int>(floor(static_cast<float>(z) / Chunk::DEPTH));

    std::shared_ptr<Chunk> chunk;
    {
        std::lock_guard<std::mutex> lock(m_ChunksMutex);
        auto it = m_Chunks.find({chunk_x, 0, chunk_z});
        if (it != m_Chunks.end())
        {
            chunk = it->second;
        }
    }

    if (chunk && chunk->getState() >= Chunk::State::TERRAIN_READY)
    {
        int local_x = x - chunk_x * Chunk::WIDTH;
        int local_z = z - chunk_z * Chunk::DEPTH;
        return chunk->getBlock(local_x, y, local_z);
    }

    return {BlockId::AIR};
}

void Engine::spawn_item(glm::vec3 position, BlockId blockId)
{
    m_items.push_back(std::make_unique<Item>(this, position, blockId));
}

void Engine::run()
{
    bool mouse_enabled = true;
    float last_time = static_cast<float>(glfwGetTime());
    double last_cursor_x, last_cursor_y;
    glfwGetCursorPos(m_Window.getGLFWwindow(), &last_cursor_x, &last_cursor_y);
    float fps_time = last_time;
    int frames = 0;

    const float FIXED_TIMESTEP = 1.0f / 60.0f;

    while (!m_Window.shouldClose())
    {
        float now = static_cast<float>(glfwGetTime());
        float dt = now - last_time;
        last_time = now;

        if (dt > 0.25f)
        {
            dt = 0.25f;
        }

        m_FrameEMA = 0.9 * m_FrameEMA + 0.1 * dt;

        glfwPollEvents();
        processInput(dt, mouse_enabled, last_cursor_x, last_cursor_y);

        m_timeAccumulator += dt;
        while (m_timeAccumulator >= 1.0f / m_ticksPerSecond)
        {
            m_gameTicks = (m_gameTicks + 1) % 24000;
            m_timeAccumulator -= 1.0f / m_ticksPerSecond;
        }

        m_physicsAccumulator += dt;
        while (m_physicsAccumulator >= FIXED_TIMESTEP)
        {
            m_player_ptr->process_keyboard(m_Window.getGLFWwindow(), FIXED_TIMESTEP);
            for (auto &entity : m_entities)
            {
                entity->update(FIXED_TIMESTEP);
            }
            for (auto &item : m_items)
            {
                item->update(FIXED_TIMESTEP);
            }
            m_physicsAccumulator -= FIXED_TIMESTEP;
        }

        const float alpha = m_physicsAccumulator / FIXED_TIMESTEP;

        m_player_ptr->update_camera_interpolated(this, alpha);

        if (m_showDebugOverlay)
        {
            float fps = 1.0f / m_FrameEMA;
            m_Renderer.getDebugOverlay()->update(*m_player_ptr, m_Settings, fps, m_TerrainGen.getSeed());
        }

        glm::vec3 hovered_block_pos_float;
        if (m_player_ptr->raycast(hovered_block_pos_float))
        {
            m_hoveredBlockPos = glm::ivec3(floor(hovered_block_pos_float.x), floor(hovered_block_pos_float.y), floor(hovered_block_pos_float.z));
        }
        else
        {
            m_hoveredBlockPos.reset();
        }

        std::vector<glm::vec3> outlineVertices;
        if (m_hoveredBlockPos)
        {
            generateBlockOutline(*m_hoveredBlockPos, outlineVertices);
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

            for (const auto &item : m_items)
            {
                debug_aabbs.push_back(item->get_world_aabb());
            }
        }

        glm::vec3 player_pos_logic = m_player_ptr->get_position();
        updateChunks(player_pos_logic);

        glm::ivec3 playerChunkPos{
            static_cast<int>(std::floor(player_pos_logic.x / Chunk::WIDTH)), 0,
            static_cast<int>(std::floor(player_pos_logic.z / Chunk::DEPTH))};

        std::unordered_map<glm::ivec3, std::shared_ptr<Chunk>, ivec3_hasher> chunks_copy;
        {
            std::lock_guard<std::mutex> lock(m_ChunksMutex);
            chunks_copy = m_Chunks;
        }

        if (!m_Renderer.drawFrame(m_player_ptr->get_camera(), player_pos_logic, chunks_copy, playerChunkPos,
                                  m_gameTicks, debug_aabbs, m_showDebugOverlay, outlineVertices, m_hoveredBlockPos, m_items))
        {
            continue;
        }
        updateWindowTitle(now, fps_time, frames, m_player_ptr->get_position());
    }
}

void Engine::advanceTime(int32_t ticks)
{
    int64_t new_ticks = (int64_t)m_gameTicks + ticks;
    new_ticks %= 24000;
    if (new_ticks < 0)
    {
        new_ticks += 24000;
    }
    m_gameTicks = (uint32_t)new_ticks;
}

void Engine::processInput(float dt, bool &mouse_enabled, double &lx, double &ly)
{
    GLFWwindow *window = m_Window.getGLFWwindow();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        mouse_enabled = false;
    }
    if (!mouse_enabled && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        mouse_enabled = true;
        glfwGetCursorPos(window, &lx, &ly);
    }

    m_debugController.handleInput(window);

    static bool fLast = false;
    bool fNow = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fNow && !fLast)
        m_Settings.wireframe = !m_Settings.wireframe;
    fLast = fNow;

    static bool cLast = false;
    bool cNow = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    if (cNow && !cLast)
        m_Settings.showCollisionBoxes = !m_Settings.showCollisionBoxes;
    cLast = cNow;

    if (mouse_enabled)
    {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        float dx = static_cast<float>(mx - lx);
        float dy = static_cast<float>(my - ly);
        lx = mx;
        ly = my;

        m_player_ptr->process_mouse_movement(dx, dy);
    }

    static bool was_mouse_pressed = false;
    bool is_mouse_pressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (is_mouse_pressed && !was_mouse_pressed && mouse_enabled)
    {
        glm::vec3 block_pos;
        if (m_player_ptr->raycast(block_pos))
        {
            set_block(
                static_cast<int>(floor(block_pos.x)),
                static_cast<int>(floor(block_pos.y)),
                static_cast<int>(floor(block_pos.z)),
                BlockId::AIR);
        }
    }
    was_mouse_pressed = is_mouse_pressed;

    static bool gLast = false;
    bool gNow = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    if (gNow && !gLast)
    {
        m_player_ptr->toggle_flight();
    }
    gLast = gNow;

    bool z_now = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
    if (z_now && !m_key_Z_last_state)
    {
        m_showDebugOverlay = !m_showDebugOverlay;
    }
    m_key_Z_last_state = z_now;

    static bool lLast = false;

    bool lNow = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (lNow && !lLast)
    {

        if (m_Settings.rayTracingFlags & SettingsEnums::SHADOWS)
        {
            m_Settings.rayTracingFlags &= ~SettingsEnums::SHADOWS;
        }
        else
        {
            if (m_Renderer.getDeviceContext()->isRayTracingSupported())
            {
                m_Settings.rayTracingFlags |= SettingsEnums::SHADOWS;
            }
        }
    }
    lLast = lNow;
}

void Engine::set_block(int x, int y, int z, BlockId id)
{
    if (y < 0 || y >= Chunk::HEIGHT)
        return;

    if (id == BlockId::AIR)
    {
        Block existing_block = get_block(x, y, z);
        if (existing_block.id != BlockId::AIR && existing_block.id != BlockId::BEDROCK)
        {
            spawn_item(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), existing_block.id);
        }
    }

    auto set_chunk_dirty = [&](int block_x, int block_z)
    {
        int chunk_x = static_cast<int>(floor(static_cast<float>(block_x) / Chunk::WIDTH));
        int chunk_z = static_cast<int>(floor(static_cast<float>(block_z) / Chunk::DEPTH));
        std::lock_guard<std::mutex> lock(m_ChunksMutex);
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
        std::lock_guard<std::mutex> lock(m_ChunksMutex);
        auto it = m_Chunks.find({chunk_x, 0, chunk_z});

        if (it != m_Chunks.end())
        {
            it->second->m_is_dirty.store(true);
        }
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

void Engine::generateBlockOutline(const glm::ivec3 &pos, std::vector<glm::vec3> &vertices)
{
    vertices.clear();

    auto add_edge = [&](const glm::vec3 &v1, const glm::vec3 &v2)
    {
        vertices.push_back(v1);
        vertices.push_back(v2);
    };

    auto check_face = [&](const glm::ivec3 &neighbor_pos, const std::vector<std::pair<glm::vec3, glm::vec3>> &edges)
    {
        Block block = get_block(neighbor_pos.x, neighbor_pos.y, neighbor_pos.z);
        if (!BlockDatabase::get().get_block_data(block.id).is_solid)
        {
            for (const auto &edge : edges)
            {
                add_edge(edge.first, edge.second);
            }
        }
    };

    const glm::vec3 v000(0, 0, 0), v100(1, 0, 0), v110(1, 1, 0), v010(0, 1, 0);
    const glm::vec3 v001(0, 0, 1), v101(1, 0, 1), v111(1, 1, 1), v011(0, 1, 1);

    const std::vector<std::pair<glm::vec3, glm::vec3>> posX_edges = {{v100, v110}, {v110, v111}, {v111, v101}, {v101, v100}};
    const std::vector<std::pair<glm::vec3, glm::vec3>> negX_edges = {{v001, v011}, {v011, v010}, {v010, v000}, {v000, v001}};
    const std::vector<std::pair<glm::vec3, glm::vec3>> posY_edges = {{v010, v110}, {v110, v111}, {v111, v011}, {v011, v010}};
    const std::vector<std::pair<glm::vec3, glm::vec3>> negY_edges = {{v001, v101}, {v101, v100}, {v100, v000}, {v000, v001}};
    const std::vector<std::pair<glm::vec3, glm::vec3>> posZ_edges = {{v101, v111}, {v111, v011}, {v011, v001}, {v001, v101}};
    const std::vector<std::pair<glm::vec3, glm::vec3>> negZ_edges = {{v000, v010}, {v010, v110}, {v110, v100}, {v100, v000}};

    check_face({pos.x + 1, pos.y, pos.z}, posX_edges);
    check_face({pos.x - 1, pos.y, pos.z}, negX_edges);
    check_face({pos.x, pos.y + 1, pos.z}, posY_edges);
    check_face({pos.x, pos.y - 1, pos.z}, negY_edges);
    check_face({pos.x, pos.y, pos.z + 1}, posZ_edges);
    check_face({pos.x, pos.y, pos.z - 1}, negZ_edges);
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
    Chunk *raw = nullptr;
    bool should_create = false;
    {
        std::lock_guard<std::mutex> lock(m_ChunksMutex);
        if (m_Chunks.find(pos) == m_Chunks.end())
        {
            auto ch = std::make_shared<Chunk>(pos);
            raw = ch.get();
            m_Chunks[pos] = std::move(ch);
            should_create = true;
        }
    }

    if (should_create && raw)
    {
        m_Pool.submit([this, raw](std::stop_token st)
                      {
            if (st.stop_requested()) return;
            m_TerrainGen.populateChunk(*raw); });
    }
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
    {
        std::lock_guard<std::mutex> lock(m_ChunksMutex);
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
}

void Engine::processGarbage()
{

    if (m_Garbage.empty())
        return;

    m_Garbage.erase(
        std::remove_if(m_Garbage.begin(), m_Garbage.end(),
                       [this](const std::shared_ptr<Chunk> &chunk)
                       {
                           if (chunk.use_count() == 1)
                           {

                               m_Renderer.scheduleChunkGpuCleanup(chunk);
                               return true;
                           }
                           return false;
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
            bool exists;
            {
                std::lock_guard<std::mutex> lock(m_ChunksMutex);
                exists = m_Chunks.count(chunkPos) > 0;
            }
            if (!exists)
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

            std::shared_ptr<Chunk> ch_ptr;
            {
                std::lock_guard<std::mutex> lock(m_ChunksMutex);
                auto it = m_Chunks.find(pos);
                if (it == m_Chunks.end())
                    continue;
                ch_ptr = it->second;
            }

            Chunk *ch = ch_ptr.get();
            if (ch->getState() == Chunk::State::INITIAL)
                continue;

            float dist = glm::distance(glm::vec2(x, z), glm::vec2(0.f));
            int reqLod = (!m_Settings.lodDistances.empty() && dist <= m_Settings.lodDistances[0]) ? 0 : 1;

            bool is_dirty = ch->m_is_dirty.load(std::memory_order_acquire);
            bool blas_is_dirty = ch->m_blas_dirty.load(std::memory_order_acquire);

            if (!ch->hasLOD(reqLod) || is_dirty)
            {
                pending.emplace_back(pos, reqLod);

                if (ch->hasLOD(reqLod))
                {
                    ch->m_blas_dirty.store(true, std::memory_order_release);
                }
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

            std::shared_ptr<Chunk> chunk_to_update;
            {
                std::lock_guard<std::mutex> chunk_lock(m_ChunksMutex);
                auto it = m_Chunks.find(job.first);
                if (it != m_Chunks.end())
                {
                    chunk_to_update = it->second;
                }
            }
            if (chunk_to_update)
            {
                chunk_to_update->m_is_dirty.store(false, std::memory_order_release);
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

        ChunkMeshInput in;
        std::shared_ptr<Chunk> selfChunk;
        {
            std::lock_guard<std::mutex> chunk_lock(m_ChunksMutex);
            auto it = m_Chunks.find(job.first);
            if (it == m_Chunks.end())
            {
                m_MeshJobsInProgress.erase(job);
                continue;
            }
            selfChunk = it->second;
        }

        in.selfChunk = selfChunk;
        glm::ivec3 p = selfChunk->getPos();

        glm::ivec3 off[8] = {{p.x - 1, 0, p.z}, {p.x + 1, 0, p.z}, {p.x, 0, p.z - 1}, {p.x, 0, p.z + 1}, {p.x - 1, 0, p.z - 1}, {p.x + 1, 0, p.z - 1}, {p.x - 1, 0, p.z + 1}, {p.x + 1, 0, p.z + 1}};
        {
            std::lock_guard<std::mutex> chunk_lock(m_ChunksMutex);
            for (int j = 0; j < 8; ++j)
            {
                auto n = m_Chunks.find(off[j]);
                if (n != m_Chunks.end() && n->second->getState() >= Chunk::State::TERRAIN_READY)
                    in.neighborChunks[j] = n->second;
            }
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

    std::vector<std::shared_ptr<Chunk>> chunks_to_process;
    {
        std::lock_guard<std::mutex> lock(m_ChunksMutex);
        chunks_to_process.reserve(m_Chunks.size());
        for (auto const &[pos, chunk] : m_Chunks)
        {
            chunks_to_process.push_back(chunk);
        }
    }

    for (auto &ch : chunks_to_process)
    {
        if (uploaded >= m_Settings.chunksToUploadPerFrame)
            break;
        if (ch->getState() != Chunk::State::STAGING_READY)
            continue;

        bool did_upload = false;
        if (ch->uploadMesh(m_Renderer, 0))
            did_upload = true;
        if (ch->uploadMesh(m_Renderer, 1))
            did_upload = true;
        if (ch->uploadTransparentMesh(m_Renderer, 0))
            did_upload = true;
        if (ch->uploadTransparentMesh(m_Renderer, 1))
            did_upload = true;

        if (did_upload)
        {
            uploaded++;
        }
    }
}