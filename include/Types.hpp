#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace cfpe {

using WordCountMap = std::unordered_map<std::string, std::uint64_t>;

struct ChunkDescriptor {
    const char* data{nullptr};
    std::size_t size{0};
    std::size_t offset{0};
    std::size_t chunkId{0};
    std::size_t fileSize{0};

    [[nodiscard]] std::size_t endOffset() const noexcept { return offset + size; }
    [[nodiscard]] bool empty() const noexcept { return size == 0; }
};

struct ChunkResult {
    ChunkDescriptor chunk{};
    WordCountMap frequencies{};
    std::uint64_t bytesProcessed{0};
};

struct ChunkTiming {
    std::size_t chunkId{0};
    std::size_t workerId{0};
    std::size_t offset{0};
    std::size_t size{0};
    double elapsedMs{0.0};
};

struct ThreadLocalResult {
    std::size_t workerId{0};
    WordCountMap frequencies{};
    std::vector<ChunkTiming> chunkTimings{};
    std::uint64_t bytesProcessed{0};
    std::uint64_t chunksProcessed{0};
};

struct ThreadPoolResult {
    std::vector<ThreadLocalResult> threadResults{};
    std::vector<ChunkTiming> chunkTimings{};
    std::uint64_t bytesProcessed{0};
    std::uint64_t chunksProcessed{0};
    double elapsedMs{0.0};
};

struct ProcessingStats {
    std::uint64_t bytesProcessed{0};
    std::uint64_t chunksProcessed{0};
    double elapsedSeconds{0.0};

    [[nodiscard]] double throughputMiBPerSecond() const noexcept;
};

struct TopWord {
    std::string word{};
    std::uint64_t count{0};
};

struct AggregatedResult {
    WordCountMap frequencies{};
    std::vector<TopWord> topWords{};
    std::uint64_t totalWords{0};
    std::uint64_t uniqueWords{0};
    ProcessingStats stats{};
};

struct FileProcessingSummary {
    std::filesystem::path file{};
    std::uint64_t fileSizeBytes{0};
    std::size_t threadCount{0};
    std::size_t chunkSizeBytes{0};
    std::uint64_t chunksProcessed{0};
    std::uint64_t totalWords{0};
    std::uint64_t uniqueWords{0};
    double elapsedMs{0.0};
    std::vector<TopWord> topWords{};
};

enum class ProcessingStrategy {
    Sequential,
    ThreadPool,
    MMapSequential,
    MMapThreadPool
};

struct BenchmarkResult {
    ProcessingStrategy strategy{ProcessingStrategy::Sequential};
    ProcessingStats stats{};
};

} // namespace cfpe
