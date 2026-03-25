#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <stdexcept>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Submit task, returns future for result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    size_t queue_size() const;
    size_t active_threads() const;
    void shutdown();

private:
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> task_queue_;
    mutable std::mutex                queue_mutex_;
    std::condition_variable           condition_;
    std::atomic<bool>                 stop_{false};
    std::atomic<size_t>               active_count_{0};

    void worker_loop();
};

// Template implementation must be in header
template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using ReturnType = typename std::invoke_result<F, Args...>::type;

    // Package the task
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<ReturnType> result = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_.load()) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        task_queue_.emplace([task]() { (*task)(); });
    }

    condition_.notify_one();
    return result;
}
