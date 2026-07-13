#include "ChunkProcessor.hpp"
#include "ChunkSplitter.hpp"
#include "FileMapper.hpp"
#include "ThreadPool.hpp"
#include "Types.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kMiB = 1024 * 1024;
constexpr std::size_t kDefaultChunkSize = 1024 * 1024;
constexpr std::size_t kDefaultThreadCount = 4;
constexpr int64_t kBenchmarkFileSizeMiB = 256;

struct RunMetrics {
    std::uint64_t bytesProcessed{0};
    std::uint64_t chunksProcessed{0};
    double runtimeMs{0.0};
    std::vector<double> chunkLatencyMs{};
};

std::filesystem::path benchmarkFilePath(std::size_t sizeBytes)
{
    return std::filesystem::temp_directory_path() /
           ("cfpe_benchmark_" + std::to_string(sizeBytes) + ".txt");
}

std::filesystem::path ensureBenchmarkFile(std::size_t sizeBytes)
{
    const auto path = benchmarkFilePath(sizeBytes);
    if (std::filesystem::exists(path) && std::filesystem::file_size(path) == sizeBytes) {
        return path;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to create benchmark fixture: " + path.string());
    }

    const std::string line =
        "INFO worker42 processed request_id=2026 cache_hit=true latency_ms=12 "
        "systems programming mmap threadpool aggregation benchmark\n";

    std::size_t written = 0;
    while (written < sizeBytes) {
        const std::size_t remaining = sizeBytes - written;
        const std::size_t count = std::min(remaining, line.size());
        output.write(line.data(), static_cast<std::streamsize>(count));
        written += count;
    }

    return path;
}

std::string readWithFread(const std::filesystem::path& path)
{
    using FilePtr = std::unique_ptr<FILE, decltype(&std::fclose)>;
    FilePtr file(std::fopen(path.c_str(), "rb"), &std::fclose);
    if (!file) {
        throw std::runtime_error("fopen failed for benchmark file: " + path.string());
    }

    if (std::fseek(file.get(), 0, SEEK_END) != 0) {
        throw std::runtime_error("fseek failed for benchmark file: " + path.string());
    }

    const long size = std::ftell(file.get());
    if (size < 0) {
        throw std::runtime_error("ftell failed for benchmark file: " + path.string());
    }

    std::rewind(file.get());

    std::string contents(static_cast<std::size_t>(size), '\0');
    const std::size_t read = std::fread(contents.data(), 1, contents.size(), file.get());

    if (read != contents.size()) {
        throw std::runtime_error("fread did not read complete benchmark file");
    }

    return contents;
}

RunMetrics processSequential(const std::vector<cfpe::ChunkDescriptor>& chunks,
                             const cfpe::ChunkProcessor& processor)
{
    RunMetrics metrics;
    metrics.chunkLatencyMs.reserve(chunks.size());

    const auto startedAt = std::chrono::steady_clock::now();
    for (const auto& chunk : chunks) {
        const auto chunkStartedAt = std::chrono::steady_clock::now();
        const auto result = processor.process(chunk);
        const auto chunkFinishedAt = std::chrono::steady_clock::now();

        metrics.bytesProcessed += result.bytesProcessed;
        ++metrics.chunksProcessed;
        metrics.chunkLatencyMs.push_back(
            std::chrono::duration<double, std::milli>(chunkFinishedAt - chunkStartedAt).count());
    }
    const auto finishedAt = std::chrono::steady_clock::now();
    metrics.runtimeMs =
        std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();
    return metrics;
}

RunMetrics processFread(const std::filesystem::path& path, std::size_t chunkSize)
{
    const cfpe::ChunkSplitter splitter(chunkSize);
    const cfpe::ChunkProcessor processor;

    const auto startedAt = std::chrono::steady_clock::now();
    const std::string contents = readWithFread(path);
    RunMetrics metrics = processSequential(splitter.split(contents.data(), contents.size()),
                                           processor);
    const auto finishedAt = std::chrono::steady_clock::now();
    metrics.runtimeMs =
        std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();
    return metrics;
}

RunMetrics processMMapSequential(const std::filesystem::path& path, std::size_t chunkSize)
{
    const cfpe::ChunkSplitter splitter(chunkSize);
    const cfpe::ChunkProcessor processor;

    const auto startedAt = std::chrono::steady_clock::now();
    cfpe::FileMapper mapper(path);
    RunMetrics metrics = processSequential(splitter.split(mapper.data(), mapper.size()),
                                           processor);
    const auto finishedAt = std::chrono::steady_clock::now();
    metrics.runtimeMs =
        std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();
    return metrics;
}

RunMetrics processMMapThreaded(const std::filesystem::path& path,
                               std::size_t chunkSize,
                               std::size_t threadCount)
{
    const cfpe::ChunkSplitter splitter(chunkSize);
    const cfpe::ChunkProcessor processor;

    const auto startedAt = std::chrono::steady_clock::now();
    cfpe::FileMapper mapper(path);
    cfpe::ThreadPool pool(threadCount);
    const cfpe::ThreadPoolResult result = pool.process(splitter.split(mapper.data(), mapper.size()),
                                                       processor);
    const auto finishedAt = std::chrono::steady_clock::now();

    RunMetrics metrics;
    metrics.bytesProcessed = result.bytesProcessed;
    metrics.chunksProcessed = result.chunksProcessed;
    metrics.runtimeMs = std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();
    metrics.chunkLatencyMs.reserve(result.chunkTimings.size());
    for (const auto& timing : result.chunkTimings) {
        metrics.chunkLatencyMs.push_back(timing.elapsedMs);
    }
    return metrics;
}

