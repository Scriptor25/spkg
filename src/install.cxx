#include "persist.hxx"
#include <context.hxx>
#include <log.hxx>
#include <spkg.hxx>

#include <unistd.h>
#include <sys/wait.h>

#include <fstream>
#include <ranges>
#include <utility>

static int copy_files(const std::filesystem::path &from, const std::filesystem::path &to)
{
    std::error_code ec;

    auto status = std::filesystem::status(from, ec);
    if (ec)
        return spkg::Error(
            "failed to get status for '{}': {} ({})",
            from.string(),
            ec.message(),
            ec.value());

    switch (status.type())
    {
    case std::filesystem::file_type::regular:
        if (std::filesystem::create_directories(to.parent_path(), ec); ec)
            return spkg::Error(
                "failed to create directory '{}': {} ({})",
                to.parent_path().string(),
                ec.message(),
                ec.value());

        if (std::filesystem::copy_file(
            from,
            to,
            std::filesystem::copy_options::overwrite_existing,
            ec); ec)
            return spkg::Error(
                "failed to copy file from '{}' to '{}': {} ({})",
                from.string(),
                to.string(),
                ec.message(),
                ec.value());

        if (std::filesystem::permissions(
            to,
            status.permissions(),
            std::filesystem::perm_options::replace,
            ec); ec)
            return spkg::Error(
                "failed to set permissions for file '{}': {} ({})",
                to.string(),
                ec.message(),
                ec.value());

        if (std::filesystem::last_write_time(
            to,
            std::filesystem::last_write_time(from),
            ec); ec)
            return spkg::Error(
                "failed to set last write time for file '{}': {} ({})",
                to.string(),
                ec.message(),
                ec.value());

        break;

    case std::filesystem::file_type::directory:
        if (std::filesystem::create_directories(to, ec); ec)
            return spkg::Error(
                "failed to create directory '{}': {} ({})",
                to.string(),
                ec.message(),
                ec.value());

        for (auto &entry : std::filesystem::directory_iterator(from))
        {
            auto &source = entry.path();
            auto relative = std::filesystem::relative(source, from);
            auto target = to / relative;

            if (const auto error = copy_files(source, target))
                return error;
        }

        break;

    case std::filesystem::file_type::symlink:
        if (std::filesystem::copy_symlink(
            from,
            to,
            ec); ec)
            return spkg::Error(
                "failed to copy symlink from '{}' to '{}': {} ({})",
                from.string(),
                to.string(),
                ec.message(),
                ec.value());

        break;

    default:
        break;
    }

    return 0;
}

static int copy_cache(
    const std::filesystem::path &src_path,
    const std::filesystem::path &dst_path,
    const std::vector<std::string> &cache)
{
    for (auto &entry : cache)
    {
        auto from = src_path / entry;
        auto to = dst_path / entry;

        if (!std::filesystem::exists(from))
        {
            spkg::Warning("cache entry source '{}' does not exist", from.string());
            continue;
        }

        if (std::filesystem::exists(to))
            if (std::error_code ec; std::filesystem::remove_all(to, ec), ec)
                return spkg::Error(
                    "failed to remove cache entry target '{}': {} ({})",
                    to.string(),
                    ec.message(),
                    ec.value());

        if (const auto error = copy_files(from, to))
            return error;
    }

    return 0;
}

static void remove_work_directory(const std::filesystem::path &path)
{
    spkg::Info("removing work directory '{}'", path.string());
    if (std::error_code ec; std::filesystem::remove_all(path, ec), ec)
        spkg::Warning(
            "failed to remove work directory '{}': {} ({})",
            path.string(),
            ec.message(),
            ec.value());
}

static void remove_cache_directory(const std::filesystem::path &path)
{
    spkg::Info("removing cache directory '{}'", path.string());
    if (std::error_code ec; std::filesystem::remove_all(path, ec), ec)
        spkg::Warning(
            "failed to remove cache directory '{}': {} ({})",
            path.string(),
            ec.message(),
            ec.value());
}

static std::string trim(const std::string &s)
{
    auto b = s.begin();
    auto e = s.end();

    while (b != e && std::isspace(static_cast<unsigned char>(*b)))
        ++b;

    while (b != e && std::isspace(static_cast<unsigned char>(*(e - 1))))
        --e;

    return { b, e };
}

