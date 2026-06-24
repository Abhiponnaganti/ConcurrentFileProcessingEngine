#include "ResultAggregator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

cfpe::ThreadPoolResult makePoolResult()
{
    cfpe::ThreadPoolResult poolResult;
    poolResult.bytesProcessed = 128;
    poolResult.chunksProcessed = 4;
    poolResult.elapsedMs = 12.5;
    poolResult.threadResults.resize(3);

    poolResult.threadResults[0].frequencies["alpha"] = 2;
    poolResult.threadResults[0].frequencies["beta"] = 1;
    poolResult.threadResults[1].frequencies["alpha"] = 3;
    poolResult.threadResults[1].frequencies["delta"] = 2;
    poolResult.threadResults[2].frequencies["gamma"] = 2;

    return poolResult;
}

TEST(ResultAggregatorTest, MergesMultipleMapsCorrectly)
{
    const cfpe::ResultAggregator aggregator;

    const auto result = aggregator.aggregate(makePoolResult(), 10);

    EXPECT_EQ(result.frequencies.at("alpha"), 5U);
    EXPECT_EQ(result.frequencies.at("beta"), 1U);
    EXPECT_EQ(result.frequencies.at("gamma"), 2U);
    EXPECT_EQ(result.frequencies.at("delta"), 2U);
}

TEST(ResultAggregatorTest, ComputesTotalAndUniqueWords)
{
    const cfpe::ResultAggregator aggregator;

    const auto result = aggregator.aggregate(makePoolResult(), 10);

    EXPECT_EQ(result.totalWords, 10U);
    EXPECT_EQ(result.uniqueWords, 4U);
    EXPECT_EQ(result.stats.bytesProcessed, 128U);
    EXPECT_EQ(result.stats.chunksProcessed, 4U);
}

TEST(ResultAggregatorTest, TopNSortingUsesDescendingCount)
{
    const cfpe::ResultAggregator aggregator;

    const auto result = aggregator.aggregate(makePoolResult(), 2);

    ASSERT_EQ(result.topWords.size(), 2U);
    EXPECT_EQ(result.topWords[0].word, "alpha");
    EXPECT_EQ(result.topWords[0].count, 5U);
    EXPECT_EQ(result.topWords[1].count, 2U);
}

TEST(ResultAggregatorTest, TopNSortingBreaksTiesAlphabetically)
{
    cfpe::WordCountMap frequencies = {
        {"zeta", 4},
        {"alpha", 4},
        {"beta", 4},
        {"omega", 1},
    };

    const auto topWords = cfpe::ResultAggregator::topWords(frequencies, 3);

    ASSERT_EQ(topWords.size(), 3U);
    EXPECT_EQ(topWords[0].word, "alpha");
    EXPECT_EQ(topWords[1].word, "beta");
    EXPECT_EQ(topWords[2].word, "zeta");
}

TEST(ResultAggregatorTest, RejectsZeroTopN)
{
    const cfpe::ResultAggregator aggregator;

    EXPECT_THROW(
        {
            const auto result = aggregator.aggregate(makePoolResult(), 0);
            (void)result;
        },
        std::invalid_argument);
}

} // namespace
