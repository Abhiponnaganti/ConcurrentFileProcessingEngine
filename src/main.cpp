#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ChunkProcessor.hpp"
#include "ChunkSplitter.hpp"
#include "Config.hpp"
#include "FileMapper.hpp"
#include "OutputSerializer.hpp"
#include "ResultAggregator.hpp"
#include "ThreadPool.hpp"

namespace {

double bytesToMiB(std::uint64_t bytes)
{
    constexpr double bytesPerMiB = 1024.0 * 1024.0;
    return static_cast<double>(bytes) / bytesPerMiB;
}

std::vector<std::filesystem::path> findInputFiles(const std::filesystem::path& directory)
{
    if (!std::filesystem::exists(directory)) {
        throw std::runtime_error("input directory does not exist: " + directory.string());
    }
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error("input path is not a directory: " + directory.string());
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

cfpe::FileProcessingSummary processFile(const std::filesystem::path& file,
                                        const cfpe::EngineConfig& config)
{
    const auto startedAt = std::chrono::steady_clock::now();

    cfpe::FileMapper mapper(file);
    const cfpe::ChunkSplitter splitter(config.chunkSizeBytes);
    const std::vector<cfpe::ChunkDescriptor> chunks = splitter.split(mapper.data(), mapper.size());

    const cfpe::ChunkProcessor processor;
    cfpe::ThreadPool pool(config.threadCount);
    cfpe::ThreadPoolResult poolResult = pool.process(chunks, processor);

    const cfpe::ResultAggregator aggregator;
    cfpe::AggregatedResult aggregate = aggregator.aggregate(poolResult, config.topN);

    const auto finishedAt = std::chrono::steady_clock::now();
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();

    return {
        file,
        static_cast<std::uint64_t>(mapper.size()),
        config.threadCount,
        config.chunkSizeBytes,
        aggregate.stats.chunksProcessed,
        aggregate.totalWords,
        aggregate.uniqueWords,
        elapsedMs,
        aggregate.topWords,
    };
}

} // namespace

int main(int argc, char** argv)
{
    const auto configPath = argc > 1 ? argv[1] : "config.json";

    try {
        const cfpe::EngineConfig config = cfpe::Config::fromFile(configPath);
        const std::vector<std::filesystem::path> files = findInputFiles(config.inputDirectory);
        const cfpe::OutputSerializer serializer;
        std::vector<cfpe::FileProcessingSummary> summaries;
        summaries.reserve(files.size());

        std::filesystem::create_directories(config.outputDirectory);

        const auto batchStartedAt = std::chrono::steady_clock::now();

        std::cout << "Concurrent File Processing Engine\n"
                  << "input directory: " << config.inputDirectory << '\n'
                  << "output directory: " << config.outputDirectory << '\n'
                  << "threads: " << config.threadCount << '\n'
                  << "chunk size: " << config.chunkSizeBytes << " bytes\n"
                  << "files found: " << files.size() << "\n\n";

        for (const auto& file : files) {
            cfpe::FileProcessingSummary summary = processFile(file, config);
            serializer.writeFileResult(config.outputDirectory, summary, true);

            std::cout << std::fixed << std::setprecision(2)
                      << file.filename().string()
                      << " | " << bytesToMiB(summary.fileSizeBytes) << " MiB"
                      << " | chunks " << summary.chunksProcessed
                      << " | words " << summary.totalWords
                      << " | unique " << summary.uniqueWords
                      << " | " << summary.elapsedMs << " ms\n";

            summaries.push_back(std::move(summary));
        }

        const auto batchFinishedAt = std::chrono::steady_clock::now();
        const double totalProcessingTimeMs =
            std::chrono::duration<double, std::milli>(batchFinishedAt - batchStartedAt).count();

        serializer.writeBenchmarkReport(config.outputDirectory,
                                        summaries,
                                        totalProcessingTimeMs,
                                        true);

        std::cout << "\nprocessed " << summaries.size()
                  << " file(s) in " << std::fixed << std::setprecision(2)
                  << totalProcessingTimeMs << " ms\n";
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