static std::string rtrim(const std::string &s)
{
    auto b = s.begin();
    auto e = s.end();

    while (b != e && std::isspace(static_cast<unsigned char>(*(e - 1))))
        --e;

    return { b, e };
}

static int read_manifest(const std::filesystem::path &path, std::vector<std::string> &manifest)
{
    if (!std::filesystem::exists(path))
        return spkg::Error("manifest path '{}' does not exist", path.string());

    if (std::filesystem::is_directory(path))
        return spkg::Error("manifest path '{}' is a directory", path.string());

    std::ifstream stream(path);
    if (!stream)
        return spkg::Error("failed to open manifest path '{}'", path.string());

    for (std::string line; std::getline(stream, line);)
    {
        line = trim(line);
        if (!line.empty())
            manifest.push_back(std::move(line));
    }

    return 0;
}

static int read_map_manifest(const std::filesystem::path &path, spkg::PersistMap &manifest)
{
    std::vector<std::string> vec;
    if (const auto error = read_manifest(path, vec))
        return error;

    std::map<std::string, std::string> m;
    for (auto &line : vec)
    {
        const auto pos = line.find('=');
        if (pos == std::string::npos)
            return spkg::Error("invalid manifest line '{}'", line);

        auto key = line.substr(0, pos);
        auto val = line.substr(pos + 1);

        m[key] = std::move(val);
    }

    std::set<std::string> size_keys;
    for (auto &key : m | std::views::keys)
        if (key.ends_with(".size"))
            size_keys.insert(key);

    for (auto &key : size_keys)
    {
        auto size = std::stoull(m[key]);
        auto base = key.substr(0, key.find_last_of(".size"));

        spkg::PersistVec persist(size);
        for (std::size_t i = 0; i < size; ++i)
        {
            auto index = base + '[' + std::to_string(i) + ']';
            persist.push_back(std::move(m[index]));
            m.erase(index);
        }

        manifest[base] = std::move(persist);
    }

    for (auto &[key, val] : m)
        manifest[key] = std::move(val);

    return 0;
}

static int write_manifest(const std::filesystem::path &path, const std::vector<std::string> &manifest)
{
    std::filesystem::create_directories(path.parent_path());

    if (!std::filesystem::exists(path.parent_path()))
        return spkg::Error("failed to create manifest parent path '{}'", path.parent_path().string());

    std::ofstream stream(path);
    if (!stream)
        return spkg::Error("failed to open manifest path '{}'", path.string());

    for (auto &line : manifest)
        if (auto trimmed = trim(line); !trimmed.empty())
            stream << trimmed << std::endl;

    return 0;
}

static int write_map_manifest(const std::filesystem::path &path, const spkg::PersistMap &manifest)
{
    std::vector<std::string> vec;
    for (auto &[key, val] : manifest)
    {
        struct
        {
            auto operator()(const spkg::PersistVal &val) const
            {
                vec.push_back(key + '=' + val);
            }

            auto operator()(const spkg::PersistVec &val) const
            {
                vec.push_back(key + ".size=" + std::to_string(val.size()));
                for (std::size_t i = 0; i < val.size(); ++i)
                    vec.push_back(key + '[' + std::to_string(i) + "]=" + val[i]);
            }

            const std::string &key;
            std::vector<std::string> &vec;
        } visitor{ key, vec };

        std::visit(visitor, val);
    }

    return write_manifest(path, vec);
}

static int exec(
    const std::vector<std::ostream *> &streams,
    const std::filesystem::path &dir,
    const std::string &file,
    const std::vector<std::string> &args,
    const std::map<std::string, std::string> &envs)
{
    int pipes[2];
    if (pipe(pipes) < 0)
        return -1;

    const auto pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0)
    {
        close(pipes[0]);
        dup2(pipes[1], STDOUT_FILENO);
        close(pipes[1]);

        if (auto error = chdir(dir.c_str()))
            _exit(error);

        for (auto &[key, val] : envs)
            setenv(key.c_str(), val.c_str(), 1);

        std::vector<char *> argv;
        argv.reserve(args.size() + 1);

        for (auto &arg : args)
            argv.push_back(const_cast<char *>(arg.c_str()));
        argv.push_back(nullptr);

        execvp(file.c_str(), argv.data());
        _exit(127);
    }

    close(pipes[1]);

    char buffer[4096];
    for (ssize_t n; (n = read(pipes[0], buffer, sizeof(buffer))) > 0;)
    {
        if (streams.empty())
        {
            if (auto count = write(STDOUT_FILENO, buffer, n); count < 0)
                spkg::Warning("failed to write to stdout ({})", count);
            else if (count != n)
                spkg::Warning("failed to write full buffer to stdout");
            continue;
        }

        for (auto &stream : streams)
            stream->write(buffer, n);
    }
    close(pipes[0]);

    auto status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return -1;
}

