#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "ChunkProcessor.hpp"
#include "Types.hpp"

namespace cfpe {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t workerCount);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    [[nodiscard]] ThreadPoolResult process(const std::vector<ChunkDescriptor>& chunks,
                                           const ChunkProcessor& processor);
    void shutdown();
    [[nodiscard]] std::size_t workerCount() const noexcept;

private:
    void workerLoop(std::size_t workerIndex,
                    const ChunkProcessor& processor,
                    ThreadLocalResult& result);
    bool tryPopLocal(std::size_t workerIndex, ChunkDescriptor& chunk);
    bool trySteal(std::size_t workerIndex, ChunkDescriptor& chunk);

    std::vector<std::thread> workers_;
    std::vector<std::deque<ChunkDescriptor>> queues_;
    std::vector<std::mutex> queueMutexes_;
    std::condition_variable workAvailable_;
    std::mutex notificationMutex_;
    std::atomic<bool> stopping_{false};
    std::atomic<std::size_t> remainingChunks_{0};
};

} // namespace cfpe
