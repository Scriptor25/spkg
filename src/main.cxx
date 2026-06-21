#include <log.hxx>
#include <spkg.hxx>

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
            std::cerr << "\rwaiting for lock file to release [" << waited << "s]";
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
        return spkg::Error("failed to create config parent directory '{}'", path.parent_path());

    if (!std::filesystem::is_directory(path.parent_path()))
        return spkg::Error("config parent path '{}' is not a directory", path.parent_path());

    std::ofstream stream(path);
    if (!stream)
        return spkg::Error("failed open config file '{}'", path);

    stream << std::setw(2) << json::Node(value);

    return 0;
}

int main(const int argc, const char **argv) try
{
    const std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty() || ((args[0] == "help" || args[0] == "h") && args.size() == 1))
        return spkg::Help();

    acquire_lock();

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
