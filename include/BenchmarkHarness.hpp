#pragma once

#include <filesystem>
#include <vector>

#include "Config.hpp"
#include "Types.hpp"

namespace cfpe {

class BenchmarkHarness {
public:
    explicit BenchmarkHarness(EngineConfig config);

    [[nodiscard]] BenchmarkResult run(ProcessingStrategy strategy) const;
    [[nodiscard]] std::vector<BenchmarkResult> runAll() const;

private:
    EngineConfig config_;
};

} // namespace cfpe
