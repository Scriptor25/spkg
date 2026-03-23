#pragma once

#include <filesystem>

#include <json.hxx>

#include <config.hxx>
#include <specifier.hxx>

namespace spkg
{
    int Help();
    int List(const Config &config);
    int Install(Config &config, Specifier arg, bool use_cache, bool remove);
    int Remove(Config &config, Specifier arg);
    int Update(Config &config, std::optional<Specifier> arg = std::nullopt);

    std::filesystem::path GetHomeDir();
    std::filesystem::path GetDefaultPackagesDir();
    std::filesystem::path GetDefaultCacheDir();
    std::filesystem::path GetConfigDir();
}

template<>
bool from_json(const json::Node &node, std::filesystem::path &value);

template<>
void to_json(json::Node &node, const std::filesystem::path &value);
