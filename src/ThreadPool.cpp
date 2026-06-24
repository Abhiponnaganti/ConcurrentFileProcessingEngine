#include "ThreadPool.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace cfpe {

ThreadPool::ThreadPool(std::size_t workerCount)
    : queues_(workerCount), queueMutexes_(workerCount)
{
    if (workerCount == 0) {
        throw std::invalid_argument("worker count must be greater than zero");
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

ThreadPoolResult ThreadPool::process(const std::vector<ChunkDescriptor>& chunks,
                                     const ChunkProcessor& processor)
{
    shutdown();
    stopping_.store(false, std::memory_order_release);
    remainingChunks_.store(chunks.size(), std::memory_order_release);

    for (std::size_t i = 0; i < queues_.size(); ++i) {
        std::lock_guard<std::mutex> lock(queueMutexes_[i]);
        queues_[i].clear();
    }

    for (std::size_t i = 0; i < chunks.size(); ++i) {
        const std::size_t queueIndex = i % queues_.size();
        std::lock_guard<std::mutex> lock(queueMutexes_[queueIndex]);
        queues_[queueIndex].push_back(chunks[i]);
    }

    ThreadPoolResult result;
    result.threadResults.resize(workerCount());
    for (std::size_t i = 0; i < result.threadResults.size(); ++i) {
        result.threadResults[i].workerId = i;
    }

    const auto startedAt = std::chrono::steady_clock::now();

    if (!chunks.empty()) {
        workers_.reserve(workerCount());
        for (std::size_t i = 0; i < workerCount(); ++i) {
            workers_.emplace_back([this, i, &processor, &result]() {
                workerLoop(i, processor, result.threadResults[i]);
            });
        }

        workAvailable_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

    const auto finishedAt = std::chrono::steady_clock::now();
    result.elapsedMs =
        std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();

    for (const auto& threadResult : result.threadResults) {
        result.bytesProcessed += threadResult.bytesProcessed;
        result.chunksProcessed += threadResult.chunksProcessed;
        result.chunkTimings.insert(result.chunkTimings.end(),
                                   threadResult.chunkTimings.begin(),
                                   threadResult.chunkTimings.end());
    }

    return result;
}

void ThreadPool::shutdown()
{
    stopping_.store(true, std::memory_order_release);
    workAvailable_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

std::size_t ThreadPool::workerCount() const noexcept
{
    return queues_.size();
}

void ThreadPool::workerLoop(std::size_t workerIndex,
                            const ChunkProcessor& processor,
                            ThreadLocalResult& result)
{
    while (!stopping_.load(std::memory_order_acquire)) {
        ChunkDescriptor chunk;
        if (tryPopLocal(workerIndex, chunk) || trySteal(workerIndex, chunk)) {
            const auto startedAt = std::chrono::steady_clock::now();
            const ChunkResult chunkResult = processor.process(chunk);
            const auto finishedAt = std::chrono::steady_clock::now();

            for (const auto& [word, count] : chunkResult.frequencies) {
                result.frequencies[word] += count;
            }

            result.bytesProcessed += chunkResult.bytesProcessed;
            ++result.chunksProcessed;
            result.chunkTimings.push_back({
                chunk.chunkId,
                workerIndex,
                chunk.offset,
                chunk.size,
                std::chrono::duration<double, std::milli>(finishedAt - startedAt).count(),
            });

            remainingChunks_.fetch_sub(1, std::memory_order_acq_rel);
            workAvailable_.notify_all();
            continue;
        }

        if (remainingChunks_.load(std::memory_order_acquire) == 0) {
            return;
        }

        std::unique_lock<std::mutex> lock(notificationMutex_);
        workAvailable_.wait_for(lock, std::chrono::milliseconds(2));
    }
}

bool ThreadPool::tryPopLocal(std::size_t workerIndex, ChunkDescriptor& chunk)
{
    std::lock_guard<std::mutex> lock(queueMutexes_[workerIndex]);
    auto& queue = queues_[workerIndex];
    if (queue.empty()) {
        return false;
    }

    chunk = queue.back();
    queue.pop_back();
    return true;
}

bool ThreadPool::trySteal(std::size_t workerIndex, ChunkDescriptor& chunk)
{
    for (std::size_t i = 0; i < queues_.size(); ++i) {
        const std::size_t victim = (workerIndex + i + 1) % queues_.size();
        std::lock_guard<std::mutex> lock(queueMutexes_[victim]);
        auto& queue = queues_[victim];
        if (!queue.empty()) {
            chunk = queue.front();
            queue.pop_front();
            return true;
        }
    }
    return false;
}

} // namespace cfpe
