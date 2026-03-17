#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>

#include <json.hxx>

enum class file_reference_type
{
    file,
    manifest,
};

struct file_reference_t
{
    file_reference_type type{};
    std::string path;
};

struct command_step_t
{
    std::string dir;
    std::string file;
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
};

using command_list_t = std::vector<command_step_t>;

struct fragment_t
{
    std::string name;
    std::string description;
    
    std::string dir;
    std::map<std::string, std::string> env;
    
    command_list_t configure;
    command_list_t build;
    command_list_t install;

    std::vector<std::string> cache;
    std::vector<file_reference_t> files;
};

struct package_t
{
    std::string id;
    std::string version;

    std::string name;
    std::string description;
    
    std::map<std::string, std::string> env;

    command_list_t setup;
    std::vector<std::string> cache;

    fragment_t main;
    std::map<std::string, fragment_t> fragments;
};

struct config_t
{
    std::vector<std::filesystem::path> packages;
    std::filesystem::path cache;

    std::map<std::string, std::vector<std::string>> installed;
};

struct specifier_t
{
    specifier_t() = default;

    explicit specifier_t(const std::string_view s)
    {
        const auto fragment_begin = s.find_last_of('/');
        const auto version_begin = s.find_last_of(':');

        const auto id_length = fragment_begin == std::string_view::npos
                                   ? version_begin == std::string_view::npos
                                         ? std::string_view::npos
                                         : version_begin
                                   : fragment_begin;
        id = s.substr(0, id_length);

        if (fragment_begin != std::string_view::npos)
        {
            const auto fragment_length = version_begin == std::string_view::npos
                                             ? std::string_view::npos
                                             : version_begin - fragment_begin - 1;
            fragment = s.substr(fragment_begin + 1, fragment_length);
        }

        if (version_begin != std::string_view::npos)
            version = s.substr(version_begin + 1);
    }

    std::string_view id;
    std::string_view fragment;
    std::string_view version;
};

std::ostream &operator<<(std::ostream &stream, const command_step_t &step)
{
    if (!step.dir.empty())
        stream << "[" << step.dir << "] ";
    for (auto &[key, val] : step.env)
        stream << key << "=" << val << " ";
    stream << step.file;
    for (auto &arg : step.args)
        stream << " '" << arg << "'";
    return stream;
}

template<typename T>
bool get_opt(const json::Node &node, T &value, T default_value = {})
{
    std::optional<T> opt;
    if (!(node >> opt))
        return false;
    value = std::move(opt.value_or(std::move(default_value)));
    return true;
}

template<>
bool from_json(const json::Node &node, command_step_t &value)
{
    if (std::string file; node >> file)
    {
        value = { .file = std::move(file) };
        return true;
    }

    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= object_node["file"] >> value.file;
    ok &= get_opt(object_node["dir"], value.dir);
    ok &= get_opt(object_node["args"], value.args);
    ok &= get_opt(object_node["env"], value.env);

    return ok;
}

template<>
bool from_json(const json::Node &node, command_list_t &value)
{
    if (json::IsUndefined(node))
    {
        value.clear();
        return true;
    }

    if (command_step_t s; node >> s)
    {
        value.push_back(std::move(s));
        return true;
    }

    if (!json::IsArray(node))
        return false;
    
    auto &array_node = json::AsArray(node);
    value.reserve(array_node.size());

    auto ok = true;

    for (auto &element : array_node)
    {
        command_step_t s;
        auto p = element >> s;
        if (p)
            value.push_back(std::move(s));

        ok &= p;
    }

    return ok;
}

template<>
bool from_json(const json::Node &node, file_reference_t &value)
{
    if (std::string path; node >> path)
    {
        value = {
            .type = file_reference_type::file,
            .path = std::move(path),
        };
        return true;
    }

    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= object_node["type"] >> value.type;
    ok &= object_node["path"] >> value.path;

    return ok;
}

