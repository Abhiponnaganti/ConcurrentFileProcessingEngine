#pragma once

#include "Types.hpp"

namespace cfpe {

class ChunkProcessor {
public:
    ChunkProcessor() = default;

    [[nodiscard]] ChunkResult process(const ChunkDescriptor& chunk) const;
};

} // namespace cfpe
