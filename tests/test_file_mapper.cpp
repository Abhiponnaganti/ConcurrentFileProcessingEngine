#include "FileMapper.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

std::filesystem::path tempFilePath(const std::string& name)
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("cfpe_" + name + "_" + std::to_string(now));
}

void writeFile(const std::filesystem::path& path, const std::string& contents)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("failed to create test file: " + path.string());
    }
    output << contents;
}

TEST(FileMapperTest, MapsNonEmptyFile)
{
    const auto path = tempFilePath("non_empty");
    writeFile(path, "alpha beta");

    {
        const cfpe::FileMapper mapper(path);
        EXPECT_FALSE(mapper.empty());
        EXPECT_EQ(mapper.size(), 10U);
        EXPECT_EQ(mapper.view(), "alpha beta");
    }

    std::filesystem::remove(path);
}

TEST(FileMapperTest, HandlesEmptyFileSafely)
{
    const auto path = tempFilePath("empty");
    writeFile(path, "");

    {
        const cfpe::FileMapper mapper(path);
        EXPECT_TRUE(mapper.empty());
        EXPECT_EQ(mapper.size(), 0U);
        EXPECT_EQ(mapper.data(), nullptr);
        EXPECT_TRUE(mapper.view().empty());
    }

    std::filesystem::remove(path);
}

TEST(FileMapperTest, RejectsNonRegularFile)
{
    const auto tempDir = std::filesystem::temp_directory_path();

    EXPECT_THROW(
        { cfpe::FileMapper mapper(tempDir); },
        std::invalid_argument);
}

TEST(FileMapperTest, ThrowsForMissingFile)
{
    const auto path = tempFilePath("missing");

    EXPECT_THROW(
        { cfpe::FileMapper mapper(path); },
        std::system_error);
}

} // namespace
