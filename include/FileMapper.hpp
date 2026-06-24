#pragma once

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace cfpe {

class FileMapper {
public:
    FileMapper() = default;
    explicit FileMapper(const std::filesystem::path& path);
    ~FileMapper();

    FileMapper(const FileMapper&) = delete;
    FileMapper& operator=(const FileMapper&) = delete;

    FileMapper(FileMapper&& other) noexcept;
    FileMapper& operator=(FileMapper&& other) noexcept;

    void open(const std::filesystem::path& path);
    void close() noexcept;

    [[nodiscard]] const char* data() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::string_view view() const noexcept;

private:
    int fd_{-1};
    const char* data_{nullptr};
    std::size_t size_{0};
};

} // namespace cfpe