static int execute_command_once(
    spkg::Context &context,
    const std::string &key_base,
    const spkg::Command &command,
    const std::filesystem::path &work_dir,
    const std::map<std::string, std::string> &envs)
{
    auto args = command.Args;
    for (auto &arg : args)
    {
        std::set<std::string> keys;
        for (std::size_t pos = 0; (pos = arg.find("$(", pos)) != std::string::npos;)
        {
            const auto end = arg.find(")", pos);
            if (end == std::string::npos)
                break;

            auto key = arg.substr(pos + 2, end - (pos + 2));

            std::string default_value;
            if (const auto p = key.find("?:"); p != std::string::npos)
            {
                default_value = key.substr(p + 2);
                key = key.substr(0, p);
            }

            if (key.front() == '.')
                key = key_base + std::move(key);

            if (keys.contains(key))
            {
                spkg::Warning("recursive variable expansion for key '{}'", key);
                pos = end + 1;
                continue;
            }

            keys.insert(key);

            if (spkg::PersistVal value; context.GetVariable(key, value))
                arg.replace(pos, end - pos + 1, value);
            else
            {
                arg.replace(pos, end - pos + 1, default_value);
                if (default_value.empty())
                    spkg::Warning("variable '{}' is not defined", key);
            }
        }
    }

    std::vector<std::ostream *> streams;

    std::unique_ptr<std::stringstream> capture_stream;
    if (command.Capture)
    {
        capture_stream = std::make_unique<std::stringstream>();
        streams.push_back(capture_stream.get());
    }

    std::unique_ptr<std::ofstream> output_stream;
    if (command.Output)
    {
        output_stream = std::make_unique<std::ofstream>(work_dir / *command.Output);
        streams.push_back(output_stream.get());
    }

    const auto result = exec(streams, work_dir, args[0], args, envs);

    if (command.Capture)
    {
        auto &frame = context.Stack.back();

        auto key = command.Capture->Name;
        if (key.front() == '.')
            key = key_base + key;

        if (command.Capture->Array)
        {
            spkg::PersistVec vec;
            for (std::string line; std::getline(*capture_stream, line);)
            {
                line = trim(line);
                if (line.empty())
                    continue;

                vec.push_back(std::move(line));
            }

            frame[key] = std::move(vec);
        }
        else
            frame[key] = rtrim(capture_stream->str());
    }

    return result;
}

static int execute_command(
    spkg::Context &context,
    const std::string &key_base,
    const spkg::Command &command,
    const std::filesystem::path &work_dir,
    const std::map<std::string, std::string> &envs)
{
    if (command.ForEach)
    {
        auto &frame = context.Stack.back();

        auto var = command.ForEach->Var;
        if (var.front() == '.')
            var = key_base + var;

        auto of = command.ForEach->Of;
        if (of.front() == '.')
            of = key_base + of;

        spkg::PersistEntry entry;
        if (!context.GetVariable(of, entry))
        {
            frame.erase(var);
            spkg::Warning("variable '{}' is not defined", of);
            return 0;
        }

        if (const auto p = std::get_if<spkg::PersistVec>(&entry))
        {
            for (const auto &value : *p)
            {
                frame[var] = value;

                if (const auto result = execute_command_once(context, key_base, command, work_dir, envs))
                    return result;
            }

            return 0;
        }

        const auto &value = std::get<spkg::PersistVal>(entry);

        frame[var] = value;
        spkg::Warning("variable '{}' is not iterable", of);
    }

    return execute_command_once(context, key_base, command, work_dir, envs);
}

