#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <package.hxx>

namespace spkg
{
    struct Context
    {
        bool GetVariable(const std::string &key, std::string &value) const;

        bool UseCache;
        bool Remove;

        const Package &Pkg;
        std::filesystem::path WorkDir;

        std::map<std::string, std::string> Persist;

        std::vector<std::map<std::string, std::string>> Stack;
    };
}
