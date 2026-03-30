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

    if (!node.IsObject())
        return false;

    auto ok = true;

    ok &= from_json(node["name"], value.Name);
    ok &= from_json_opt(node["array"], value.Array);

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::ForEachDef &value)
{
    if (!node.IsObject())
        return false;

    auto ok = true;

    ok &= from_json(node["var"], value.Var);
    ok &= from_json(node["of"], value.Of);

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

    if (!node.IsObject())
        return false;

    auto ok = true;

    if (std::string line; from_json(node["args"], line))
        parse_args(line, value.Args);
    else
        ok &= from_json(node["args"], value.Args);

    ok &= from_json(node["capture"], value.Capture);
    ok &= from_json(node["output"], value.Output);

    ok &= from_json(node["foreach"], value.ForEach);

    ok &= from_json_opt(node["dir"], value.Dir);
    ok &= from_json_opt(node["env"], value.Env);

    return ok;
}

template<>
bool from_json(const json::Node &node, std::vector<spkg::Command> &value)
{
    if (node.IsUndefined())
        return true;

    if (spkg::Command command; from_json(node, command))
    {
        value.push_back(std::move(command));
        return true;
    }

    if (!node.IsArray())
        return false;

    value.resize(node.size());

    auto ok = true;
    for (std::size_t i = 0; i < value.size(); ++i)
        ok &= from_json(node[i], value[i]);
    return ok;
}

template<>
bool from_json<spkg::Step>(const json::Node &node, spkg::Step &value)
{
    if (!node.IsObject())
        return false;

    auto ok = true;

    ok &= from_json(node["id"], value.Id);

    ok &= from_json_opt(node["dir"], value.Dir);
    ok &= from_json_opt(node["env"], value.Env);

    ok &= from_json_opt(node["cache"], value.Cache);
    ok &= from_json_opt(node["persist"], value.Persist);

    ok &= from_json_opt(node["once"], value.Once);
    ok &= from_json_opt(node["remove"], value.Remove);

    ok &= from_json_opt(node["run"], value.Run);

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::Fragment &value)
{
    if (from_json(node, value.Steps))
        return true;

    if (!node.IsObject())
        return false;

    auto ok = true;

    ok &= from_json_opt(node["name"], value.Name);
    ok &= from_json_opt(node["description"], value.Description);

    ok &= from_json_opt(node["dir"], value.Dir);
    ok &= from_json_opt(node["env"], value.Env);

    ok &= from_json_opt(node["steps"], value.Steps);

    return ok;
}

template<>
bool from_json(const json::Node &node, spkg::Package &value)
{
    if (!node.IsObject())
        return false;

    auto ok = true;

    ok &= from_json(node["id"], value.Id);

    ok &= from_json_opt(node["name"], value.Name);
    ok &= from_json_opt(node["description"], value.Description);

    ok &= from_json_opt(node["params"], value.Params);

    ok &= from_json_opt(node["env"], value.Env);

    ok &= from_json_opt(node["steps"], value.Steps);

    ok &= from_json(node["default"], value.Default);
    ok &= from_json_opt(node["fragments"], value.Fragments);

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
    if (!node.IsObject())
        return false;

    auto ok = true;

    ok &= from_json_opt(node["packages"], value.Packages, { spkg::GetDefaultPackagesDir() });
    ok &= from_json_opt(node["cache"], value.Cache, spkg::GetDefaultCacheDir());
    ok &= from_json_opt(node["installed"], value.Installed);

    return ok;
}

template<>
void to_json(json::Node &node, const spkg::Config &value)
{
    std::map<std::string, json::Node> nodes;

    to_json(nodes["packages"], value.Packages);
    to_json(nodes["cache"], value.Cache);
    to_json(nodes["installed"], value.Installed);

    node = { std::move(nodes) };
}

template<>
bool from_json(const json::Node &node, spkg::PersistEntry &value)
{
    if (spkg::PersistVal val; from_json(node, val))
    {
        value = std::move(val);
        return true;
    }
    if (spkg::PersistVec vec; from_json(node, vec))
    {
        value = std::move(vec);
        return true;
    }
    return false;
}

template<>
void to_json(json::Node &node, const spkg::PersistEntry &value)
{
    std::visit([&node](const auto &value) { return to_json(node, value); }, value);
}
