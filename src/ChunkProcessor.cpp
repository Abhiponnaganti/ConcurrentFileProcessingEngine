#include "ChunkProcessor.hpp"

#include <cctype>
#include <stdexcept>
#include <string>

namespace cfpe {

namespace {

bool isWordCharacter(char value)
{
    return std::isalnum(static_cast<unsigned char>(value)) != 0;
}

char normalize(char value)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

} // namespace

ChunkResult ChunkProcessor::process(const ChunkDescriptor& chunk) const
{
    ChunkResult result;
    result.chunk = chunk;
    result.bytesProcessed = chunk.size;

    std::string current;
    std::size_t index = 0;

    if (chunk.empty()) {
        return result;
    }
    if (chunk.data == nullptr) {
        throw std::invalid_argument("non-empty chunk descriptor has null data");
    }
    if (chunk.offset > chunk.fileSize || chunk.size > chunk.fileSize - chunk.offset) {
        throw std::invalid_argument("chunk descriptor exceeds file bounds");
    }

    // If this chunk starts inside a word, discard that leading fragment.
    if (chunk.offset > 0 && isWordCharacter(chunk.data[-1]) && isWordCharacter(chunk.data[0])) {
        while (index < chunk.size && isWordCharacter(chunk.data[index])) {
            ++index;
        }
    }

    for (; index < chunk.size; ++index) {
        const char character = chunk.data[index];
        if (isWordCharacter(character)) {
            current.push_back(normalize(character));
            continue;
        }

        if (!current.empty()) {
            ++result.frequencies[current];
            current.clear();
        }
    }

    if (!current.empty()) {
        std::size_t cursor = chunk.size;
        while (chunk.offset + cursor < chunk.fileSize && isWordCharacter(chunk.data[cursor])) {
            current.push_back(normalize(chunk.data[cursor]));
            ++cursor;
        }
        ++result.frequencies[current];
    }

    return result;
}

} // namespace cfpe
