#include <config.hxx>
#include <log.hxx>
#include <spkg.hxx>

#include <toolkit/args.hxx>

#include <fstream>
#include <lock.hxx>
#include <thread>

static toolkit::result<spkg::Config> read_config()
{
    auto directory = spkg::GetConfigDir();
    auto config_path = directory / "config.json";
    auto lock_path = directory / "config.lock";

    if (std::filesystem::exists(config_path))
    {
        spkg::FileLock lock;
        if (auto res = spkg::FileLock::Lock(lock_path) >> lock; !res)
            return res;

        if (std::ifstream stream(config_path); stream)
        {
            json::Node node;
            stream >> node;

            if (spkg::Config value; node >> value)
                return value;
        }
    }

    return spkg::Config
    {
        .Cache = spkg::GetDefaultCacheDir(),
        .Packages = { spkg::GetDefaultPackagesDir() },
        .Installed = {},
    };
}

static spkg::Config read_config_no_lock()
{
    auto directory = spkg::GetConfigDir();
    auto config_path = directory / "config.json";

    if (std::filesystem::exists(config_path))
    {
        if (std::ifstream stream(config_path); stream)
        {
            json::Node node;
            stream >> node;

            if (spkg::Config value; node >> value)
                return value;
        }
    }

    return spkg::Config
    {
        .Cache = spkg::GetDefaultCacheDir(),
        .Packages = { spkg::GetDefaultPackagesDir() },
        .Installed = {},
    };
}

static toolkit::result<> write_config(spkg::Config &config)
{
    if (!config.CacheUpdated
        && config.PackagesAdded.empty()
        && config.PackagesRemoved.empty()
        && config.InstalledAdded.empty()
        && config.InstalledRemoved.empty())
        return {};

    auto directory = spkg::GetConfigDir();
    auto config_path = directory / "config.json";
    auto temp_path = directory / "config.temp";
    auto lock_path = directory / "config.lock";

    if (!std::filesystem::exists(directory))
    {
        if (std::error_code ec; std::filesystem::create_directories(directory, ec), ec)
            return toolkit::make_error("failed to create config parent directory '{}': {}", directory, ec.message());
    }
    else
    {
        if (!std::filesystem::is_directory(directory))
            return toolkit::make_error("config parent path is not a directory.");
    }

    spkg::FileLock lock;
    if (auto res = spkg::FileLock::Lock(lock_path) >> lock; !res)
        return res;

    auto merge = read_config_no_lock();

    if (config.CacheUpdated)
        merge.Cache = config.Cache;
    for (auto &key : config.PackagesAdded)
        merge.Packages.insert(key);
    for (auto &key : config.PackagesRemoved)
        merge.Packages.erase(key);
    for (auto &key : config.InstalledAdded)
        merge.Installed.insert({ key, config.Installed.at(key) });
    for (auto &key : config.InstalledRemoved)
        merge.Installed.erase(key);

    {
        std::ofstream stream(temp_path);
        if (!stream)
            return toolkit::make_error("failed open temporary config file.");

        stream << std::setw(2) << json::Node(merge);
    }

    if (std::error_code ec; std::filesystem::remove(config_path, ec), ec)
        return toolkit::make_error("failed to remove original config file.");
    if (std::error_code ec; std::filesystem::rename(temp_path, config_path, ec), ec)
        return toolkit::make_error("failed to rename temporary config file.");

    config.CacheUpdated = {};
    config.PackagesAdded = {};
    config.PackagesRemoved = {};
    config.InstalledAdded = {};
    config.InstalledRemoved = {};

    return {};
}

static const toolkit::arg_manifest manifest;

enum class Operation
{
    Help,
    List,
    Install,
    Remove,
    Update,
};

static const std::unordered_map<std::string_view, Operation> operations
{
    { "help", Operation::Help },
    { "h", Operation::Help },
    { "?", Operation::Help },
    { "list", Operation::List },
    { "l", Operation::List },
    { "install", Operation::Install },
    { "i", Operation::Install },
    { "remove", Operation::Remove },
    { "r", Operation::Remove },
    { "update", Operation::Update },
    { "u", Operation::Update },
};

static toolkit::result<> run(const toolkit::arg_context &args, spkg::Config &config, Operation operation)
{
    const auto count = args.limit == ~size_t() ? args.size() : args.limit;

    switch (operation)
    {
    case Operation::Help:
        spkg::Help();
        return {};

    case Operation::List:
        if (count != 1)
            break;

        List(config);
        return {};

    case Operation::Install:
        if (count != 2)
            break;

        return Install(
            config,
            args[1],
            { args.positional.begin() + static_cast<long>(count), args.positional.end() },
            true,
            false);

    case Operation::Remove:
        if (count != 2)
            break;

        return Remove(config, args[1]);

    case Operation::Update:
        if (count == 1)
            return Update(config);
        if (count == 2)
            return Update(config, args[1]);
        break;
    }

    return toolkit::make_error(
        "invalid arguments for operation '{}'. Use '{} help' to print the manual.",
        args[0],
        args.file);
}

int main(const int argc, const char **argv)
{
    toolkit::arg_context args;
    if (auto res = toolkit::arg_parse(manifest, argc, argv) >> args; !res)
    {
        spkg::Error("failed to parse arguments: {}", res.error());
        return 1;
    }

    if (args.empty())
    {
        spkg::Help();
        return 0;
    }

    auto it = operations.find(args[0]);
    if (it == operations.end())
    {
        std::cerr
                << "invalid operation '"
                << args[0]
                << "'. Use '"
                << args.file
                << " help' to print the manual."
                << std::endl;
        return 1;
    }

    auto operation = it->second;

    spkg::Config config;
    if (auto res = read_config() >> config; !res)
    {
        spkg::Error("failed to get config: {}", res.error());
        return 1;
    }

    if (auto res = run(args, config, operation); !res)
    {
        std::cerr << res.error() << std::endl;
        return 1;
    }

    write_config(config);
    return 0;
}