static int execute_command(
    spkg::Context &context,
    const spkg::Package &package,
    const spkg::Fragment *fragment,
    const spkg::Step &step,
    const spkg::Command &command,
    const std::filesystem::path &work_dir)
{
    std::map<std::string, std::string> envs;
    envs.insert(package.Env.begin(), package.Env.end());
    if (fragment)
        envs.insert(fragment->Env.begin(), fragment->Env.end());
    envs.insert(step.Env.begin(), step.Env.end());
    envs.insert(command.Env.begin(), command.Env.end());

    std::filesystem::path dir;
    if (fragment)
        dir = work_dir / fragment->Dir / step.Dir / command.Dir;
    else
        dir = work_dir / step.Dir / command.Dir;

    return execute_command(context, step.Id, command, dir, envs);
}

static void restore(
    const std::set<std::string> &keys,
    const std::string &key_base,
    const spkg::PersistMap &values,
    spkg::PersistMap &frame)
{
    for (auto key : keys)
    {
        if (key.front() == '.')
            key = key_base + std::move(key);

        if (auto it = values.find(key); it != values.end())
        {
            frame[key] = it->second;
            continue;
        }

        spkg::Warning("variable '{}' is not defined", key);
    }
}

static void save(
    const std::set<std::string> &keys,
    const std::string &key_base,
    spkg::PersistMap &values,
    const spkg::PersistMap &frame)
{
    for (auto key : keys)
    {
        if (key.front() == '.')
            key = key_base + std::move(key);

        if (auto it = frame.find(key); it != frame.end())
        {
            values[key] = it->second;
            continue;
        }

        spkg::Warning("variable '{}' is not defined", key);
    }
}

static int execute_steps(
    spkg::Context &context,
    const spkg::Fragment *fragment,
    const std::filesystem::path &base_cache_dir,
    const std::vector<spkg::Step> &steps,
    bool &any)
{
    const auto use_cache = context.UseCache;
    const auto remove = context.Remove;

    const auto &package = context.Pkg;
    const auto &work_dir = context.WorkDir;

    const auto fragment_work_dir = fragment ? work_dir / fragment->Dir : work_dir;

    any = false;

    for (auto &step : steps)
    {
        const auto frame_index = context.Stack.size();
        {
            auto &frame = context.Stack.emplace_back();
            frame[step.Id + ".dir"] = step.Dir;

            if ((!use_cache || remove) && !step.Persist.empty())
            {
                spkg::Info("restoring step '{}'", step.Id);
                restore(step.Persist, step.Id, context.Persist, frame);
            }
        }

        if (step.Remove != remove)
            continue;

        any = true;

        auto step_work_dir = fragment_work_dir / step.Dir;

        auto step_cache_dir = base_cache_dir / "__step__" / step.Id;
        auto step_manifest = step_cache_dir / "__manifest__";

        const auto step_cache_exists = std::filesystem::exists(step_cache_dir);
        const auto step_manifest_exists = std::filesystem::exists(step_manifest);

        const auto step_has_cache = !step.Cache.empty();

        if (use_cache && step_cache_exists)
        {
            if (step_has_cache)
            {
                spkg::Info("restoring cache from '{}'", step_cache_dir.string());
                if (const auto error = copy_cache(step_cache_dir, step_work_dir, step.Cache))
                    return error;
            }

            if (step_manifest_exists)
            {
                spkg::Info("restoring manifest from '{}'", step_manifest.string());
                if (const auto error = read_map_manifest(step_manifest, context.Stack[frame_index]))
                    return error;
            }
        }

        auto cached = use_cache
                      && step_manifest_exists
                      && (step_cache_exists || !step_has_cache)
                      && step.Once;

        if (cached)
            continue;

        spkg::Info("executing step {}", step.Id);
        for (auto &command : step.Run)
            if (auto result = execute_command(context, package, fragment, step, command, work_dir))
                return Error("command '{}' exited with non-zero result {}", command, result);

        if (!step_cache_exists)
        {
            spkg::Info("creating cache directory '{}'", step_cache_dir.string());
            if (std::error_code ec; std::filesystem::create_directories(step_cache_dir, ec), ec)
                return spkg::Error(
                    "failed to create cache directory '{}': {} ({})",
                    step_cache_dir.string(),
                    ec.message(),
                    ec.value());
        }

        if (step_has_cache)
        {
            spkg::Info("saving cache to '{}'", step_cache_dir.string());
            if (const auto error = copy_cache(step_work_dir, step_cache_dir, step.Cache))
            {
                remove_cache_directory(step_cache_dir);
                return error;
            }
        }

        spkg::Info("saving manifest to '{}'", step_manifest.string());
        if (const auto error = write_map_manifest(step_manifest, context.Stack[frame_index]))
        {
            remove_cache_directory(step_cache_dir);
            return error;
        }

        if (!step.Persist.empty())
        {
            auto &frame = context.Stack[frame_index];

            spkg::Info("saving step '{}'", step.Id);
            save(step.Persist, step.Id, context.Persist, frame);
        }
    }

    return 0;
}

