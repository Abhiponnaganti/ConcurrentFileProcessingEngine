#include "BenchmarkHarness.hpp"

#include "ChunkProcessor.hpp"
#include "ChunkSplitter.hpp"
#include "FileMapper.hpp"
#include "ThreadPool.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace cfpe {

namespace {

std::vector<std::filesystem::path> regularFiles(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(directory)) {
        return files;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open input file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void processSequential(const std::vector<ChunkDescriptor>& chunks,
                       const ChunkProcessor& processor,
                       ProcessingStats& stats)
{
    for (const auto& chunk : chunks) {
        const ChunkResult result = processor.process(chunk);
        stats.bytesProcessed += result.bytesProcessed;
        ++stats.chunksProcessed;
    }
}

void processThreaded(const std::vector<ChunkDescriptor>& chunks,
                     const ChunkProcessor& processor,
                     std::size_t threadCount,
                     ProcessingStats& stats)
{
    ThreadPool pool(threadCount);
    const ThreadPoolResult result = pool.process(chunks, processor);
    stats.bytesProcessed += result.bytesProcessed;
    stats.chunksProcessed += result.chunksProcessed;
}

} // namespace

BenchmarkHarness::BenchmarkHarness(EngineConfig config)
    : config_(std::move(config))
{
}

BenchmarkResult BenchmarkHarness::run(ProcessingStrategy strategy) const
{
    BenchmarkResult result;
    result.strategy = strategy;

    const ChunkSplitter splitter(config_.chunkSizeBytes);
    const ChunkProcessor processor;
    const auto startedAt = std::chrono::steady_clock::now();

    for (const auto& file : regularFiles(config_.inputDirectory)) {
        if (strategy == ProcessingStrategy::Sequential ||
            strategy == ProcessingStrategy::ThreadPool) {
            const std::string contents = readFile(file);
            const auto chunks = splitter.split(contents.data(), contents.size());
            if (strategy == ProcessingStrategy::Sequential) {
                processSequential(chunks, processor, result.stats);
            } else {
                processThreaded(chunks, processor, config_.threadCount, result.stats);
            }
            continue;
        }

        FileMapper mapper(file);
        const auto chunks = splitter.split(mapper.data(), mapper.size());
        if (strategy == ProcessingStrategy::MMapSequential) {
            processSequential(chunks, processor, result.stats);
        } else {
            processThreaded(chunks, processor, config_.threadCount, result.stats);
        }
    }

    const auto finishedAt = std::chrono::steady_clock::now();
    result.stats.elapsedSeconds =
        std::chrono::duration<double>(finishedAt - startedAt).count();
    return result;
}

std::vector<BenchmarkResult> BenchmarkHarness::runAll() const
{
    return {
        run(ProcessingStrategy::Sequential),
        run(ProcessingStrategy::ThreadPool),
        run(ProcessingStrategy::MMapSequential),
        run(ProcessingStrategy::MMapThreadPool),
    };
}

} // namespace cfpe
