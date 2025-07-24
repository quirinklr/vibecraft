#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>
#include <thread>
#include <stop_token>
#include <mutex>

class ThreadPool
{
    std::vector<std::jthread> workers;
    std::mutex mtx;
    std::condition_variable_any cv;
    std::queue<std::function<void(std::stop_token)>> jobs;
    bool quit = false;

public:
    explicit ThreadPool(size_t n = std::thread::hardware_concurrency())
    {
        if (n == 0)
            n = 1;
        for (size_t i = 0; i < n; ++i)
        {
            workers.emplace_back([this](std::stop_token st)
                                 {
                for (;;) {
                    std::function<void(std::stop_token)> job;
                    {
                        std::unique_lock lock(mtx);
                        cv.wait(lock, st, [this] { return quit || !jobs.empty(); });
                        if ((quit || st.stop_requested()) && jobs.empty()) return;
                        job = std::move(jobs.front());
                        jobs.pop();
                    }
                    job(st);
                } });
        }
    }
    ~ThreadPool()
    {
        {
            std::scoped_lock lock(mtx);
            quit = true;
        }
        cv.notify_all();
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
