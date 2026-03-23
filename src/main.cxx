#include <fstream>

#include <log.hxx>
#include <spkg.hxx>

static spkg::Config get_config()
{
    if (std::ifstream stream(spkg::GetConfigDir() / "config.json"); stream)
    {
        json::Node node;
        stream >> node;

        if (spkg::Config value; node >> value)
            return value;
    }

    return {
        .Packages = { spkg::GetDefaultPackagesDir() },
        .Cache = spkg::GetDefaultCacheDir(),
        .Installed = {},
    };
}

static int set_config(const spkg::Config &value)
{
    const auto path = spkg::GetConfigDir() / "config.json";

    std::filesystem::create_directories(path.parent_path());
    if (!std::filesystem::exists(path.parent_path()))
        return spkg::Error("failed to create config parent directory '{}'", path.parent_path().string());

    if (!std::filesystem::is_directory(path.parent_path()))
        return spkg::Error("config parent path '{}' is not a directory", path.parent_path().string());

    std::ofstream stream(path);
    if (!stream)
        return spkg::Error("failed open config file '{}'", path.string());

    json::Node json;
    to_json(json, value);
    stream << json::pretty << json;

    return 0;
}

int main(const int argc, const char **argv)
{
    const std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty() || ((args[0] == "help" || args[0] == "h") && args.size() == 1))
        return spkg::Help();

    auto config = get_config();
    auto code = -1;

    if (args[0] == "list" || args[0] == "l")
    {
        if (args.size() == 1)
            code = List(config);
    }
    else if (args[0] == "install" || args[0] == "i")
    {
        if (args.size() == 2)
            code = Install(config, args[1], true, false);
    }
    else if (args[0] == "remove" || args[0] == "r")
    {
        if (args.size() == 2)
            code = Remove(config, args[1]);
    }
    else if (args[0] == "update" || args[0] == "u")
    {
        if (args.size() == 1)
            code = Update(config);
        else if (args.size() == 2)
            code = Update(config, args[1]);
    }

    if (code < 0)
    {
        std::cerr << "Invalid arguments. Use '" << argv[0] << " help' to print the manual." << std::endl;
        return 1;
    }

    set_config(config);
}
