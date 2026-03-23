#include <package.hxx>
#include <spkg.hxx>

static void parse_args(const std::string &line, std::vector<std::string> &command)
{
    std::string buffer;

    enum
    {
        STATE_NORMAL,
        STATE_SINGLE_QUOTE,
        STATE_DOUBLE_QUOTE,
        STATE_EXPANSION,
    } state = STATE_NORMAL;

    for (std::size_t i = 0; i < line.size(); ++i)
    {
        const auto c = line[i];

        switch (state)
        {
        case STATE_NORMAL:
            switch (c)
            {
            case ' ':
            case '\t':
                if (!buffer.empty())
                    command.push_back(std::move(buffer));
                break;
            case '\'':
                state = STATE_SINGLE_QUOTE;
                break;
            case '"':
                state = STATE_DOUBLE_QUOTE;
                break;
            case '$':
                if (line[i + 1] == '(')
                {
                    buffer += c;
                    buffer += line[++i];
                    state = STATE_EXPANSION;
                }
                break;
            case '\\':
                if (i + 1 < line.size())
                    buffer += line[++i];
                break;
            default:
                buffer += c;
                break;
            }
            break;

        case STATE_SINGLE_QUOTE:
            if (c == '\'')
            {
                state = STATE_NORMAL;
                break;
            }
            buffer += c;
            break;

        case STATE_DOUBLE_QUOTE:
            if (c == '"')
            {
                state = STATE_NORMAL;
                break;
            }
            if (c == '\\')
            {
                if (i + 1 >= line.size())
                    break;

                if (const auto n = line[i + 1]; n == '"' || n == '\\')
                {
                    buffer += line[++i];
                    break;
                }
            }
            buffer += c;
            break;

        case STATE_EXPANSION:
            buffer += c;
            if (c == ')')
            {
                state = STATE_NORMAL;
                break;
            }
            break;
        }
    }

    if (!buffer.empty())
        command.push_back(std::move(buffer));
}

template<>
bool from_json(const json::Node &node, spkg::CaptureDef &value)
{
    if (from_json(node, value.Name))
        return true;

    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= from_json(object_node["name"], value.Name);
    ok &= from_json_opt(object_node["array"], value.Array);

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::ForEachDef &value)
{
    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= from_json(object_node["var"], value.Var);
    ok &= from_json(object_node["of"], value.Of);

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::Command &value)
{
    if (std::string line; node >> line)
    {
        parse_args(line, value.Args);
        return true;
    }

    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    if (std::string line; from_json(object_node["args"], line))
        parse_args(line, value.Args);
    else
        ok &= from_json(object_node["args"], value.Args);

    ok &= from_json(object_node["capture"], value.Capture);
    ok &= from_json(object_node["output"], value.Output);

    ok &= from_json(object_node["foreach"], value.ForEach);

    ok &= from_json_opt(object_node["dir"], value.Dir);
    ok &= from_json_opt(object_node["env"], value.Env);

    return ok;
}

template<>
bool from_json(const json::Node &node, std::vector<spkg::Command> &value)
{
    if (json::IsUndefined(node))
        return true;

    if (spkg::Command command; from_json(node, command))
    {
        value.push_back(std::move(command));
        return true;
    }

    if (!json::IsArray(node))
        return false;

    auto &array_node = json::AsArray(node);
    value.resize(array_node.size());

    auto ok = true;
    for (std::size_t i = 0; i < value.size(); ++i)
        ok &= from_json(array_node[i], value[i]);
    return ok;
}

template<>
bool from_json<spkg::Step>(const json::Node &node, spkg::Step &value)
{
    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= from_json(object_node["id"], value.Id);

    ok &= from_json_opt(object_node["dir"], value.Dir);
    ok &= from_json_opt(object_node["env"], value.Env);

    ok &= from_json_opt(object_node["cache"], value.Cache);
    ok &= from_json_opt(object_node["persist"], value.Persist);

    ok &= from_json_opt(object_node["once"], value.Once);
    ok &= from_json_opt(object_node["remove"], value.Remove);

    ok &= from_json_opt(object_node["run"], value.Run);

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::Fragment &value)
{
    if (from_json(node, value.Steps))
        return true;

    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= from_json_opt(object_node["name"], value.Name);
    ok &= from_json_opt(object_node["description"], value.Description);

    ok &= from_json_opt(object_node["dir"], value.Dir);
    ok &= from_json_opt(object_node["env"], value.Env);

    ok &= from_json_opt(object_node["steps"], value.Steps);

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::Package &value)
{
    if (!json::IsObject(node))
        return false;

    auto &object_node = json::AsObject(node);

    auto ok = true;

    ok &= from_json(object_node["id"], value.Id);

    ok &= from_json_opt(object_node["name"], value.Name);
    ok &= from_json_opt(object_node["description"], value.Description);

    ok &= from_json_opt(object_node["params"], value.Params);

    ok &= from_json_opt(object_node["env"], value.Env);

    ok &= from_json_opt(object_node["steps"], value.Steps);

    ok &= from_json(object_node["default"], value.Default);
    ok &= from_json_opt(object_node["fragments"], value.Fragments);

    return ok;
}

template<>
bool from_json(const json::Node &node, std::filesystem::path &value)
{
    if (std::string s; from_json(node, s))
    {
        value = std::move(s);
        return true;
    }

    return false;
}

template<>
void to_json(json::Node &node, const std::filesystem::path &value)
{
    to_json(node, value.string());
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

    to_json(object_node["packages"], value.Packages);
    to_json(object_node["cache"], value.Cache);
    to_json(object_node["installed"], value.Installed);

    node = std::move(object_node);
}
