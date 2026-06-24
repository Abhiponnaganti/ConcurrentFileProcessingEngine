#include "ChunkSplitter.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {

TEST(ChunkSplitterTest, EmptyFileProducesNoChunks)
{
    const cfpe::ChunkSplitter splitter(64);

    EXPECT_TRUE(splitter.split(nullptr, 0).empty());
}

TEST(ChunkSplitterTest, FileSmallerThanChunkSizeProducesSingleChunk)
{
    const std::string input = "small file";
    const cfpe::ChunkSplitter splitter(64);

    const auto chunks = splitter.split(input.data(), input.size());

    ASSERT_EQ(chunks.size(), 1U);
    EXPECT_EQ(chunks[0].data, input.data());
    EXPECT_EQ(chunks[0].size, input.size());
    EXPECT_EQ(chunks[0].offset, 0U);
    EXPECT_EQ(chunks[0].chunkId, 0U);
    EXPECT_EQ(chunks[0].fileSize, input.size());
}

TEST(ChunkSplitterTest, FileExactlyEqualToChunkSizeProducesSingleChunk)
{
    const std::string input = "12345678";
    const cfpe::ChunkSplitter splitter(8);

    const auto chunks = splitter.split(input.data(), input.size());

    ASSERT_EQ(chunks.size(), 1U);
    EXPECT_EQ(chunks[0].size, 8U);
    EXPECT_EQ(chunks[0].offset, 0U);
    EXPECT_EQ(chunks[0].chunkId, 0U);
}

TEST(ChunkSplitterTest, FileLargerThanChunkSizeProducesManyChunks)
{
    const std::string input = "abcdefghijkl";
    const cfpe::ChunkSplitter splitter(4);

    const auto chunks = splitter.split(input.data(), input.size());

    ASSERT_EQ(chunks.size(), 3U);
    EXPECT_EQ(chunks[0].offset, 0U);
    EXPECT_EQ(chunks[0].size, 4U);
    EXPECT_EQ(chunks[1].offset, 4U);
    EXPECT_EQ(chunks[1].size, 4U);
    EXPECT_EQ(chunks[2].offset, 8U);
    EXPECT_EQ(chunks[2].size, 4U);
}

TEST(ChunkSplitterTest, FinalPartialChunkHasRemainingSize)
{
    const std::string input = "abcdefghij";
    const cfpe::ChunkSplitter splitter(4);

    const auto chunks = splitter.split(input.data(), input.size());

    ASSERT_EQ(chunks.size(), 3U);
    EXPECT_EQ(chunks[2].offset, 8U);
    EXPECT_EQ(chunks[2].size, 2U);
    EXPECT_EQ(chunks[2].chunkId, 2U);
    EXPECT_EQ(chunks[2].data, input.data() + 8);
}

TEST(ChunkSplitterTest, RejectsZeroSizedChunks)
{
    EXPECT_THROW(cfpe::ChunkSplitter(0), std::invalid_argument);
}

} // namespace
