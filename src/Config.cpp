#include "Config.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace cfpe {

namespace {

std::size_t readPositiveSize(const nlohmann::json& document,
                             const char* key,
                             std::size_t currentValue)
{
    if (!document.contains(key)) {
        return currentValue;
    }
    const auto& value = document.at(key);
    if (!value.is_number_integer()) {
        throw std::invalid_argument(std::string(key) + " must be a positive integer");
    }

    if (value.is_number_unsigned()) {
        const auto parsed = value.get<std::uint64_t>();
        if (parsed == 0) {
            throw std::invalid_argument(std::string(key) + " must be greater than zero");
        }
        if (parsed > std::numeric_limits<std::size_t>::max()) {
            throw std::invalid_argument(std::string(key) + " is too large");
        }
        return static_cast<std::size_t>(parsed);
    }

    const auto parsed = value.get<std::int64_t>();
    if (parsed <= 0) {
        throw std::invalid_argument(std::string(key) + " must be greater than zero");
    }
    if (static_cast<std::uint64_t>(parsed) > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument(std::string(key) + " is too large");
    }

    return static_cast<std::size_t>(parsed);
}

} // namespace

EngineConfig Config::defaults()
{
    EngineConfig config;
    config.threadCount = std::max(1u, std::thread::hardware_concurrency());
    return config;
}

EngineConfig Config::fromJson(const nlohmann::json& document)
{
    EngineConfig config = defaults();

    if (document.contains("input_directory")) {
        config.inputDirectory = document.at("input_directory").get<std::string>();
    }
    if (document.contains("output_directory")) {
        config.outputDirectory = document.at("output_directory").get<std::string>();
    }
    config.threadCount = readPositiveSize(document, "thread_count", config.threadCount);
    config.chunkSizeBytes = readPositiveSize(document, "chunk_size_bytes", config.chunkSizeBytes);
    config.topN = readPositiveSize(document, "top_n", config.topN);
    if (document.contains("mode")) {
        config.mode = document.at("mode").get<std::string>();
    }
    if (document.contains("output_format")) {
        config.outputFormat = document.at("output_format").get<std::string>();
    }

    validate(config);
    return config;
}

EngineConfig Config::fromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open config file: " + path.string());
    }

    nlohmann::json document;
    input >> document;
    return fromJson(document);
}

void Config::validate(const EngineConfig& config)
{
    if (config.chunkSizeBytes == 0) {
        throw std::invalid_argument("chunk_size_bytes must be greater than zero");
    }
    if (config.threadCount == 0) {
        throw std::invalid_argument("thread_count must be greater than zero");
    }
    if (config.topN == 0) {
        throw std::invalid_argument("top_n must be greater than zero");
    }
    if (config.mode != "word_frequency") {
        throw std::invalid_argument("mode must be \"word_frequency\"");
    }
    if (config.outputFormat != "json") {
        throw std::invalid_argument("output_format must be \"json\"");
    }
}

} // namespace cfpe
