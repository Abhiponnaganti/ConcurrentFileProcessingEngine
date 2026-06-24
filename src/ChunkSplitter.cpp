#include "ChunkSplitter.hpp"

#include <algorithm>
#include <stdexcept>

namespace cfpe {

ChunkSplitter::ChunkSplitter(std::size_t chunkSizeBytes)
    : chunkSizeBytes_(chunkSizeBytes)
{
    if (chunkSizeBytes_ == 0) {
        throw std::invalid_argument("chunk size must be greater than zero");
    }
}

std::vector<ChunkDescriptor> ChunkSplitter::split(const char* data, std::size_t size) const
{
    std::vector<ChunkDescriptor> chunks;
    if (data == nullptr || size == 0) {
        return chunks;
    }

    std::size_t chunkId = 0;
    for (std::size_t offset = 0; offset < size; offset += chunkSizeBytes_) {
        const std::size_t remaining = size - offset;
        const std::size_t chunkSize = std::min(chunkSizeBytes_, remaining);
        chunks.push_back({data + offset, chunkSize, offset, chunkId, size});
        ++chunkId;
    }

    return chunks;
}

std::size_t ChunkSplitter::chunkSizeBytes() const noexcept
{
    return chunkSizeBytes_;
}

} // namespace cfpe
