#pragma once

#include <specifier.hxx>

#include <json/json.hxx>

#include <filesystem>
#include <format>

namespace spkg
{
    struct Config;

    int Help();
    int List(const Config &config);
    int Install(Config &config, Specifier arg, bool use_cache, bool remove);
    int Remove(Config &config, Specifier arg);
    int Update(Config &config, const std::optional<Specifier> &arg = std::nullopt);

    std::filesystem::path GetHomeDir();
    std::filesystem::path GetDefaultPackagesDir();
    std::filesystem::path GetDefaultCacheDir();
    std::filesystem::path GetConfigDir();
}

template<>
struct data::serializer<std::filesystem::path>
{
    static bool from_data(const json::Node &node, std::filesystem::path &value);
    static void to_data(json::Node &node, const std::filesystem::path &value);
};

template<>
struct std::formatter<std::filesystem::path> : std::formatter<std::string>
{
    template<typename C>
    auto format(const std::filesystem::path &path, C &&ctx) const
    {
        return std::formatter<std::string>::format(std::filesystem::weakly_canonical(path).string(), ctx);
    }
};
