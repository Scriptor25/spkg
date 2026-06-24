#include <config.hxx>
#include <log.hxx>
#include <spkg.hxx>

#include <toolkit/args.hxx>

#include <fstream>
#include <thread>

static const auto path = spkg::GetConfigDir() / "config.json";
static const auto lock = spkg::GetConfigDir() / "config.lock";

static auto locked = false;

static void acquire_lock()
{
    if (locked)
        return;

    if (std::filesystem::exists(lock))
    {
        size_t waited{};

        do
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ++waited;
            std::cerr << "\rWaiting for lock file to release [" << waited << "s]";
        }
        while (std::filesystem::exists(lock));

        std::cerr << std::endl;
    }

    {
        // create lock file
        locked = true;
        std::ofstream lock_stream(lock);
    }
}

static void release_lock()
{
    if (!locked)
        return;

    std::filesystem::remove(lock);
    locked = false;
}

static spkg::Config get_config()
{
    if (std::filesystem::exists(path))
    {
        if (std::ifstream stream(path); stream)
        {
            json::Node node;
            stream >> node;

            if (spkg::Config value; node >> value)
                return value;
        }
    }

    return {
        .Packages = { spkg::GetDefaultPackagesDir() },
        .Cache = spkg::GetDefaultCacheDir(),
        .Installed = {},
    };
}

static int set_config(const spkg::Config &value)
{
    std::filesystem::create_directories(path.parent_path());
    if (!std::filesystem::exists(path.parent_path()))
        return spkg::Error("Failed to create config parent directory '{}'", path.parent_path());

    if (!std::filesystem::is_directory(path.parent_path()))
        return spkg::Error("Config parent path '{}' is not a directory", path.parent_path());

    std::ofstream stream(path);
    if (!stream)
        return spkg::Error("Failed open config file '{}'", path);

    stream << std::setw(2) << json::Node(value);

    return 0;
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

int main(const int argc, const char **argv) try
{
    toolkit::arg_context args;
    if (auto res = toolkit::arg_parse(manifest, argc, argv) >> args; !res)
    {
        std::cerr << res.error() << std::endl;
        return 1;
    }

    if (args.empty())
        return spkg::Help();

    auto it = operations.find(args[0]);
    if (it == operations.end())
    {
        std::cerr
                << "Invalid operation '"
                << args[0]
                << "'. Use '"
                << args.file
                << " help' to print the manual."
                << std::endl;
        return 1;
    }

    auto operation = it->second;

    acquire_lock();

    auto config = get_config();
    auto code = -1;

    auto count = args.limit == ~size_t() ? args.size() : args.limit;

    switch (operation)
    {
    case Operation::Help:
        return spkg::Help();
    case Operation::List:
        if (count == 1)
            code = List(config);
        break;
    case Operation::Install:
        if (count == 2)
            code = Install(
                config,
                args[1],
                { args.positional.begin() + count, args.positional.end() },
                true,
                false);
        break;
    case Operation::Remove:
        if (count == 2)
            code = Remove(config, args[1]);
        break;
    case Operation::Update:
        if (count == 1)
            code = Update(config);
        else if (count == 2)
            code = Update(config, args[1]);
        break;
    }

    if (code < 0)
    {
        std::cerr
                << "Invalid arguments for operation '"
                << args[0]
                << "'. Use '"
                << args.file
                << " help' to print the manual."
                << std::endl;

        release_lock();
        return 1;
    }

    if (!code)
    {
        set_config(config);

        release_lock();
        return 0;
    }

    release_lock();
    return code;
}
catch (const std::runtime_error &error)
{
    std::cerr << error.what() << std::endl;

    release_lock();
    return -1;
}
