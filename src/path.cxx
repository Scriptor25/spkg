#include <spkg.hxx>

std::filesystem::path spkg::GetHomeDir()
{
    if (const auto home = std::getenv("HOME"))
        return home;
    return std::filesystem::current_path();
}

std::filesystem::path spkg::GetDefaultPackagesDir()
{
    return GetHomeDir() / ".local" / "share" / "spkg" / "packages";
}

std::filesystem::path spkg::GetDefaultCacheDir()
{
    return GetHomeDir() / ".cache" / "spkg";
}

std::filesystem::path spkg::GetConfigDir()
{
    return GetHomeDir() / ".config" / "spkg";
}
