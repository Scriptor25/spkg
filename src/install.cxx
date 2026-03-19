#include <unistd.h>
#include <sys/wait.h>

#include <ranges>
#include <spkg.hxx>

static int copy_directory(const std::filesystem::path &from, const std::filesystem::path &to)
{
    std::error_code ec;

    for (auto &entry : std::filesystem::directory_iterator(from))
    {
        auto &source = entry.path();
        auto relative = std::filesystem::relative(source, from);
        auto target = to / relative;

        auto status = entry.status(ec);
        if (ec)
            return spkg::Error(
                "failed to get status for entry '{}': {} ({})",
                source.string(),
                ec.message(),
                ec.value());

        switch (status.type())
        {
        case std::filesystem::file_type::regular:
            if (std::filesystem::create_directories(target.parent_path(), ec); ec)
                return spkg::Error(
                    "failed to create directory '{}': {} ({})",
                    target.parent_path().string(),
                    ec.message(),
                    ec.value());

            if (std::filesystem::copy_file(
                source,
                target,
                std::filesystem::copy_options::overwrite_existing,
                ec); ec)
                return spkg::Error(
                    "failed to copy file from '{}' to '{}': {} ({})",
                    source.string(),
                    target.string(),
                    ec.message(),
                    ec.value());

            if (std::filesystem::permissions(
                target,
                status.permissions(),
                std::filesystem::perm_options::replace,
                ec); ec)
                return spkg::Error(
                    "failed to set permissions for file '{}': {} ({})",
                    target.string(),
                    ec.message(),
                    ec.value());

            if (std::filesystem::last_write_time(
                target,
                std::filesystem::last_write_time(source),
                ec); ec)
                return spkg::Error(
                    "failed to set last write time for file '{}': {} ({})",
                    target.string(),
                    ec.message(),
                    ec.value());

            break;

        case std::filesystem::file_type::directory:
            if (std::filesystem::create_directories(target, ec); ec)
                return spkg::Error(
                    "failed to create directory '{}': {} ({})",
                    target.string(),
                    ec.message(),
                    ec.value());

            if (const auto error = copy_directory(source, target))
                return error;

            break;

        case std::filesystem::file_type::symlink:
            if (std::filesystem::copy_symlink(
                source,
                target,
                ec); ec)
                return spkg::Error(
                    "failed to copy symlink from '{}' to '{}': {} ({})",
                    source.string(),
                    target.string(),
                    ec.message(),
                    ec.value());

            break;

        default:
            break;
        }
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

        if (std::filesystem::exists(to))
            if (std::error_code ec; std::filesystem::remove_all(to, ec), ec)
                return spkg::Error(
                    "failed to remove cache entry destination directory '{}': {} ({})",
                    to.string(),
                    ec.message(),
                    ec.value());

        if (const auto error = copy_directory(from, to))
            return error;
    }

    return 0;
}

static void remove_work_directory(const std::filesystem::path &path)
{
    if (std::error_code ec; std::filesystem::remove_all(path, ec), ec)
        spkg::Warning(
            "failed to remove work directory '{}': {} ({})",
            path.string(),
            ec.message(),
            ec.value());
}

