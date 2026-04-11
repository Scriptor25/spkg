#pragma once

#include <config.hxx>
#include <specifier.hxx>

#include <data/serializer.hxx>

#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace spkg
{
    struct CaptureDef
    {
        std::string Name;
        bool Array{};
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
        std::set<std::string> Persist;

        bool Once{};
        bool Remove{};

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

    void ParseArgs(const std::string &line, std::vector<std::string> &args);

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
struct data::serializer<spkg::CaptureDef>
{
    template<node N>
    static bool from_data(const N &node, spkg::CaptureDef &value)
    {
        using Map = N::Map;

        if (node >> value.Name)
            return true;

        if (!node.template Is<Map>())
            return false;

        auto ok = true;

        ok &= node["name"] >> value.Name;
        ok &= from_data_opt(node["array"], value.Array);

        return ok;
    }
};

template<>
struct data::serializer<spkg::ForEachDef>
{
    template<node N>
    static bool from_data(const N &node, spkg::ForEachDef &value)
    {
        using Map = N::Map;

        if (!node.template Is<Map>())
            return false;

        auto ok = true;

        ok &= node["var"] >> value.Var;
        ok &= node["of"] >> value.Of;

        return ok;
    }
};

template<>
struct data::serializer<spkg::Command>
{
    template<node N>
    static bool from_data(const N &node, spkg::Command &value)
    {
        using Map = N::Map;

        if (std::string line; node >> line)
        {
            spkg::ParseArgs(line, value.Args);
            return true;
        }

        if (!node.template Is<Map>())
            return false;

        auto ok = true;

        if (std::string line; node["args"] >> line)
            spkg::ParseArgs(line, value.Args);
        else
            ok &= node["args"] >> value.Args;

        ok &= node["capture"] >> value.Capture;
        ok &= node["output"] >> value.Output;

        ok &= node["foreach"] >> value.ForEach;

        ok &= from_data_opt(node["dir"], value.Dir);
        ok &= from_data_opt(node["env"], value.Env);

        return ok;
    }
};

template<>
struct data::serializer<std::vector<spkg::Command>>
{
    template<node N>
    static bool from_data(const N &node, std::vector<spkg::Command> &value)
    {
        using Vec = N::Vec;

        if (!node)
            return true;

        if (spkg::Command command; node >> command)
        {
            value.push_back(std::move(command));
            return true;
        }

        if (!node.template Is<Vec>())
            return false;

        value.resize(node.size());

        auto ok = true;
        for (std::size_t i = 0; i < value.size(); ++i)
            ok &= node[i] >> value[i];
        return ok;
    }
};

template<>
struct data::serializer<spkg::Step>
{
    template<node N>
    static bool from_data(const N &node, spkg::Step &value)
    {
        using Map = N::Map;

        if (!node.template Is<Map>())
            return false;

        auto ok = true;

        ok &= node["id"] >> value.Id;

        ok &= from_data_opt(node["dir"], value.Dir);
        ok &= from_data_opt(node["env"], value.Env);

        ok &= from_data_opt(node["cache"], value.Cache);
        ok &= from_data_opt(node["persist"], value.Persist);

        ok &= from_data_opt(node["once"], value.Once);
        ok &= from_data_opt(node["remove"], value.Remove);

        ok &= from_data_opt(node["run"], value.Run);

        return ok;
    }
};

template<>
struct data::serializer<spkg::Fragment>
{
    template<node N>
    static bool from_data(const N &node, spkg::Fragment &value)
    {
        using Map = N::Map;

        if (node >> value.Steps)
            return true;

        if (!node.template Is<Map>())
            return false;

        auto ok = true;

        ok &= from_data_opt(node["name"], value.Name);
        ok &= from_data_opt(node["description"], value.Description);

        ok &= from_data_opt(node["dir"], value.Dir);
        ok &= from_data_opt(node["env"], value.Env);

        ok &= from_data_opt(node["steps"], value.Steps);

        return ok;
    }
};

template<>
struct data::serializer<spkg::Package>
{
    template<node N>
    static bool from_data(const N &node, spkg::Package &value)
    {
        using Map = N::Map;

        if (!node.template Is<Map>())
            return false;

        auto ok = true;

        ok &= node["id"] >> value.Id;

        ok &= from_data_opt(node["name"], value.Name);
        ok &= from_data_opt(node["description"], value.Description);

        ok &= from_data_opt(node["params"], value.Params);

        ok &= from_data_opt(node["env"], value.Env);

        ok &= from_data_opt(node["steps"], value.Steps);

        ok &= node["default"] >> value.Default;
        ok &= from_data_opt(node["fragments"], value.Fragments);

        return ok;
    }
};
