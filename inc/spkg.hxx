#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <json.hxx>

namespace spkg
{
    struct Package;

    struct Context
    {
        const Package &Pkg;

        std::filesystem::path WorkDir;

        std::vector<std::map<std::string, std::string>> Stack;
    };

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

    struct Command
    {
        std::vector<std::string> Args;

        std::string Capture;
        std::string Output;

        std::string Dir;
        std::map<std::string, std::string> Env;
    };

    struct Step
    {
        std::string Id;

        std::string Dir;
        std::map<std::string, std::string> Env;

        std::vector<std::string> Cache;
        bool Once;

        std::vector<Command> Run;
    };

    struct Fragment
    {
        std::string Name;
        std::string Description;

        std::string Dir;
        std::map<std::string, std::string> Env;

        std::vector<Step> Steps;

        std::vector<FileReference> Files;
    };

    struct Package
    {
        std::string Id;
        std::string Version;

        std::string Name;
        std::string Description;

        std::map<std::string, std::string> Env;

        std::vector<Step> Setup;

        Fragment Default;
        std::map<std::string, Fragment> Fragments;
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
        explicit Specifier(const std::string &s);

        std::string Id;
        std::string Fragment;
        std::string Version;
    };

    std::ostream &operator<<(std::ostream &stream, const Command &command);

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
    int Install(Config &config, const std::string &arg);
    int Remove(Config &config, const std::string &arg);
    int Update(Config &config, const std::string &arg = {});

    std::filesystem::path GetHomeDir();
    std::filesystem::path GetDefaultPackagesDir();
    std::filesystem::path GetDefaultCacheDir();
    std::filesystem::path GetConfigDir();

    bool FindPackage(const Config &config, const Specifier &specifier, Package &package);
    bool ForEachPackage(const Config &config, const std::function<bool(Package &&)> &fn);
}

template<>
struct std::formatter<spkg::Command> : std::formatter<std::string>
{
    template<typename C>
    auto format(const spkg::Command &command, C &context) const
    {
        std::string str;
        for (auto it = command.Args.begin(); it != command.Args.end(); ++it)
        {
            if (it != command.Args.begin())
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
bool from_json(const json::Node &node, spkg::Command &value);

template<>
bool from_json(const json::Node &node, std::vector<spkg::Command> &value);

template<>
bool from_json(const json::Node &node, spkg::FileReferenceType &value);

template<>
bool from_json(const json::Node &node, spkg::FileReference &value);

template<>
bool from_json(const json::Node &node, spkg::Step &value);

template<>
bool from_json(const json::Node &node, spkg::Fragment &value);

template<>
bool from_json(const json::Node &node, spkg::Package &value);

template<>
bool from_json(const json::Node &node, spkg::Config &value);

template<>
void to_json(json::Node &node, const spkg::Config &value);
