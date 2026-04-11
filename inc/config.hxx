#pragma once

#include <persist.hxx>

#include <json/json.hxx>

#include <filesystem>
#include <map>
#include <set>

namespace spkg
{
    struct Config
    {
        std::set<std::filesystem::path> Packages;
        std::filesystem::path Cache;

        std::map<std::string, PersistMap> Installed;
    };
}

template<>
bool from_data(const json::Node &node, spkg::Config &value);

template<>
void to_data(json::Node &node, const spkg::Config &value);
