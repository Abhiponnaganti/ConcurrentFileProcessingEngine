#include "ChunkProcessor.hpp"
#include "ChunkSplitter.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

cfpe::ChunkDescriptor descriptorFor(const std::string& input,
                                    std::size_t offset,
                                    std::size_t size,
                                    std::size_t chunkId = 0)
{
    return {input.data() + offset, size, offset, chunkId, input.size()};
}

cfpe::WordCountMap mergeProcessedChunks(const std::vector<cfpe::ChunkDescriptor>& chunks)
{
    const cfpe::ChunkProcessor processor;
    cfpe::WordCountMap merged;
    for (const auto& chunk : chunks) {
        const auto result = processor.process(chunk);
        for (const auto& [word, count] : result.frequencies) {
            merged[word] += count;
        }
    }
    return merged;
}

TEST(ChunkProcessorTest, CountsBasicWords)
{
    const std::string input = "alpha beta gamma";
    const cfpe::ChunkProcessor processor;

    const auto result = processor.process(descriptorFor(input, 0, input.size()));

    EXPECT_EQ(result.frequencies.at("alpha"), 1U);
    EXPECT_EQ(result.frequencies.at("beta"), 1U);
    EXPECT_EQ(result.frequencies.at("gamma"), 1U);
}

TEST(ChunkProcessorTest, NormalizesWordsToLowercase)
{
    const std::string input = "Alpha ALPHA alpha";
    const cfpe::ChunkProcessor processor;

    const auto result = processor.process(descriptorFor(input, 0, input.size()));

    EXPECT_EQ(result.frequencies.at("alpha"), 3U);
    EXPECT_EQ(result.frequencies.size(), 1U);
}

TEST(ChunkProcessorTest, TreatsPunctuationAsDelimiters)
{
    const std::string input = "alpha,beta.gamma;delta!";
    const cfpe::ChunkProcessor processor;

    const auto result = processor.process(descriptorFor(input, 0, input.size()));

    EXPECT_EQ(result.frequencies.at("alpha"), 1U);
    EXPECT_EQ(result.frequencies.at("beta"), 1U);
    EXPECT_EQ(result.frequencies.at("gamma"), 1U);
    EXPECT_EQ(result.frequencies.at("delta"), 1U);
}

TEST(ChunkProcessorTest, CountsRepeatedWords)
{
    const std::string input = "cache cache worker cache";
    const cfpe::ChunkProcessor processor;

    const auto result = processor.process(descriptorFor(input, 0, input.size()));

    EXPECT_EQ(result.frequencies.at("cache"), 3U);
    EXPECT_EQ(result.frequencies.at("worker"), 1U);
}

TEST(ChunkProcessorTest, KeepsNumbersInsideTokens)
{
    const std::string input = "http2 worker42 2026 http2";
    const cfpe::ChunkProcessor processor;

    const auto result = processor.process(descriptorFor(input, 0, input.size()));

    EXPECT_EQ(result.frequencies.at("http2"), 2U);
    EXPECT_EQ(result.frequencies.at("worker42"), 1U);
    EXPECT_EQ(result.frequencies.at("2026"), 1U);
}

TEST(ChunkProcessorTest, CountsWordSplitAcrossBoundaryOnce)
{
    const std::string input = "alpha boundaryword omega";
    const cfpe::ChunkProcessor processor;

    const auto first = processor.process(descriptorFor(input, 0, 14, 0));
    const auto second = processor.process(descriptorFor(input, 14, input.size() - 14, 1));

    EXPECT_EQ(first.frequencies.at("alpha"), 1U);
    EXPECT_EQ(first.frequencies.at("boundaryword"), 1U);
    EXPECT_EQ(first.frequencies.count("boundar"), 0U);
    EXPECT_EQ(second.frequencies.count("rd"), 0U);
    EXPECT_EQ(second.frequencies.count("boundaryword"), 0U);
    EXPECT_EQ(second.frequencies.at("omega"), 1U);
}

TEST(ChunkProcessorTest, SkipsLeadingPartialWord)
{
    const std::string input = "alpha beta gamma";
    const cfpe::ChunkProcessor processor;

    const auto result = processor.process(descriptorFor(input, 3, 7, 1));

    EXPECT_EQ(result.frequencies.count("ha"), 0U);
    EXPECT_EQ(result.frequencies.at("beta"), 1U);
}

TEST(ChunkProcessorTest, CompletesTrailingWordFromBackingFile)
{
    const std::string input = "alpha beta gamma";
    const cfpe::ChunkProcessor processor;

    const auto result = processor.process(descriptorFor(input, 0, 8, 0));

    EXPECT_EQ(result.frequencies.at("alpha"), 1U);
    EXPECT_EQ(result.frequencies.at("beta"), 1U);
    EXPECT_EQ(result.frequencies.count("be"), 0U);
}

TEST(ChunkProcessorTest, CountsLongWordOnlyFromStartingChunk)
{
    const std::string input = "supercalifragilistic";
    const cfpe::ChunkProcessor processor;

    const auto first = processor.process(descriptorFor(input, 0, 5, 0));
    const auto second = processor.process(descriptorFor(input, 5, 5, 1));
    const auto third = processor.process(descriptorFor(input, 10, input.size() - 10, 2));

    EXPECT_EQ(first.frequencies.at("supercalifragilistic"), 1U);
    EXPECT_TRUE(second.frequencies.empty());
    EXPECT_TRUE(third.frequencies.empty());
}

TEST(ChunkProcessorTest, SplitAndProcessChunksPreservesBoundaryWordCount)
{
    const std::string input = "alpha boundaryword omega boundaryword";
    const cfpe::ChunkSplitter splitter(8);

    const auto merged = mergeProcessedChunks(splitter.split(input.data(), input.size()));

    EXPECT_EQ(merged.at("alpha"), 1U);
    EXPECT_EQ(merged.at("boundaryword"), 2U);
    EXPECT_EQ(merged.at("omega"), 1U);
    EXPECT_EQ(merged.size(), 3U);
}

TEST(ChunkProcessorTest, RejectsDescriptorOutsideFileBounds)
{
    const std::string input = "alpha";
    const cfpe::ChunkProcessor processor;

    EXPECT_THROW(
        {
            const auto result = processor.process({input.data(), 8, 0, 0, input.size()});
            (void)result;
        },
        std::invalid_argument);
}

TEST(ChunkProcessorTest, RejectsNonEmptyNullDescriptor)
{
    const cfpe::ChunkProcessor processor;

    EXPECT_THROW(
        {
            const auto result = processor.process({nullptr, 1, 0, 0, 1});
            (void)result;
        },
        std::invalid_argument);
}

} // namespace
