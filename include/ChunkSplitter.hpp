#pragma once

#include <cstddef>
#include <vector>

#include "Types.hpp"

namespace cfpe {

class ChunkSplitter {
public:
    explicit ChunkSplitter(std::size_t chunkSizeBytes);

    [[nodiscard]] std::vector<ChunkDescriptor> split(const char* data, std::size_t size) const;
    [[nodiscard]] std::size_t chunkSizeBytes() const noexcept;

private:
    std::size_t chunkSizeBytes_{0};
};

} // namespace cfpe