double percentile(std::vector<double> values, double p)
{
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    const double rank = (p / 100.0) * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(rank);
    const auto upper = std::min(lower + 1, values.size() - 1);
    const double fraction = rank - static_cast<double>(lower);
    return values[lower] + (values[upper] - values[lower]) * fraction;
}

double throughputMiBPerSecond(const RunMetrics& metrics)
{
    if (metrics.runtimeMs <= 0.0) {
        return 0.0;
    }
    return (static_cast<double>(metrics.bytesProcessed) / static_cast<double>(kMiB)) /
           (metrics.runtimeMs / 1000.0);
}

using BaselineKey = std::pair<std::size_t, std::size_t>;

struct BaselineKeyHash {
    std::size_t operator()(const BaselineKey& key) const noexcept
    {
        return key.first ^ (key.second + 0x9e3779b97f4a7c15ULL + (key.first << 6) + (key.first >> 2));
    }
};

double freadBaselineRuntimeMs(const std::filesystem::path& path,
                              std::size_t sizeBytes,
                              std::size_t chunkSize)
{
    static std::mutex mutex;
    static std::unordered_map<BaselineKey, double, BaselineKeyHash> cache;

    const BaselineKey key{sizeBytes, chunkSize};
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = cache.find(key);
        if (found != cache.end()) {
            return found->second;
        }
    }

    const RunMetrics baseline = processFread(path, chunkSize);
    std::lock_guard<std::mutex> lock(mutex);
    cache[key] = baseline.runtimeMs;
    return baseline.runtimeMs;
}

void setCounters(benchmark::State& state,
                 const RunMetrics& metrics,
                 std::size_t sizeBytes,
                 double baselineRuntimeMs)
{
    const double speedup = metrics.runtimeMs > 0.0 ? baselineRuntimeMs / metrics.runtimeMs : 0.0;

    state.counters["file_size_mb"] =
        benchmark::Counter(static_cast<double>(sizeBytes) / static_cast<double>(kMiB));
    state.counters["total_runtime_ms"] = benchmark::Counter(metrics.runtimeMs);
    state.counters["mb_per_second"] = benchmark::Counter(throughputMiBPerSecond(metrics));
    state.counters["speedup_over_fread"] = benchmark::Counter(speedup);
    state.counters["p50_chunk_latency_ms"] = benchmark::Counter(percentile(metrics.chunkLatencyMs, 50.0));
    state.counters["p95_chunk_latency_ms"] = benchmark::Counter(percentile(metrics.chunkLatencyMs, 95.0));
    state.counters["p99_chunk_latency_ms"] = benchmark::Counter(percentile(metrics.chunkLatencyMs, 99.0));
}

void BM_FreadSingleThreaded(benchmark::State& state)
{
    const auto sizeBytes = static_cast<std::size_t>(state.range(0)) * kMiB;
    const auto path = ensureBenchmarkFile(sizeBytes);
    RunMetrics metrics;

    for (auto _ : state) {
        metrics = processFread(path, kDefaultChunkSize);
        benchmark::DoNotOptimize(metrics.bytesProcessed);
    }

    setCounters(state, metrics, sizeBytes, metrics.runtimeMs);
}

void BM_MMapSingleThreaded(benchmark::State& state)
{
    const auto sizeBytes = static_cast<std::size_t>(state.range(0)) * kMiB;
    const auto path = ensureBenchmarkFile(sizeBytes);
    const double baselineMs = freadBaselineRuntimeMs(path, sizeBytes, kDefaultChunkSize);
    RunMetrics metrics;

    for (auto _ : state) {
        metrics = processMMapSequential(path, kDefaultChunkSize);
        benchmark::DoNotOptimize(metrics.bytesProcessed);
    }

    setCounters(state, metrics, sizeBytes, baselineMs);
}

void BM_MMapThreadPool(benchmark::State& state)
{
    const auto sizeBytes = static_cast<std::size_t>(state.range(0)) * kMiB;
    const auto path = ensureBenchmarkFile(sizeBytes);
    const auto threadCount = static_cast<std::size_t>(state.range(1));
    const double baselineMs = freadBaselineRuntimeMs(path, sizeBytes, kDefaultChunkSize);
    RunMetrics metrics;

    for (auto _ : state) {
        metrics = processMMapThreaded(path, kDefaultChunkSize, threadCount);
        benchmark::DoNotOptimize(metrics.bytesProcessed);
    }

    setCounters(state, metrics, sizeBytes, baselineMs);
    state.counters["thread_count"] = benchmark::Counter(static_cast<double>(threadCount));
}

BENCHMARK(BM_FreadSingleThreaded)->Arg(kBenchmarkFileSizeMiB)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MMapSingleThreaded)->Arg(kBenchmarkFileSizeMiB)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MMapThreadPool)->Args({kBenchmarkFileSizeMiB, static_cast<int64_t>(kDefaultThreadCount)})
    ->Unit(benchmark::kMillisecond);

} // namespace

BENCHMARK_MAIN();
