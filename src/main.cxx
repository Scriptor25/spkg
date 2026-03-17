#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>

#include <json.hxx>

template<typename T>
bool get_opt(const json::Node &node, T &value, T default_value = {});

template<typename T>
bool from_json(const json::Node &node, std::set<T> &value);

template<>
bool from_json(const json::Node &node, command_step_t &value)
{
    if (std::string file; node >> file)
    {
        value = { .command = { std::move(file) } };
        return true;
    }

    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= object_node["command"] >> value.command;

    ok &= get_opt(object_node["dir"], value.dir);
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
bool from_json(const json::Node &node, file_reference_type &value)
{
    static const std::map<std::string, file_reference_type> m
    {
        { "file", file_reference_type::file },
        { "manifest", file_reference_type::manifest },
    };

    if (std::string s; node >> s)
    {
        if (!m.contains(s))
            return false;

        value = m.at(s);
        return true;
    }

    return false;
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

template<typename T>
bool from_json(const json::Node &node, std::set<T> &value)
{
    if (std::vector<T> v; node >> v)
    {
        value = { v.begin(), v.end() };
        return true;
    }

    return false;
}

template<typename T>
bool get_opt(const json::Node &node, T &value, T default_value)
{
    if (std::optional<T> opt; node >> opt)
    {
        value = std::move(opt.value_or(std::move(default_value)));
        return true;
    }
    
    return false;
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

    std::filesystem::create_directories(path.parent_path());
    if (!std::filesystem::exists(path.parent_path()))
        return log_error("failed to create config parent directory '{}'", path.parent_path().string());

    if (!std::filesystem::is_directory(path.parent_path()))
        return log_error("config parent path '{}' is not a directory", path.parent_path().string());

    std::ofstream stream(path);
    if (!stream)
        return log_error("failed open config file '{}'", path.string());

    json::Node json;
    to_json(json, value);
    stream << json;

    return 0;
}

static std::vector<std::string> read_manifest(const std::filesystem::path &path)
{
    if (!std::filesystem::exists(path))
    {
        log_warning("manifest path '{}' does not exist", path.string());
        return {};
    }
    
    if (std::filesystem::is_directory(path))
    {
        log_warning("manifest path '{}' is a directory", path.string());
        return {};
    }

    std::ifstream stream(path);
    if (!stream)
    {
        log_warning("failed to open manifest file '{}'", path.string());
        return {};
    }

    std::vector<std::string> manifest;
    for (std::string line; std::getline(stream, line);)
        manifest.push_back(std::move(line));
    return manifest;
}

static int help()
{
    std::cout <<
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
    {
        if (!std::filesystem::exists(path))
        {
            log_warning("package repository path '{}' in config does not exist", path.string());
            continue;
        }

        if (!std::filesystem::is_directory(path))
        {
            log_warning("package repository path '{}' in config is not a directory", path.string());
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

            package_t package;
            if (!(node >> package))
            {
                log_warning("invalid package json in file '{}'", entry.path().string());
                continue;
            }

            std::cout << package.id << ':' << package.version << std::endl;
            std::cout << package.name << " - " << package.description << std::endl;
            
            for (auto &[key, fragment] : package.fragments)
            {
                std::cout << "   - /" << key << std::endl;
                std::cout << "     " << fragment.name << " - " << fragment.description << std::endl;
            }
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
    std::map<std::string, std::string> envs;
    envs.insert(package.env.begin(), package.env.end());
    envs.insert(step.env.begin(), step.env.end());

    return exec(work_dir / step.dir, step.command[0], step.command, envs);
}

static int exec(
    const package_t &package,
    const fragment_t &fragment,
    const command_step_t &step,
    const std::filesystem::path &work_dir)
{
    std::map<std::string, std::string> envs;
    envs.insert(package.env.begin(), package.env.end());
    envs.insert(fragment.env.begin(), fragment.env.end());
    envs.insert(step.env.begin(), step.env.end());

    return exec(work_dir / fragment.dir / step.dir, step.command[0], step.command, envs);
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

            if (!(node >> package))
            {
                log_warning("invalid package json in file '{}'", entry.path().string());
                continue;
            }

            found = package.id == specifier.id
                    && (specifier.version.empty() || package.version == specifier.version);

            if (found)
                break;
        }

    if (!found)
        return log_error("no package '{}:{}'", specifier.id, specifier.version);

    const fragment_t *fragment_ptr;
    if (specifier.fragment.empty())
        fragment_ptr = &package.main;
    else
    {
        if (!package.fragments.contains(std::string(specifier.fragment)))
            return log_error("no fragment '{}' in package '{}:{}'", specifier.fragment, specifier.id, specifier.version);

        fragment_ptr = &package.fragments.at(std::string(specifier.fragment));
    }

    auto &fragment = *fragment_ptr;

    auto cache_key = package.id
                     + (specifier.fragment.empty() ? "" : '-' + std::string(specifier.fragment))
                     + '-' + package.version;
    auto work_dir = config.cache / cache_key;

    auto cache_dir = config.cache / package.id / package.version;
    auto package_cache_dir = cache_dir / "__package__";
    auto fragment_cache_dir = cache_dir / (specifier.fragment.empty() ? "__fragment__" : specifier.fragment);

    std::filesystem::create_directories(work_dir);
    if (!std::filesystem::exists(work_dir))
        return log_error("failed to create work directory '{}'", work_dir.string());

    if (std::filesystem::exists(package_cache_dir))
        copy_cache(package_cache_dir, work_dir, package.cache);
    else
    {
        for (auto &step : package.setup)
            if (auto code = exec(package, fragment, step, work_dir))
            {
                std::filesystem::remove_all(work_dir);
                return log_error("setup step '{}' exited with non-zero code {}", step, code);
            }

        std::filesystem::create_directories(package_cache_dir);

        if (std::filesystem::exists(work_dir))
            copy_cache(work_dir, package_cache_dir, package.cache);
        else
            log_warning("failed to create cache directory '{}'", package_cache_dir.string());
    }

    if (std::filesystem::exists(fragment_cache_dir))
        copy_cache(fragment_cache_dir, work_dir, fragment.cache);

    for (auto &step : fragment.configure)
        if (auto code = exec(package, step, work_dir))
        {
            std::filesystem::remove_all(work_dir);
            return log_error("configure step '{}' exited with non-zero code {}", step, code);
        }

    for (auto &step : fragment.build)
        if (auto code = exec(package, fragment, step, work_dir))
        {
            std::filesystem::remove_all(work_dir);
            return log_error("build step '{}' exited with non-zero code {}", step, code);
        }

    if (std::filesystem::exists(fragment_cache_dir))
        copy_cache(work_dir, fragment_cache_dir, fragment.cache);
    else
    {
        std::filesystem::create_directories(fragment_cache_dir);

        if (std::filesystem::exists(fragment_cache_dir))
            copy_cache(work_dir, fragment_cache_dir, fragment.cache);
        else
            log_warning("failed to create cache directory '{}'", fragment_cache_dir.string());
    }

    for (auto &step : fragment.install)
        if (auto code = exec(package, fragment, step, work_dir))
        {
            std::filesystem::remove_all(work_dir);
            return log_error("install step '{}' exited with non-zero code {}", step, code);
        }

    auto &installed = config.installed[cache_key];
    for (auto &[type, path] : fragment.files)
    {
        switch (type)
        {
        case file_reference_type::file:
            installed.insert(path);
            break;
        case file_reference_type::manifest:
        {
            auto manifest = read_manifest(work_dir / fragment.dir / path);
            for (auto &entry : manifest)
                installed.insert(std::move(entry));
            break;
        }
        }
    }

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
