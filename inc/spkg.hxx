#pragma once

#include <specifier.hxx>

#include <json/json.hxx>

#include <filesystem>
#include <format>
#include <span>
#include <toolkit/result.hxx>

namespace spkg
{
    struct Config;

    void Help();
    void List(const Config &config);
    toolkit::result<> Install(
        Config &config,
        Specifier spec,
        std::span<const std::string_view> line,
        bool use_cache,
        bool remove);
    toolkit::result<> Remove(Config &config, Specifier spec);
    toolkit::result<> Update(Config &config, const std::optional<Specifier> &spec = std::nullopt);

    std::filesystem::path GetHomeDir();
    std::filesystem::path GetDefaultPackagesDir();
    std::filesystem::path GetDefaultCacheDir();
    std::filesystem::path GetConfigDir();
}

template<>
struct data::serializer<std::filesystem::path>
{
    static bool from_data(const json::node &node, std::filesystem::path &value);
    static void to_data(json::node &node, const std::filesystem::path &value);
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
