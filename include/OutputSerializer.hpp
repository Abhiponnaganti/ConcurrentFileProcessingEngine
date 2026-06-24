#pragma once

#include <filesystem>
#include <vector>

#include <nlohmann/json.hpp>

#include "Types.hpp"

namespace cfpe {

class OutputSerializer {
public:
    [[nodiscard]] nlohmann::json fileResultToJson(const FileProcessingSummary& summary) const;
    [[nodiscard]] nlohmann::json benchmarkReportToJson(
        const std::vector<FileProcessingSummary>& summaries,
        double totalProcessingTimeMs) const;

    void writeFileResult(const std::filesystem::path& outputDirectory,
                         const FileProcessingSummary& summary,
                         bool pretty) const;

    void writeBenchmarkReport(const std::filesystem::path& outputDirectory,
                              const std::vector<FileProcessingSummary>& summaries,
                              double totalProcessingTimeMs,
                              bool pretty) const;
};

} // namespace cfpe
