#include "FileMapper.hpp"

#include <cerrno>
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cfpe {

FileMapper::FileMapper(const std::filesystem::path& path)
{
    open(path);
}

FileMapper::~FileMapper()
{
    close();
}

FileMapper::FileMapper(FileMapper&& other) noexcept
    : fd_(other.fd_), data_(other.data_), size_(other.size_)
{
    other.fd_ = -1;
    other.data_ = nullptr;
    other.size_ = 0;
}

FileMapper& FileMapper::operator=(FileMapper&& other) noexcept
{
    if (this != &other) {
        close();
        fd_ = other.fd_;
        data_ = other.data_;
        size_ = other.size_;
        other.fd_ = -1;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

void FileMapper::open(const std::filesystem::path& path)
{
    close();

    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ == -1) {
        throw std::system_error(errno, std::generic_category(), "open failed: " + path.string());
    }

    struct stat statBuffer {};
    if (::fstat(fd_, &statBuffer) == -1) {
        const int error = errno;
        close();
        throw std::system_error(error, std::generic_category(), "fstat failed: " + path.string());
    }

    if (!S_ISREG(statBuffer.st_mode)) {
        close();
        throw std::invalid_argument("path is not a regular file: " + path.string());
    }

    size_ = static_cast<std::size_t>(statBuffer.st_size);
    if (size_ == 0) {
        return;
    }

    void* mapped = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (mapped == MAP_FAILED) {
        const int error = errno;
        close();
        throw std::system_error(error, std::generic_category(), "mmap failed: " + path.string());
    }

    data_ = static_cast<const char*>(mapped);
}

void FileMapper::close() noexcept
{
    if (data_ != nullptr) {
        ::munmap(const_cast<char*>(data_), size_);
        data_ = nullptr;
    }

    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }

    size_ = 0;
}

const char* FileMapper::data() const noexcept
{
    return data_;
}

std::size_t FileMapper::size() const noexcept
{
    return size_;
}

bool FileMapper::empty() const noexcept
{
    return size_ == 0;
}

std::string_view FileMapper::view() const noexcept
{
    if (data_ == nullptr || size_ == 0) {
        return {};
    }
    return {data_, size_};
}

} // namespace cfpe
