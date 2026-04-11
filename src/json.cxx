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
bool from_data(const json::Node &node, spkg::CaptureDef &value)
{
    if (node >> value.Name)
        return true;

    if (!node.Is<json::Node::Map>())
        return false;

    auto ok = true;

    ok &= node["name"] >> value.Name;
    ok &= from_data_opt(node["array"], value.Array);

    return ok;
}

template<>
bool from_data(const json::Node &node, spkg::ForEachDef &value)
{
    if (!node.Is<json::Node::Map>())
        return false;

    auto ok = true;

    ok &= node["var"] >> value.Var;
    ok &= node["of"] >> value.Of;

    return ok;
}

template<>
bool from_data(const json::Node &node, spkg::Command &value)
{
    if (std::string line; node >> line)
    {
        parse_args(line, value.Args);
        return true;
    }

    if (!node.Is<json::Node::Map>())
        return false;

    auto ok = true;

    if (std::string line; node["args"] >> line)
        parse_args(line, value.Args);
    else
        ok &= node["args"] >> value.Args;

    ok &= node["capture"] >> value.Capture;
    ok &= node["output"] >> value.Output;

    ok &= node["foreach"] >> value.ForEach;

    ok &= from_data_opt(node["dir"], value.Dir);
    ok &= from_data_opt(node["env"], value.Env);

    return ok;
}

template<>
bool from_data(const json::Node &node, std::vector<spkg::Command> &value)
{
    if (!node)
        return true;

    if (spkg::Command command; node >> command)
    {
        value.push_back(std::move(command));
        return true;
    }

    if (!node.Is<json::Node::Vec>())
        return false;

    value.resize(node.size());

    auto ok = true;
    for (std::size_t i = 0; i < value.size(); ++i)
        ok &= node[i] >> value[i];
    return ok;
}

template<>
bool from_data(const json::Node &node, spkg::Step &value)
{
    if (!node.Is<json::Node::Map>())
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

template<>
bool from_data(const json::Node &node, spkg::Fragment &value)
{
    if (node >> value.Steps)
        return true;

    if (!node.Is<json::Node::Map>())
        return false;

    auto ok = true;

    ok &= from_data_opt(node["name"], value.Name);
    ok &= from_data_opt(node["description"], value.Description);

    ok &= from_data_opt(node["dir"], value.Dir);
    ok &= from_data_opt(node["env"], value.Env);

    ok &= from_data_opt(node["steps"], value.Steps);

    return ok;
}

template<>
bool from_data(const json::Node &node, spkg::Package &value)
{
    if (!node.Is<json::Node::Map>())
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

bool data::serializer<std::filesystem::path>::from_data(const json::Node &node, std::filesystem::path &value)
{
    if (std::string val; node >> val)
    {
        value = std::move(val);
        return true;
    }

    return false;
}

void data::serializer<std::filesystem::path>::to_data(json::Node &node, const std::filesystem::path &value)
{
    node = value.string();
}

template<>
bool from_data(const json::Node &node, spkg::Config &value)
{
    if (!node.Is<json::Node::Map>())
        return false;

    auto ok = true;

    ok &= from_data_opt(node["packages"], value.Packages, { spkg::GetDefaultPackagesDir() });
    ok &= from_data_opt(node["cache"], value.Cache, spkg::GetDefaultCacheDir());
    ok &= from_data_opt(node["installed"], value.Installed);

    return ok;
}

template<>
void to_data(json::Node &node, const spkg::Config &value)
{
    node = json::Node::Map
    {
        { "packages", value.Packages },
        { "cache", value.Cache },
        { "installed", value.Installed },
    };
}
