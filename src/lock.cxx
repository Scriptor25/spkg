#include <lock.hxx>

#include <fstream>

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

toolkit::result<spkg::FileLock> spkg::FileLock::Lock(const std::filesystem::path &path)
{
    const auto path_string = path.string();
    const auto fd = open(path_string.c_str(), O_CREAT | O_RDWR, 0666);

    if (fd < 0)
    {
        return toolkit::make_error("failed to open lock file.");
    }

    if (flock(fd, LOCK_EX) != 0)
    {
        close(fd);
        return toolkit::make_error("failed to acquire lock.");
    }

    return FileLock(fd);
}

spkg::FileLock::~FileLock()
{
    if (m_Handle >= 0)
    {
        flock(m_Handle, LOCK_UN);
        close(m_Handle);
    }
}

spkg::FileLock::FileLock(FileLock &&other) noexcept
{
    std::swap(m_Handle, other.m_Handle);
}

spkg::FileLock &spkg::FileLock::operator=(FileLock &&other) noexcept
{
    std::swap(m_Handle, other.m_Handle);
    return *this;
}

spkg::FileLock::FileLock(const int handle)
    : m_Handle(handle)
{
}
