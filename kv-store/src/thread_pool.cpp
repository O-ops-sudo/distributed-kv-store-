#include "thread_pool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_.store(true);
    }
    condition_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::worker_loop() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // Wait until there's work OR we're stopping
            condition_.wait(lock, [this] {
                return stop_.load() || !task_queue_.empty();
            });

            // If stopping and no work left, exit
            if (stop_.load() && task_queue_.empty()) {
                return;
            }

            task = std::move(task_queue_.front());
            task_queue_.pop();
        }

        active_count_.fetch_add(1);
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "[ThreadPool] Task threw: " << e.what() << "\n";
        }
        active_count_.fetch_sub(1);
    }
}

size_t ThreadPool::queue_size() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return task_queue_.size();
}

size_t ThreadPool::active_threads() const {
    return active_count_.load();
}
