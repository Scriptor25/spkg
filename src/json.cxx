#include <config.hxx>
#include <package.hxx>
#include <spkg.hxx>

void spkg::ParseArgs(const std::string &line, std::vector<std::string> &args)
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
                    args.push_back(std::move(buffer));
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
        args.push_back(std::move(buffer));
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

bool data::serializer<spkg::Config>::from_data(const json::Node &node, spkg::Config &value)
{
    if (!node.Is<json::Object>())
        return false;

    auto ok = true;

    ok &= from_data_opt(node["cache"], value.Cache, spkg::GetDefaultCacheDir());
    ok &= from_data_opt(node["packages"], value.Packages, { spkg::GetDefaultPackagesDir() });
    ok &= from_data_opt(node["installed"], value.Installed);

    return ok;
}

void data::serializer<spkg::Config>::to_data(json::Node &node, const spkg::Config &value)
{
    node = json::Object
    {
        { "cache", value.Cache },
        { "packages", value.Packages },
        { "installed", value.Installed },
    };
}
