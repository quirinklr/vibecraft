#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>
#include <thread>
#include <stop_token>
#include <mutex>
#include <iostream>

class ThreadPool
{
    std::vector<std::jthread> workers;
    std::mutex mtx;
    std::condition_variable_any cv;
    std::queue<std::function<void(std::stop_token)>> jobs;
    bool quit = false;

public:
    explicit ThreadPool()
    {
        size_t hw = std::thread::hardware_concurrency();
        size_t n = hw > 2 ? hw - 2 : 1;
        for (size_t i = 0; i < n; ++i)
        {
            workers.emplace_back([this](std::stop_token st)
                                 {
                                     for (;;)
                                     {
                                         std::function<void(std::stop_token)> job;
                                         {
                                             std::unique_lock lock(mtx);
                                             cv.wait(lock, st, [this]
                                                     { return quit || !jobs.empty(); });
                                             if ((quit || st.stop_requested()) && jobs.empty())
                                                 return;
                                             job = std::move(jobs.front());
                                             jobs.pop();
                                         }
                                         job(st);
                                     } });
        }
    }

    ~ThreadPool()
    {

        if (!workers.empty())
        {
            shutdown();
        }
    }

    void shutdown()
    {
        std::cout << "ThreadPool: Shutdown initiated." << std::endl;
        {
            std::scoped_lock lock(mtx);

            std::queue<std::function<void(std::stop_token)>> empty;
            jobs.swap(empty);
            quit = true;
        }
        cv.notify_all();

        workers.clear();
        std::cout << "ThreadPool: All workers joined. Shutdown complete." << std::endl;
    }

    void submit(std::function<void(std::stop_token)> f)
    {
        {
            std::scoped_lock lock(mtx);
            jobs.emplace(std::move(f));
        }
        cv.notify_one();
    }
};