template<>
bool from_json(const json::Node &node, fragment_t &value)
{
    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= get_opt(object_node["name"], value.name);
    ok &= get_opt(object_node["description"], value.description);
    ok &= get_opt(object_node["env"], value.env);
    ok &= get_opt(object_node["dir"], value.dir);
    ok &= get_opt(object_node["cache"], value.cache);
    ok &= get_opt(object_node["configure"], value.configure);
    ok &= get_opt(object_node["build"], value.build);
    ok &= get_opt(object_node["install"], value.install);
    ok &= get_opt(object_node["files"], value.files);

    return ok;
}

template<>
bool from_json(const json::Node &node, package_t &value)
{
    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= object_node["id"] >> value.id;
    ok &= object_node["version"] >> value.version;
    ok &= object_node["main"] >> value.main;

    ok &= get_opt(object_node["name"], value.name);
    ok &= get_opt(object_node["description"], value.description);
    ok &= get_opt(object_node["env"], value.env);
    ok &= get_opt(object_node["setup"], value.setup);
    ok &= get_opt(object_node["cache"], value.cache);
    ok &= get_opt(object_node["fragments"], value.fragments);

    return ok;
}

static std::filesystem::path get_home_path()
{
    if (const auto home = std::getenv("HOME"))
        return home;
    return std::filesystem::current_path();
}

static std::filesystem::path get_packages_path()
{
    return get_home_path() / ".local" / "share" / "spkg" / "packages";
}

static std::filesystem::path get_cache_path()
{
    return get_home_path() / ".cache" / "spkg";
}

static std::filesystem::path get_config_path()
{
    return get_home_path() / ".config" / "spkg";
}

template<>
bool from_json(const json::Node &node, std::filesystem::path &value)
{
    if (std::string s; node >> s)
    {
        value = std::move(s);
        return true;
    }

    return false;
}

template<>
void to_json(json::Node &node, const std::filesystem::path &value)
{
    node << value.string();
}

template<>
bool from_json(const json::Node &node, config_t &value)
{
    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= get_opt(object_node["packages"], value.packages, { get_packages_path() });
    ok &= get_opt(object_node["cache"], value.cache, get_cache_path());
    ok &= get_opt(object_node["installed"], value.installed);

    return ok;
}

template<>
void to_json(json::Node &node, const config_t &value)
{
    json::ObjectNode object_node;

    object_node["packages"] << value.packages;
    object_node["cache"] << value.cache;
    object_node["installed"] << value.installed;

    node = std::move(object_node);
}

static config_t get_config()
{
    if (std::ifstream stream(get_config_path() / "config.json"); stream) {
        json::Node node;
        stream >> node;

        if (config_t value; node >> value)
            return value;
    }

    return {
        .packages = { get_packages_path() },
        .cache = get_cache_path(),
        .installed = {},
    };
}

static int set_config(const config_t &value)
{
    const auto path = get_config_path() / "config.json";

    std::ofstream stream(path);
    if (!stream)
    {
        std::cerr << "Failed open config file '" << path.string() << "'." << std::endl;
        return 1;
    }

    json::Node json;
    to_json(json, value);
    stream << json;

    return 0;
}

static std::vector<std::string> read_manifest(const std::filesystem::path &path)
{
    std::ifstream stream(path);
    if (!stream)
        return {};

    std::vector<std::string> manifest;
    for (std::string line; std::getline(stream, line);)
        manifest.push_back(std::move(line));

    return manifest;
}

static int help()
{
    std::cerr <<
            "spkg\n"
            "\n"
            "spkg [help|h]                                  - print manual\n"
            "spkg (list|l)                                  - list all packages\n"
            "spkg (install|i) <id>[/<fragment>][:<version>] - install package\n"
            "spkg (remove|r) <id>[/<fragment>]              - remove package\n"
            "spkg (update|u) [<id>[/<fragment>]]            - update all or specific package\n"
            << std::endl;

    return 0;
}

static int list(const config_t &config)
{
    for (auto &path : config.packages)
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

            package_t package;
            node >> package;

            std::cout << package.id << ':' << package.version << std::endl;
            std::cout << package.name << " - " << package.description << std::endl;
            
            for (auto &[key, fragment] : package.fragments)
            {
                std::cout << "   - /" << key << std::endl;
                std::cout << "     " << fragment.name << " - " << fragment.description << std::endl;
            }
        }

    return 0;
}

