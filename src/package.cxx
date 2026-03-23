#include <fstream>

#include <log.hxx>
#include <package.hxx>

bool spkg::FindPackage(const Config &config, const Specifier &specifier, Package &package)
{
    return ForEachPackage(
        config,
        [&](Package &&value)
        {
            const auto found = value.Id == specifier.Id;
            package = std::forward<Package>(value);
            return found;
        });
}

bool spkg::ForEachPackage(const Config &config, const std::function<bool(Package &&)> &fn)
{
    for (auto &path : config.Packages)
    {
        if (!std::filesystem::exists(path))
        {
            Warning("repository path '{}' does not exist", path.string());
            continue;
        }

        if (!std::filesystem::is_directory(path))
        {
            Warning("repository path '{}' is not a directory", path.string());
            continue;
        }

        for (auto &entry : std::filesystem::directory_iterator(path))
        {
            if (entry.is_directory())
                continue;

            if (entry.path().extension() != ".json")
                continue;

            std::ifstream stream(entry.path());
            if (!stream)
            {
                Warning("failed to open package file '{}'", entry.path().string());
                continue;
            }

            json::Node node;
            stream >> node;

            Package package;
            if (!(node >> package))
            {
                Warning("invalid json in package file '{}'", entry.path().string());
                continue;
            }

            if (fn(std::move(package)))
                return true;
        }
    }

    return false;
}
