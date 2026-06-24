#include "OutputSerializer.hpp"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace cfpe {

namespace {

double bytesToMiB(std::uint64_t bytes)
{
    constexpr double bytesPerMiB = 1024.0 * 1024.0;
    return static_cast<double>(bytes) / bytesPerMiB;
}

nlohmann::json topWordsToJson(const std::vector<TopWord>& topWords)
{
    nlohmann::json values = nlohmann::json::array();
    for (const auto& word : topWords) {
        values.push_back({{"word", word.word}, {"count", word.count}});
    }
    return values;
}

std::string resultFilename(const std::filesystem::path& file)
{
    std::string name = file.filename().string();
    for (char& character : name) {
        const auto value = static_cast<unsigned char>(character);
        if (std::isalnum(value) == 0 && character != '.' && character != '-' && character != '_') {
            character = '_';
        }
    }
    return name + ".word_frequency.json";
}

void writeJsonDocument(const std::filesystem::path& path,
                       const nlohmann::json& document,
                       bool pretty)
{
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to open output file: " + path.string());
    }

    output << (pretty ? document.dump(2) : document.dump()) << '\n';
}

} // namespace

nlohmann::json OutputSerializer::fileResultToJson(const FileProcessingSummary& summary) const
{
    return {
        {"file", summary.file.string()},
        {"file_size_mb", bytesToMiB(summary.fileSizeBytes)},
        {"thread_count", summary.threadCount},
        {"chunk_size_bytes", summary.chunkSizeBytes},
        {"chunks_processed", summary.chunksProcessed},
        {"total_words", summary.totalWords},
        {"unique_words", summary.uniqueWords},
        {"processing_time_ms", summary.elapsedMs},
        {"top_words", topWordsToJson(summary.topWords)},
    };
}

nlohmann::json OutputSerializer::benchmarkReportToJson(
    const std::vector<FileProcessingSummary>& summaries,
    double totalProcessingTimeMs) const
{
    nlohmann::json files = nlohmann::json::array();
    std::uint64_t totalBytes = 0;
    std::uint64_t totalWords = 0;
    std::uint64_t totalChunks = 0;

    for (const auto& summary : summaries) {
        files.push_back(fileResultToJson(summary));
        totalBytes += summary.fileSizeBytes;
        totalWords += summary.totalWords;
        totalChunks += summary.chunksProcessed;
    }

    return {
        {"files_processed", summaries.size()},
        {"total_file_size_mb", bytesToMiB(totalBytes)},
        {"total_words", totalWords},
        {"total_chunks_processed", totalChunks},
        {"total_processing_time_ms", totalProcessingTimeMs},
        {"files", files},
    };
}

void OutputSerializer::writeFileResult(const std::filesystem::path& outputDirectory,
                                       const FileProcessingSummary& summary,
                                       bool pretty) const
{
    std::filesystem::create_directories(outputDirectory);
    writeJsonDocument(outputDirectory / resultFilename(summary.file),
                      fileResultToJson(summary),
                      pretty);
}

void OutputSerializer::writeBenchmarkReport(
    const std::filesystem::path& outputDirectory,
    const std::vector<FileProcessingSummary>& summaries,
    double totalProcessingTimeMs,
    bool pretty) const
{
    std::filesystem::create_directories(outputDirectory);
    writeJsonDocument(outputDirectory / "benchmark_report.json",
                      benchmarkReportToJson(summaries, totalProcessingTimeMs),
                      pretty);
}

} // namespace cfpe