static void copy_cache(
    const std::filesystem::path &src_path,
    const std::filesystem::path &dst_path,
    const std::vector<std::string> &cache)
{
    for (auto &entry : cache)
    {
        auto from = src_path / entry;
        auto to = dst_path / entry;

        std::filesystem::copy(
            from,
            to,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::update_existing);
    }
}

static int exec(
    const std::filesystem::path &dir,
    const std::string &file,
    const std::vector<std::string> &args,
    const std::map<std::string, std::string> &envs)
{
    std::vector<char *> argv;
    argv.reserve(args.size() + 1);

    for (auto &arg : args)
        argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);

    const auto pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0)
    {
        chdir(dir.c_str());

        for (auto &[key,val] : envs)
            setenv(key.c_str(), val.c_str(), 1);

        execvp(file.c_str(), argv.data());
        _exit(127);
    }

    auto status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return -1;
}

static int exec(
    const package_t &package,
    const command_step_t &step,
    const std::filesystem::path &work_dir)
{
    std::vector<std::string> args;
    args.push_back(step.file);
    args.insert(args.end(), step.args.begin(), step.args.end());

    std::map<std::string, std::string> envs;
    envs.insert(package.env.begin(), package.env.end());
    envs.insert(step.env.begin(), step.env.end());

    return exec(work_dir / step.dir, step.file, args, envs);
}

static int exec(
    const package_t &package,
    const fragment_t &fragment,
    const command_step_t &step,
    const std::filesystem::path &work_dir)
{
    std::vector<std::string> args;
    args.push_back(step.file);
    args.insert(args.end(), step.args.begin(), step.args.end());

    std::map<std::string, std::string> envs;
    envs.insert(package.env.begin(), package.env.end());
    envs.insert(fragment.env.begin(), fragment.env.end());
    envs.insert(step.env.begin(), step.env.end());

    return exec(work_dir / fragment.dir / step.dir, step.file, args, envs);
}

