#include <spkg.hxx>

int spkg::List(const Config &config)
{
    for (auto &path : config.Packages)
    {
        if (!std::filesystem::exists(path))
        {
            Warning("package repository path '{}' in config does not exist", path.string());
            continue;
        }

        if (!std::filesystem::is_directory(path))
        {
            Warning("package repository path '{}' in config is not a directory", path.string());
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
                continue;

            json::Node node;
            stream >> node;

            Package package;
            if (!(node >> package))
            {
                Warning("invalid package json in file '{}'", entry.path().string());
                continue;
            }

            std::cout << package.Id << ':' << package.Version << std::endl;
            std::cout << package.Name << " - " << package.Description << std::endl;

            for (auto &[key, fragment] : package.Fragments)
            {
                std::cout << "   - /" << key << std::endl;
                std::cout << "     " << fragment.Name << " - " << fragment.Description << std::endl;
            }
        }
    }

    return 0;
}
