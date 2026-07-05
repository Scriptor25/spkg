#include <config.hxx>
#include <log.hxx>
#include <package.hxx>
#include <spkg.hxx>

#include <json/json.hxx>
#include <toml/toml.hxx>

#include <fstream>

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
            Warning("repository path '{}' does not exist", path);
            continue;
        }

        if (!std::filesystem::is_directory(path))
        {
            Warning("repository path '{}' is not a directory", path);
            continue;
        }

        for (auto &entry : std::filesystem::directory_iterator(path))
        {
            if (entry.is_directory())
                continue;

            auto ext = entry.path().extension();
            if (ext != ".json" && ext != ".toml")
                continue;

            std::ifstream stream(entry.path());
            if (!stream)
            {
                Warning("failed to open package file '{}'", entry.path());
                continue;
            }

            Package package;

            if (ext == ".json")
            {
                json::node node;
                stream >> node;

                if (!(node >> package))
                {
                    Warning("invalid json in package file '{}'", entry.path());
                    continue;
                }
            }
            else if (ext == ".toml")
            {
                toml::node node;
                stream >> node;

                if (!(node >> package))
                {
                    Warning("invalid toml in package file '{}'", entry.path());
                    continue;
                }
            }

            if (fn(std::move(package)))
                return true;
        }
    }

    return false;
}
