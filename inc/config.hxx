#pragma once

#include <persist.hxx>

#include <json/json.hxx>

#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace spkg
{
    struct Config
    {
        std::filesystem::path Cache;
        std::unordered_set<std::filesystem::path> Packages;
        std::unordered_map<std::string, PersistMap> Installed;

        bool CacheUpdated{};
        std::unordered_set<std::filesystem::path> PackagesAdded;
        std::unordered_set<std::filesystem::path> PackagesRemoved;
        std::unordered_set<std::string> InstalledAdded;
        std::unordered_set<std::string> InstalledRemoved;
    };
}

template<>
struct data::serializer<spkg::Config>
{
    static bool from_data(const json::node &node, spkg::Config &value);
    static void to_data(json::node &node, const spkg::Config &value);
};
