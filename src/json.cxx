#include <spkg.hxx>

template<>
bool from_json(const json::Node &node, spkg::CommandStep &value)
{
    if (std::string file; node >> file)
    {
        value = { .Command = { std::move(file) } };
        return true;
    }

    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= object_node["command"] >> value.Command;
    ok &= from_json_opt(object_node["capture"], value.Capture);
    ok &= from_json_opt(object_node["dir"], value.Dir);
    ok &= from_json_opt(object_node["env"], value.Env);

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::CommandList &value)
{
    if (json::IsUndefined(node))
    {
        value.clear();
        return true;
    }

    if (spkg::CommandStep s; node >> s)
    {
        value.push_back(std::move(s));
        return true;
    }

    if (!json::IsArray(node))
        return false;

    auto &array_node = json::AsArray(node);
    value.reserve(array_node.size());

    auto ok = true;

    for (auto &element : array_node)
    {
        spkg::CommandStep s;
        auto p = element >> s;
        if (p)
            value.push_back(std::move(s));

        ok &= p;
    }

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::FileReferenceType &value)
{
    static const std::map<std::string, spkg::FileReferenceType> m
    {
        { "file", spkg::FileReferenceType::file },
        { "manifest", spkg::FileReferenceType::manifest },
    };

    if (std::string s; node >> s)
    {
        if (!m.contains(s))
            return false;

        value = m.at(s);
        return true;
    }

    return false;
}

template<>
bool from_json(const json::Node &node, spkg::FileReference &value)
{
    if (std::string path; node >> path)
    {
        value = {
            .Type = spkg::FileReferenceType::file,
            .Path = std::move(path),
        };
        return true;
    }

    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= object_node["type"] >> value.Type;
    ok &= object_node["path"] >> value.Path;

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::Fragment &value)
{
    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= from_json_opt(object_node["name"], value.Name);
    ok &= from_json_opt(object_node["description"], value.Description);
    ok &= from_json_opt(object_node["env"], value.Env);
    ok &= from_json_opt(object_node["dir"], value.Dir);
    ok &= from_json_opt(object_node["cache"], value.Cache);
    ok &= from_json_opt(object_node["configure"], value.Configure);
    ok &= from_json_opt(object_node["build"], value.Build);
    ok &= from_json_opt(object_node["install"], value.Install);
    ok &= from_json_opt(object_node["files"], value.Files);

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::Package &value)
{
    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= object_node["id"] >> value.Id;
    ok &= object_node["version"] >> value.Version;
    ok &= object_node["main"] >> value.Main;

    ok &= from_json_opt(object_node["name"], value.Name);
    ok &= from_json_opt(object_node["description"], value.Description);
    ok &= from_json_opt(object_node["env"], value.Env);
    ok &= from_json_opt(object_node["setup"], value.Setup);
    ok &= from_json_opt(object_node["cache"], value.Cache);
    ok &= from_json_opt(object_node["fragments"], value.Fragments);

    return ok;
}

template<>
bool from_json(const json::Node &node, std::filesystem::path &value)
{
    if (std::string s; node >> s)
    {
        value = std::move(s);
        return true;
    }

    return false;
}

template<>
void to_json(json::Node &node, const std::filesystem::path &value)
{
    node << value.string();
}

template<>
bool from_json(const json::Node &node, spkg::Config &value)
{
    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= from_json_opt(object_node["packages"], value.Packages, { spkg::GetDefaultPackagesDir() });
    ok &= from_json_opt(object_node["cache"], value.Cache, spkg::GetDefaultCacheDir());
    ok &= from_json_opt(object_node["installed"], value.Installed);

    return ok;
}

template<>
void to_json(json::Node &node, const spkg::Config &value)
{
    json::ObjectNode object_node;

    object_node["packages"] << value.Packages;
    object_node["cache"] << value.Cache;
    object_node["installed"] << value.Installed;

    node = std::move(object_node);
}
