#include "Config.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

nlohmann::json validConfig()
{
    return {
        {"input_directory", "input"},
        {"output_directory", "output"},
        {"thread_count", 4},
        {"chunk_size_bytes", 1024},
        {"top_n", 25},
        {"mode", "word_frequency"},
        {"output_format", "json"},
    };
}

TEST(ConfigTest, ValidConfigParses)
{
    const auto config = cfpe::Config::fromJson(validConfig());

    EXPECT_EQ(config.inputDirectory, "input");
    EXPECT_EQ(config.outputDirectory, "output");
    EXPECT_EQ(config.threadCount, 4U);
    EXPECT_EQ(config.chunkSizeBytes, 1024U);
    EXPECT_EQ(config.topN, 25U);
    EXPECT_EQ(config.mode, "word_frequency");
    EXPECT_EQ(config.outputFormat, "json");
}

TEST(ConfigTest, RejectsInvalidThreadCount)
{
    auto document = validConfig();
    document["thread_count"] = 0;

    EXPECT_THROW(cfpe::Config::fromJson(document), std::invalid_argument);
}

TEST(ConfigTest, RejectsNegativeThreadCount)
{
    auto document = validConfig();
    document["thread_count"] = -1;

    EXPECT_THROW(cfpe::Config::fromJson(document), std::invalid_argument);
}

TEST(ConfigTest, RejectsInvalidChunkSize)
{
    auto document = validConfig();
    document["chunk_size_bytes"] = 0;

    EXPECT_THROW(cfpe::Config::fromJson(document), std::invalid_argument);
}

TEST(ConfigTest, RejectsNonIntegerChunkSize)
{
    auto document = validConfig();
    document["chunk_size_bytes"] = 1024.5;

    EXPECT_THROW(cfpe::Config::fromJson(document), std::invalid_argument);
}

TEST(ConfigTest, RejectsInvalidTopN)
{
    auto document = validConfig();
    document["top_n"] = 0;

    EXPECT_THROW(cfpe::Config::fromJson(document), std::invalid_argument);
}

TEST(ConfigTest, RejectsUnsupportedMode)
{
    auto document = validConfig();
    document["mode"] = "line_count";

    EXPECT_THROW(cfpe::Config::fromJson(document), std::invalid_argument);
}

TEST(ConfigTest, RejectsUnsupportedOutputFormat)
{
    auto document = validConfig();
    document["output_format"] = "csv";

    EXPECT_THROW(cfpe::Config::fromJson(document), std::invalid_argument);
}

} // namespace
