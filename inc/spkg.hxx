#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <json.hxx>

namespace spkg
{
    enum class FileReferenceType
    {
        file,
        manifest,
    };

    struct FileReference
    {
        FileReferenceType Type{};
        std::string Path;
    };

    struct CommandStep
    {
        std::vector<std::string> Command;
        std::string Capture;

        std::string Dir;
        std::map<std::string, std::string> Env;
    };

    using CommandList = std::vector<CommandStep>;

    struct Fragment
    {
        std::string Name;
        std::string Description;

        std::string Dir;
        std::map<std::string, std::string> Env;

        CommandList Configure;
        CommandList Build;
        CommandList Install;

        std::vector<std::string> Cache;
        std::vector<FileReference> Files;
    };

    struct Package
    {
        std::string Id;
        std::string Version;

        std::string Name;
        std::string Description;

        std::map<std::string, std::string> Env;

        CommandList Setup;
        std::vector<std::string> Cache;

        Fragment Main;
        std::map<std::string, Fragment> Fragments;
    };

    struct Context
    {
        std::vector<std::map<std::string, std::string>> Stack;
    };

    struct Config
    {
        std::set<std::filesystem::path> Packages;
        std::filesystem::path Cache;

        std::map<std::string, std::set<std::string>> Installed;
    };

    struct Specifier
    {
        Specifier() = default;

        explicit Specifier(std::string_view s);

        std::string_view Id;
        std::string_view Fragment;
        std::string_view Version;
    };

    std::ostream &operator<<(std::ostream &stream, const CommandStep &step);

    template<typename... A>
    auto Info(std::format_string<A...> &&format, A &&... args)
    {
        std::cerr << "Info: " << std::format<A...>(
            std::forward<std::format_string<A...>>(format),
            std::forward<A>(args)...) << std::endl;
    }

    template<typename... A>
    auto Warning(std::format_string<A...> &&format, A &&... args)
    {
        std::cerr << "Warning: " << std::format<A...>(
            std::forward<std::format_string<A...>>(format),
            std::forward<A>(args)...) << std::endl;
    }

    template<typename... A>
    auto Error(std::format_string<A...> &&format, A &&... args)
    {
        std::cerr << "Error: " << std::format<A...>(
            std::forward<std::format_string<A...>>(format),
            std::forward<A>(args)...) << std::endl;
        return 1;
    }

    int Help();
    int List(const Config &config);
    int Install(Config &config, std::string_view arg);
    int Remove(Config &config, std::string_view arg);
    int Update(Config &config, std::string_view arg = {});

    std::filesystem::path GetHomeDir();
    std::filesystem::path GetDefaultPackagesDir();
    std::filesystem::path GetDefaultCacheDir();
    std::filesystem::path GetConfigDir();
}

template<>
struct std::formatter<spkg::CommandStep> : std::formatter<std::string>
{
    template<typename C>
    auto format(const spkg::CommandStep &step, C &context) const
    {
        std::string str;
        if (!step.Dir.empty())
            str += "[" + step.Dir + "] ";
        for (auto &[key, val] : step.Env)
            str += key + "=" + val + " ";
        for (auto it = step.Command.begin(); it != step.Command.end(); ++it)
        {
            if (it != step.Command.begin())
                str += ' ';
            str += "'" + *it + "'";
        }

        return std::formatter<std::string>::format(str, context);
    }
};

template<>
bool from_json(const json::Node &node, std::filesystem::path &value);

template<>
void to_json(json::Node &node, const std::filesystem::path &value);

template<>
bool from_json(const json::Node &node, spkg::CommandStep &value);

template<>
bool from_json(const json::Node &node, spkg::CommandList &value);

template<>
bool from_json(const json::Node &node, spkg::FileReferenceType &value);

template<>
bool from_json(const json::Node &node, spkg::FileReference &value);

template<>
bool from_json(const json::Node &node, spkg::Fragment &value);

template<>
bool from_json(const json::Node &node, spkg::Package &value);

template<>
bool from_json(const json::Node &node, spkg::Config &value);

template<>
void to_json(json::Node &node, const spkg::Config &value);
