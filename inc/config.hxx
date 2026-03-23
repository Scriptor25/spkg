#pragma once

#include <filesystem>
#include <map>
#include <set>

#include <json.hxx>

namespace spkg
{
    struct Config
    {
        std::set<std::filesystem::path> Packages;
        std::filesystem::path Cache;

        std::map<std::string, std::map<std::string, std::string>> Installed;
    };
}

template<>
bool from_json(const json::Node &node, spkg::Config &value);

template<>
void to_json(json::Node &node, const spkg::Config &value);
