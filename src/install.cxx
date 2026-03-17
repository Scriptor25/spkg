#include <unistd.h>
#include <sys/wait.h>

#include <ranges>
#include <spkg.hxx>

static int copy_cache(
    const std::filesystem::path &src_path,
    const std::filesystem::path &dst_path,
    const std::vector<std::string> &cache)
{
    for (auto &entry : cache)
    {
        auto from = src_path / entry;
        auto to = dst_path / entry;

        std::error_code ec;
        std::filesystem::copy(
            from,
            to,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::update_existing,
            ec);

        if (ec)
            return spkg::Error(
                "failed to copy files from '{}' to '{}': {} ({})",
                from.string(),
                to.string(),
                ec.message(),
                ec.value());
    }

    return 0;
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
    std::string *out,
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
    std::ostringstream stream;
    for (ssize_t n; (n = read(pipes[0], buffer, sizeof(buffer))) > 0;)
        if (out)
            stream.write(buffer, n);
        else
            write(STDOUT_FILENO, buffer, n);
    close(pipes[0]);

    auto status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;

    if (out)
        *out = rtrim(stream.str());

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

    const auto out = step.Capture.empty() ? nullptr : &context.Stack.back()[step.Capture];

    return exec(out, work_dir / step.Dir, command[0], command, envs);
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

    auto cache_key = package.Id
                     + (specifier.Fragment.empty() ? "" : '-' + std::string(specifier.Fragment))
                     + '-' + package.Version;
    auto work_dir = config.Cache / cache_key;

    auto cache_dir = config.Cache / package.Id / package.Version;
    auto package_cache_dir = cache_dir / "__package__";
    auto fragment_cache_dir = cache_dir / (specifier.Fragment.empty() ? "__fragment__" : specifier.Fragment);

    std::filesystem::create_directories(work_dir);
    if (!std::filesystem::exists(work_dir))
        return Error("failed to create work directory '{}'", work_dir.string());

    auto &package_frame = context.Stack.emplace_back();

    if (std::filesystem::exists(package_cache_dir))
    {
        if (auto error = copy_cache(package_cache_dir, work_dir, package.Cache))
            return error;

        if (auto error = read_map_manifest(package_cache_dir / "manifest.txt", package_frame))
            return error;
    }
    else
    {
        for (auto &step : package.Setup)
            if (auto code = exec(context, package, step, work_dir))
            {
                std::filesystem::remove_all(work_dir);
                return Error("setup step '{}' exited with non-zero code {}", step, code);
            }

        std::filesystem::create_directories(package_cache_dir);
        if (!std::filesystem::exists(package_cache_dir))
            return Error("failed to create cache directory '{}'", package_cache_dir.string());

        if (auto error = copy_cache(work_dir, package_cache_dir, package.Cache))
            return error;

        if (auto error = write_map_manifest(package_cache_dir / "manifest.txt", package_frame))
            return error;
    }

    auto &fragment_frame = context.Stack.emplace_back();

    if (std::filesystem::exists(fragment_cache_dir))
    {
        if (auto error = copy_cache(fragment_cache_dir, work_dir, fragment.Cache))
            return error;

        if (auto error = read_map_manifest(fragment_cache_dir / "manifest.txt", fragment_frame))
            return error;
    }

    for (auto &step : fragment.Configure)
        if (auto code = exec(context, package, fragment, step, work_dir))
        {
            std::filesystem::remove_all(work_dir);
            return Error("configure step '{}' exited with non-zero code {}", step, code);
        }

    for (auto &step : fragment.Build)
        if (auto code = exec(context, package, fragment, step, work_dir))
        {
            std::filesystem::remove_all(work_dir);
            return Error("build step '{}' exited with non-zero code {}", step, code);
        }

    std::filesystem::create_directories(fragment_cache_dir);
    if (!std::filesystem::exists(fragment_cache_dir))
        return Error("failed to create cache directory '{}'", fragment_cache_dir.string());

    if (auto error = copy_cache(work_dir, fragment_cache_dir, fragment.Cache))
        return error;

    if (auto error = write_map_manifest(fragment_cache_dir / "manifest.txt", fragment_frame))
        return error;

    for (auto &step : fragment.Install)
        if (auto code = exec(context, package, fragment, step, work_dir))
        {
            std::filesystem::remove_all(work_dir);
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

    std::filesystem::remove_all(work_dir);
    return 0;
}