static void remove_cache_directory(const std::filesystem::path &path)
{
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

static int read_map_manifest(const std::filesystem::path &path, std::map<std::string, std::string> &manifest)
{
    std::vector<std::string> vec;
    if (const auto error = read_manifest(path, vec))
        return error;

    for (auto &line : vec)
    {
        const auto pos = line.find('=');
        if (pos == std::string::npos)
            return spkg::Error("invalid manifest line '{}'", line);

        auto key = line.substr(0, pos);
        auto val = line.substr(pos + 1);

        manifest[key] = std::move(val);
    }

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

static int write_map_manifest(const std::filesystem::path &path, const std::map<std::string, std::string> &manifest)
{
    std::vector<std::string> vec;
    for (auto &[key, val] : manifest)
        vec.push_back(key + '=' + val);

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

        chdir(dir.c_str());

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
            write(STDOUT_FILENO, buffer, n);
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

static int exec(
    spkg::Context &context,
    const spkg::CommandStep &step,
    const std::filesystem::path &work_dir,
    const std::map<std::string, std::string> &envs)
{
    auto command = step.Command;
    for (auto &segment : command)
    {
        std::set<std::string> keys;
        for (std::size_t pos = 0; (pos = segment.find("$(", pos)) != std::string::npos;)
        {
            const auto end = segment.find(")", pos);
            if (end == std::string::npos)
                break;

            auto key = segment.substr(pos + 2, end - (pos + 2));
            if (keys.contains(key))
            {
                spkg::Warning("recursive variable expansion for key '{}'", key);
                pos = end + 1;
                continue;
            }

            keys.insert(key);

            auto replaced = false;
            for (auto &frame : std::ranges::reverse_view(context.Stack))
                if (auto it = frame.find(key); it != frame.end())
                {
                    segment.replace(pos, end - pos + 1, it->second);
                    replaced = true;
                    break;
                }
            if (!replaced)
                pos = end + 1;
        }
    }

    std::vector<std::ostream *> streams;

    std::unique_ptr<std::ostringstream> capture_stream;
    if (!step.Capture.empty())
    {
        capture_stream = std::make_unique<std::ostringstream>();
        streams.push_back(capture_stream.get());
    }

    std::unique_ptr<std::ofstream> output_stream;
    if (!step.Output.empty())
    {
        output_stream = std::make_unique<std::ofstream>(work_dir / step.Dir / step.Output);
        streams.push_back(output_stream.get());
    }

    const auto code = exec(streams, work_dir / step.Dir, command[0], command, envs);

    if (!step.Capture.empty())
    {
        auto &capture = context.Stack.back()[step.Capture];
        capture = rtrim(capture_stream->str());
    }

    return code;
}

static int exec(
    spkg::Context &context,
    const spkg::Package &package,
    const spkg::CommandStep &step,
    const std::filesystem::path &work_dir)
{
    std::map<std::string, std::string> envs;
    envs.insert(package.Env.begin(), package.Env.end());
    envs.insert(step.Env.begin(), step.Env.end());

    return exec(context, step, work_dir, envs);
}

static int exec(
    spkg::Context &context,
    const spkg::Package &package,
    const spkg::Fragment &fragment,
    const spkg::CommandStep &step,
    const std::filesystem::path &work_dir)
{
    std::map<std::string, std::string> envs;
    envs.insert(package.Env.begin(), package.Env.end());
    envs.insert(fragment.Env.begin(), fragment.Env.end());
    envs.insert(step.Env.begin(), step.Env.end());

    return exec(context, step, work_dir / fragment.Dir, envs);
}

int spkg::Install(Config &config, const std::string_view arg)
{
    const Specifier specifier(arg);

    Package package;
    if (!FindPackage(config, specifier, package))
        return Error("no package '{}:{}'", specifier.Id, specifier.Version);

    const Fragment *fragment_ptr = &package.Main;
    if (!specifier.Fragment.empty())
    {
        if (!package.Fragments.contains(std::string(specifier.Fragment)))
            return Error(
                "no fragment '{}' in package '{}:{}'",
                specifier.Fragment,
                specifier.Id,
                specifier.Version);

        fragment_ptr = &package.Fragments.at(std::string(specifier.Fragment));
    }

    auto &fragment = *fragment_ptr;

    Context context;
    std::error_code ec;

    auto cache_key = package.Id
                     + (specifier.Fragment.empty() ? "" : '-' + std::string(specifier.Fragment))
                     + '-' + package.Version;
    auto work_dir = config.Cache / cache_key;

    auto cache_dir = config.Cache / package.Id / package.Version;

    auto package_cache_dir = cache_dir / "__package__";
    auto fragment_cache_dir = cache_dir / (specifier.Fragment.empty() ? "__fragment__" : specifier.Fragment);

    auto package_manifest_path = package_cache_dir / "manifest.txt";
    auto fragment_manifest_path = fragment_cache_dir / "manifest.txt";

    Info("creating work directory '{}'", work_dir.string());
    if (std::filesystem::create_directories(work_dir, ec); ec)
        return Error(
            "failed to create work directory '{}': {} ({})",
            work_dir.string(),
            ec.message(),
            ec.value());

    auto &package_frame = context.Stack.emplace_back();

    if (std::filesystem::exists(package_cache_dir) && !std::filesystem::is_empty(package_cache_dir))
    {
        Info("restoring package cache from '{}'", package_cache_dir.string());
        if (auto error = copy_cache(package_cache_dir, work_dir, package.Cache))
        {
            remove_work_directory(work_dir);
            return error;
        }

        Info("restoring package manifest from '{}'", package_manifest_path.string());
        if (auto error = read_map_manifest(package_manifest_path, package_frame))
        {
            remove_work_directory(work_dir);
            return error;
        }
    }
    else
    {
        Info("executing package setup");

        for (auto &step : package.Setup)
            if (auto code = exec(context, package, step, work_dir))
            {
                remove_work_directory(work_dir);
                return Error("package setup step '{}' exited with non-zero code {}", step, code);
            }

        Info("creating package cache directory '{}'", package_cache_dir.string());
        if (std::filesystem::create_directories(package_cache_dir, ec); ec)
        {
            remove_work_directory(work_dir);
            return Error(
                "failed to create package cache directory '{}': {} ({})",
                package_cache_dir.string(),
                ec.message(),
                ec.value());
        }

        Info("saving package cache to '{}'", package_cache_dir.string());
        if (auto error = copy_cache(work_dir, package_cache_dir, package.Cache))
        {
            remove_work_directory(work_dir);
            remove_cache_directory(package_cache_dir);
            return error;
        }

        Info("saving package manifest to '{}'", package_manifest_path.string());
        if (auto error = write_map_manifest(package_manifest_path, package_frame))
        {
            remove_work_directory(work_dir);
            remove_cache_directory(package_cache_dir);
            return error;
        }
    }

    auto &fragment_frame = context.Stack.emplace_back();

    if (std::filesystem::exists(fragment_cache_dir))
    {
        Info("restoring fragment cache from '{}'", fragment_cache_dir.string());
        if (auto error = copy_cache(fragment_cache_dir, work_dir, fragment.Cache))
        {
            remove_work_directory(work_dir);
            return error;
        }

        Info("restoring fragment manifest from '{}'", fragment_manifest_path.string());
        if (auto error = read_map_manifest(fragment_manifest_path, fragment_frame))
        {
            remove_work_directory(work_dir);
            return error;
        }
    }

    Info("executing fragment configure");
    for (auto &step : fragment.Configure)
        if (auto code = exec(context, package, fragment, step, work_dir))
        {
            remove_work_directory(work_dir);
            return Error("configure step '{}' exited with non-zero code {}", step, code);
        }

    Info("executing fragment build");
    for (auto &step : fragment.Build)
        if (auto code = exec(context, package, fragment, step, work_dir))
        {
            remove_work_directory(work_dir);
            return Error("build step '{}' exited with non-zero code {}", step, code);
        }

    Info("creating fragment cache directory '{}'", fragment_cache_dir.string());
    if (std::filesystem::create_directories(fragment_cache_dir, ec); ec)
    {
        remove_work_directory(work_dir);
        return Error(
            "failed to create fragment cache directory '{}': {} ({})",
            fragment_cache_dir.string(),
            ec.message(),
            ec.value());
    }

    Info("saving fragment cache to '{}'", fragment_cache_dir.string());
    if (auto error = copy_cache(work_dir, fragment_cache_dir, fragment.Cache))
    {
        remove_work_directory(work_dir);
        remove_cache_directory(fragment_cache_dir);
        return error;
    }

    Info("saving fragment manifest to '{}'", fragment_manifest_path.string());
    if (auto error = write_map_manifest(fragment_manifest_path, fragment_frame))
    {
        remove_work_directory(work_dir);
        remove_cache_directory(fragment_cache_dir);
        return error;
    }

    Info("executing fragment install");
    for (auto &step : fragment.Install)
        if (auto code = exec(context, package, fragment, step, work_dir))
        {
            remove_work_directory(work_dir);
            return Error("install step '{}' exited with non-zero code {}", step, code);
        }

    auto &installed = config.Installed[cache_key];
    for (auto &[type, path] : fragment.Files)
        switch (type)
        {
        case FileReferenceType::file:
            installed.insert(path);
            break;
        case FileReferenceType::manifest:
        {
            std::vector<std::string> manifest;
            if (auto error = read_manifest(work_dir / fragment.Dir / path, manifest))
                return error;
            installed.insert(manifest.begin(), manifest.end());
            break;
        }
        }

    Info("cleaning up work directory '{}'", work_dir.string());
    remove_work_directory(work_dir);
    return 0;
}
