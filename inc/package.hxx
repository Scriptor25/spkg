#pragma once

#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <json.hxx>

#include <config.hxx>
#include <specifier.hxx>

namespace spkg
{
    struct CaptureDef
    {
        std::string Name;
        bool Array;
    };

    struct ForEachDef
    {
        std::string Var;
        std::string Of;
    };

    struct Command
    {
        std::vector<std::string> Args;

        std::optional<CaptureDef> Capture;
        std::optional<std::string> Output;

        std::optional<ForEachDef> ForEach;

        std::string Dir;
        std::map<std::string, std::string> Env;
    };

    struct Step
    {
        std::string Id;

        std::string Dir;
        std::map<std::string, std::string> Env;

        std::vector<std::string> Cache;
        std::vector<std::string> Persist;

        bool Once;
        bool Remove;

        std::vector<Command> Run;
    };

    struct Fragment
    {
        std::string Name;
        std::string Description;

        std::string Dir;
        std::map<std::string, std::string> Env;

        std::vector<Step> Steps;
    };
    
    struct Package
    {
        bool GetFragment(std::string id, Fragment &fragment) const;

        std::string Id;

        std::string Name;
        std::string Description;

        std::set<std::string> Params;

        std::map<std::string, std::string> Env;

        std::vector<Step> Steps;

        Fragment Default;
        std::map<std::string, Fragment> Fragments;
    };

    bool FindPackage(const Config &config, const Specifier &specifier, Package &package);
    bool ForEachPackage(const Config &config, const std::function<bool(Package &&)> &fn);

    std::ostream &operator<<(std::ostream &stream, const Command &command);
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
            str += '\'' + *it + '\'';
        }
        return std::formatter<std::string>::format(str, context);
    }
};

template<>
bool from_json(const json::Node &node, spkg::CaptureDef &value);

template<>
bool from_json(const json::Node &node, spkg::ForEachDef &value);

template<>
bool from_json(const json::Node &node, spkg::Command &value);

template<>
bool from_json(const json::Node &node, std::vector<spkg::Command> &value);

template<>
bool from_json(const json::Node &node, spkg::Step &value);

template<>
bool from_json(const json::Node &node, spkg::Fragment &value);

template<>
bool from_json(const json::Node &node, spkg::Package &value);