static int execute_segment(
    spkg::Context &context,
    const std::size_t frame_index,
    const spkg::Fragment *fragment,
    const std::filesystem::path &base_cache_dir,
    const std::vector<spkg::Step> &steps)
{
    const auto use_cache = context.UseCache;

    const auto manifest = base_cache_dir / "__manifest__";

    if (use_cache && std::filesystem::exists(manifest))
    {
        spkg::Info("restoring manifest from '{}'", manifest.string());
        if (const auto error = read_map_manifest(manifest, context.Stack[frame_index]))
            return error;
    }

    bool any;
    if (const auto error = execute_steps(context, fragment, base_cache_dir, steps, any))
        return error;

    if (!any)
        return 0;

    if (!std::filesystem::exists(base_cache_dir))
    {
        spkg::Info("creating cache directory '{}'", base_cache_dir.string());
        if (std::error_code ec; std::filesystem::create_directories(base_cache_dir, ec), ec)
            return spkg::Error(
                "failed to create cache directory '{}': {} ({})",
                base_cache_dir.string(),
                ec.message(),
                ec.value());
    }

    spkg::Info("saving manifest to '{}'", manifest.string());
    if (const auto error = write_map_manifest(manifest, context.Stack[frame_index]))
    {
        remove_cache_directory(base_cache_dir);
        return error;
    }

    return 0;
}

int spkg::Install(Config &config, Specifier arg, bool use_cache, bool remove)
{
    Package package;
    if (!FindPackage(config, arg, package))
        return Error("no package '{}'", arg.Id);

    Fragment *p_fragment;
    if (arg.Fragment == "default")
        p_fragment = &package.Default;
    else if (auto it = package.Fragments.find(arg.Fragment); it != package.Fragments.end())
        p_fragment = &it->second;
    else
        return Error("no fragment '{}' in package '{}'", arg.Fragment, arg.Id);

    auto &fragment = *p_fragment;

    auto work_dir = config.Cache / (arg.Id + '-' + arg.Fragment);
    auto cache_dir = config.Cache / arg.Id;

    Context context
    {
        .UseCache = use_cache,
        .Remove = remove,
        .Pkg = package,
        .WorkDir = work_dir,
        .Persist = {},
        .Stack = {},
    };

    if (auto it = config.Installed.find(arg); it != config.Installed.end())
        context.Persist = it->second;

    if (!std::filesystem::exists(work_dir))
    {
        Info("creating work directory '{}'", work_dir.string());
        if (std::error_code ec; std::filesystem::create_directories(work_dir, ec), ec)
            return Error(
                "failed to create work directory '{}': {} ({})",
                work_dir.string(),
                ec.message(),
                ec.value());
    }

    auto package_frame_index = context.Stack.size();
    {
        auto &frame = context.Stack.emplace_back();
        frame["package.id"] = package.Id;
        frame["package.name"] = package.Name;
        frame["package.description"] = package.Description;

        for (auto &param : package.Params)
        {
            auto key = '@' + param;

            // TODO: frame[key] = params[param];
        }
    }
    if (auto error = execute_segment(context, package_frame_index, nullptr, cache_dir / "__package__", package.Steps))
    {
        remove_work_directory(work_dir);
        return error;
    }

    auto fragment_frame_index = context.Stack.size();
    {
        auto &frame = context.Stack.emplace_back();
        frame["fragment.name"] = fragment.Name;
        frame["fragment.description"] = fragment.Description;
        frame["fragment.dir"] = fragment.Dir;
    }
    if (auto error = execute_segment(
        context,
        fragment_frame_index,
        &fragment,
        cache_dir / arg.Fragment,
        fragment.Steps))
    {
        remove_work_directory(work_dir);
        return error;
    }

    if (remove)
        config.Installed.erase(arg);
    else
        config.Installed[arg] = std::move(context.Persist);

    remove_work_directory(work_dir);
    return 0;
}
