#pragma once

#include <toolkit/result.hxx>

#include <filesystem>

namespace spkg
{
    class FileLock
    {
    public:
        [[nodiscard]] static toolkit::result<FileLock> Lock(const std::filesystem::path &path);

        FileLock() = default;
        ~FileLock();

        FileLock(const FileLock &) = delete;
        FileLock &operator=(const FileLock &) = delete;

        FileLock(FileLock &&other) noexcept;
        FileLock &operator=(FileLock &&other) noexcept;

    private:
        explicit FileLock(int handle);

        int m_Handle = -1;
    };
}
