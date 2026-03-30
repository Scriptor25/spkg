#pragma once

#include <package.hxx>
#include <persist.hxx>

#include <json/json.hxx>

#include <filesystem>
#include <string>

namespace spkg
{
    struct Context
    {
        bool GetVariable(const std::string &key, PersistEntry &value) const;
        bool GetVariable(const std::string &key, PersistVal &value) const;

        bool UseCache;
        bool Remove;

        const Package &Pkg;
        std::filesystem::path WorkDir;

        PersistMap Persist;

        std::vector<PersistMap> Stack;
    };
}