static int install(config_t &config, const std::string_view arg)
{
    const specifier_t specifier(arg);

    package_t package;
    auto found = false;

    for (auto &path : config.packages)
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
            node >> package;

            found = package.id == specifier.id
                    && (specifier.version.empty() || package.version == specifier.version);

            if (found)
                break;
        }

    if (!found)
    {
        std::cerr << "No package '" << specifier.id << ':' << specifier.version << "'." << std::endl;
        return 1;
    }

    fragment_t fragment;
    if (specifier.fragment.empty())
        fragment = package.main;
    else
    {
        if (!package.fragments.contains(std::string(specifier.fragment)))
        {
            std::cerr
                    << "No fragment '"
                    << specifier.fragment
                    << "' in package '"
                    << specifier.id
                    << ':'
                    << specifier.version
                    << "'."
                    << std::endl;
            return 1;
        }

        fragment = package.fragments.at(std::string(specifier.fragment));
    }

    auto cache_key = package.id
                     + (specifier.fragment.empty() ? "" : '-' + std::string(specifier.fragment))
                     + '-' + package.version;
    auto work_dir = config.cache / cache_key;

    auto cache_dir = config.cache / package.id / package.version;
    auto package_cache_dir = cache_dir / "__package__";
    auto fragment_cache_dir = cache_dir / (specifier.fragment.empty() ? "__fragment__" : specifier.fragment);

    std::filesystem::create_directories(work_dir);
    if (!std::filesystem::exists(work_dir))
    {
        std::cerr << "Install failed: failed to create work directory '" << work_dir.string() << "'." << std::endl;
        return 1;
    }

    if (std::filesystem::exists(package_cache_dir))
        copy_cache(package_cache_dir, work_dir, package.cache);
    else
    {
        for (auto &step : package.setup)
            if (auto code = exec(package, fragment, step, work_dir))
            {
                std::cerr
                        << "Install failed: setup step '"
                        << step
                        << "' exited with non-zero code "
                        << code
                        << "."
                        << std::endl;

                std::filesystem::remove_all(work_dir);
                return 1;
            }

        std::filesystem::create_directories(package_cache_dir);

        if (std::filesystem::exists(work_dir))
            copy_cache(work_dir, package_cache_dir, package.cache);
        else
            std::cerr
                    << "Warning: failed to create cache directory '"
                    << package_cache_dir.string()
                    << "'."
                    << std::endl;
    }

    if (std::filesystem::exists(fragment_cache_dir))
        copy_cache(fragment_cache_dir, work_dir, fragment.cache);

    for (auto &step : fragment.configure)
        if (auto code = exec(package, step, work_dir))
        {
            std::cerr
                    << "Install failed: configure step '"
                    << step
                    << "' exited with non-zero code "
                    << code
                    << "."
                    << std::endl;

            std::filesystem::remove_all(work_dir);
            return 1;
        }

    for (auto &step : fragment.build)
        if (auto code = exec(package, fragment, step, work_dir))
        {
            std::cerr
                    << "Install failed: build step '"
                    << step
                    << "' exited with non-zero code "
                    << code
                    << "."
                    << std::endl;

            std::filesystem::remove_all(work_dir);
            return 1;
        }

    if (std::filesystem::exists(fragment_cache_dir))
        copy_cache(work_dir, fragment_cache_dir, fragment.cache);
    else
    {
        std::filesystem::create_directories(fragment_cache_dir);

        if (std::filesystem::exists(fragment_cache_dir))
            copy_cache(work_dir, fragment_cache_dir, fragment.cache);
        else
            std::cerr
                    << "Warning: failed to create cache directory '"
                    << fragment_cache_dir.string()
                    << "'."
                    << std::endl;
    }

    for (auto &step : fragment.install)
        if (auto code = exec(package, fragment, step, work_dir))
        {
            std::cerr
                    << "Install failed: install step '"
                    << step
                    << "' exited with non-zero code "
                    << code
                    << "."
                    << std::endl;

            std::filesystem::remove_all(work_dir);
            return 1;
        }

    std::vector<std::string> installed;
    for (auto &[type, path] : fragment.files)
    {
        switch (type)
        {
        case file_reference_type::file:
            installed.push_back(path);
            break;
        case file_reference_type::manifest:
        {
            auto manifest = read_manifest(path);
            for (auto &entry : manifest)
                installed.push_back(std::move(entry));
            break;
        }
        }
    }

    config.installed[cache_key] = std::move(installed);

    std::filesystem::remove_all(work_dir);
    return 0;
}

static int remove(config_t &config, const std::string_view arg)
{
    const specifier_t specifier(arg);

    // remove files specified by package

    return 0;
}

static int update(config_t &config, const std::string_view arg = {})
{
    const specifier_t specifier(arg);

    // install without cache

    return 0;
}

/*
 * spkg [help|h]
 * spkg (list|l)
 * spkg (install|i) <id>[/<fragment>][:<version>]
 * spkg (remove|r) <id>[/<fragment>]
 * spkg (update|u) [<id>[/<fragment>]]
 */

int main(const int argc, const char **argv)
{
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    if (args.empty() || ((args[0] == "help" || args[0] == "h") && args.size() == 1))
        return help();

    auto config = get_config();
    auto code = -1;

    if (args[0] == "list" || args[0] == "l")
    {
        if (args.size() == 1)
            code = list(config);
    }
    else if (args[0] == "install" || args[0] == "i")
    {
        if (args.size() == 2)
            code = install(config, args[1]);
    }
    else if (args[0] == "remove" || args[0] == "r")
    {
        if (args.size() == 2)
            code = remove(config, args[1]);
    }
    else if (args[0] == "update" || args[0] == "u")
    {
        if (args.size() == 1)
            code = update(config);
        else if (args.size() == 2)
            code = update(config, args[1]);
    }

    if (code < 0)
    {
        std::cerr << "Invalid arguments. Use '" << argv[0] << " help' to print the manual." << std::endl;
        return 1;
    }

    set_config(config);
}
