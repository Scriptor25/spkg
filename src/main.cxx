#include <filesystem>
#include <fstream>
#include <iostream>

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

struct fragment_t
{
    std::string name;
    std::string description;
    std::map<std::string, std::string> env;
    std::string dir;
    std::vector<std::string> cache;
    std::vector<std::string> configure;
    std::vector<std::string> build;
    std::vector<std::string> install;
    std::vector<file_reference_t> files;
};

struct package_t
{
    std::string id;
    std::string version;
    std::string name;
    std::string description;
    std::map<std::string, std::string> env;
    std::vector<std::string> setup;
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

bool CommandListFromJson(const spkg::json::JsonNode &node, std::vector<std::string> &value)
{
    if (spkg::json::IsUndefined(node))
        return true;

    if (spkg::json::IsString(node))
    {
        auto &string_node = spkg::json::AsString(node);
        value.push_back(string_node);
        return true;
    }

    if (spkg::json::IsArray(node))
    {
        auto &array_node = spkg::json::AsArray(node);

        for (auto &element : array_node)
        {
            if (spkg::json::IsString(element))
            {
                auto &string_node = spkg::json::AsString(element);
                value.push_back(string_node);
                continue;
            }

            auto segments = spkg::json::As<std::vector<std::string>>(element);

            std::string line;
            for (auto it = segments.begin(); it != segments.end(); ++it)
            {
                if (it != segments.begin())
                    line += ' ';
                line += *it;
            }

            value.push_back(std::move(line));
        }

        return true;
    }

    return false;
}

template<>
bool FromJson(const spkg::json::JsonNode &node, file_reference_t &value)
{
    if (spkg::json::IsString(node))
    {
        value = {
            .type = file_reference_type::file,
            .path = spkg::json::AsString(node),
        };
        return true;
    }

    if (!spkg::json::IsObject(node))
        return false;

    auto &object_node = spkg::json::AsObject(node);

    const auto type_node = object_node["type"];
    const auto path_node = object_node["path"];

    const auto type_string = spkg::json::As<std::string>(type_node);
    if (type_string == "file")
        value.type = file_reference_type::file;
    else if (type_string == "manifest")
        value.type = file_reference_type::manifest;
    else
        return false;

    value.path = spkg::json::As<std::string>(path_node);

    return true;
}

template<>
bool FromJson(const spkg::json::JsonNode &node, fragment_t &value)
{
    if (!spkg::json::IsObject(node))
        return false;

    auto &object_node = spkg::json::AsObject(node);

    const auto name_node = object_node["name"];
    const auto description_node = object_node["description"];
    const auto env_node = object_node["env"];
    const auto dir_node = object_node["dir"];
    const auto cache_node = object_node["cache"];
    const auto configure_node = object_node["configure"];
    const auto build_node = object_node["build"];
    const auto install_node = object_node["install"];
    const auto files_node = object_node["files"];

    value.name = spkg::json::As<std::optional<std::string>>(name_node).value_or({});
    value.description = spkg::json::As<std::optional<std::string>>(description_node).value_or({});
    value.env = spkg::json::As<std::optional<std::map<std::string, std::string>>>(env_node).value_or({});
    value.dir = spkg::json::As<std::optional<std::string>>(dir_node).value_or({});

    value.cache = spkg::json::As<std::optional<std::vector<std::string>>>(cache_node).value_or({});

    if (!CommandListFromJson(configure_node, value.configure))
        return false;
    if (!CommandListFromJson(build_node, value.build))
        return false;
    if (!CommandListFromJson(install_node, value.install))
        return false;

    value.files = spkg::json::As<std::optional<std::vector<file_reference_t>>>(files_node).value_or({});

    return true;
}

template<>
bool FromJson(const spkg::json::JsonNode &node, package_t &value)
{
    if (!spkg::json::IsObject(node))
        return false;

    auto &object_node = spkg::json::AsObject(node);

    const auto id_node = object_node["id"];
    const auto version_node = object_node["version"];
    const auto name_node = object_node["name"];
    const auto description_node = object_node["description"];
    const auto env_node = object_node["env"];
    const auto setup_node = object_node["setup"];
    const auto cache_node = object_node["cache"];
    const auto main_node = object_node["main"];
    const auto fragments_node = object_node["fragments"];

    value.id = spkg::json::As<std::string>(id_node);
    value.version = spkg::json::As<std::string>(version_node);
    value.name = spkg::json::As<std::optional<std::string>>(name_node).value_or({});
    value.description = spkg::json::As<std::optional<std::string>>(description_node).value_or({});

    value.env = spkg::json::As<std::optional<std::map<std::string, std::string>>>(env_node).value_or({});

    if (!CommandListFromJson(setup_node, value.setup))
        return false;

    value.cache = spkg::json::As<std::optional<std::vector<std::string>>>(cache_node).value_or({});
    value.main = spkg::json::As<fragment_t>(main_node);
    value.fragments = spkg::json::As<std::optional<std::map<std::string, fragment_t>>>(fragments_node).value_or({});

    return true;
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
bool FromJson(const spkg::json::JsonNode &node, std::filesystem::path &value)
{
    if (!spkg::json::IsString(node))
        return false;

    value = spkg::json::AsString(node);
    return true;
}

template<>
void ToJson(spkg::json::JsonNode &node, const std::filesystem::path &value)
{
    ToJson(node, value.string());
}

template<>
bool FromJson(const spkg::json::JsonNode &node, config_t &value)
{
    if (!spkg::json::IsObject(node))
        return false;

    auto &object_node = spkg::json::AsObject(node);

    const auto packages_node = object_node["packages"];
    const auto cache_node = object_node["cache"];
    const auto installed_node = object_node["installed"];

    value.packages = spkg::json::As<std::optional<std::vector<std::filesystem::path>>>(packages_node)
            .value_or({ get_packages_path() });

    value.cache = spkg::json::As<std::optional<std::filesystem::path>>(cache_node)
            .value_or(get_cache_path());

    value.installed = spkg::json::As<std::optional<std::map<std::string, std::vector<std::string>>>>(installed_node)
            .value_or({});

    return true;
}

template<>
void ToJson(spkg::json::JsonNode &node, const config_t &value)
{
    spkg::json::JsonObjectNode object_node;

    ToJson(object_node["packages"], value.packages);
    ToJson(object_node["cache"], value.cache);
    ToJson(object_node["installed"], value.installed);

    node = std::move(object_node);
}

static config_t get_config()
{
    if (std::ifstream stream(get_config_path() / "config.json"); stream)
        return spkg::json::Parse<config_t>(stream);

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

    spkg::json::JsonNode json;
    ToJson(json, value);
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

            auto package = spkg::json::Parse<package_t>(stream);

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
    const std::string &file,
    const std::vector<std::string> &args,
    const std::filesystem::path &dir,
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

std::vector<std::string> split_command_line(const std::string &line)
{
    std::vector<std::string> args;
    std::string current;

    auto single_quote = false;
    auto double_quote = false;
    auto escape = false;

    for (const auto c : line)
    {
        if (escape)
        {
            current += c;
            escape = false;
            continue;
        }

        if (c == '\\')
        {
            escape = true;
            continue;
        }

        if (c == '"' && !single_quote)
        {
            double_quote = !double_quote;
            continue;
        }

        if (c == '\'' && !double_quote)
        {
            single_quote = !single_quote;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c)) && !single_quote && !double_quote)
        {
            if (!current.empty())
            {
                args.push_back(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.empty())
        args.push_back(current);

    return args;
}

static int exec(
    const std::string &line,
    const std::filesystem::path &dir,
    const std::map<std::string, std::string> &envs)
{
    const auto args = split_command_line(line);
    return exec(args[0], args, dir, envs);
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

            package = spkg::json::Parse<package_t>(stream);
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
    auto work_path = config.cache / cache_key;
    auto package_cache_path = config.cache / package.id / package.version / "__package__";
    auto fragment_cache_path = config.cache / package.id / package.version / (specifier.fragment.empty()
                                                                                  ? "__fragment__"
                                                                                  : specifier.fragment);

    std::filesystem::create_directories(work_path);
    if (!std::filesystem::exists(work_path))
    {
        std::cerr << "Install failed: failed to create work directory '" << work_path.string() << "'." << std::endl;
        return 1;
    }

    if (std::filesystem::exists(package_cache_path))
        copy_cache(package_cache_path, work_path, package.cache);
    else
    {
        for (auto &step : package.setup)
            if (auto code = exec(step, work_path, package.env))
            {
                std::cerr
                        << "Install failed: setup step '"
                        << step
                        << "' exited with non-zero error code "
                        << code
                        << "."
                        << std::endl;

                std::filesystem::remove_all(work_path);
                return 1;
            }

        std::filesystem::create_directories(package_cache_path);

        if (std::filesystem::exists(work_path))
            copy_cache(work_path, package_cache_path, package.cache);
        else
            std::cerr
                    << "Warning: failed to create cache directory '"
                    << package_cache_path.string()
                    << "'."
                    << std::endl;
    }

    if (std::filesystem::exists(fragment_cache_path))
        copy_cache(fragment_cache_path, work_path, fragment.cache);

    for (auto &step : fragment.configure)
        if (auto code = exec(step, work_path / fragment.dir, fragment.env))
        {
            std::cerr
                    << "Install failed: configure step '"
                    << step
                    << "' exited with non-zero error code "
                    << code
                    << "."
                    << std::endl;

            std::filesystem::remove_all(work_path);
            return 1;
        }

    for (auto &step : fragment.build)
        if (auto code = exec(step, work_path / fragment.dir, fragment.env))
        {
            std::cerr
                    << "Install failed: build step '"
                    << step
                    << "' exited with non-zero error code "
                    << code
                    << "."
                    << std::endl;

            std::filesystem::remove_all(work_path);
            return 1;
        }

    if (std::filesystem::exists(fragment_cache_path))
        copy_cache(work_path, fragment_cache_path, fragment.cache);
    else
    {
        std::filesystem::create_directories(fragment_cache_path);

        if (std::filesystem::exists(fragment_cache_path))
            copy_cache(work_path, fragment_cache_path, fragment.cache);
        else
            std::cerr
                    << "Warning: failed to create cache directory '"
                    << fragment_cache_path.string()
                    << "'."
                    << std::endl;
    }

    for (auto &step : fragment.install)
        if (auto code = exec(step, work_path / fragment.dir, fragment.env))
        {
            std::cerr
                    << "Install failed: install step '"
                    << step
                    << "' exited with non-zero error code "
                    << code
                    << "."
                    << std::endl;

            std::filesystem::remove_all(work_path);
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

    std::filesystem::remove_all(work_path);
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
