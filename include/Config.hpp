#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace cfpe {

struct EngineConfig {
    std::filesystem::path inputDirectory{"input"};
    std::filesystem::path outputDirectory{"output"};
    std::size_t chunkSizeBytes{4 * 1024 * 1024};
    std::size_t threadCount{0};
    std::size_t topN{20};
    std::string mode{"word_frequency"};
    std::string outputFormat{"json"};
};

class Config {
public:
    static EngineConfig defaults();
    static EngineConfig fromJson(const nlohmann::json& document);
    static EngineConfig fromFile(const std::filesystem::path& path);
    static void validate(const EngineConfig& config);
};

} // namespace cfpe
