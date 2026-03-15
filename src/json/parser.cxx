#include <istream>
#include <json/parser.hxx>

spkg::json::Parser::Parser(std::istream &stream)
    : m_Stream(stream),
      m_Buffer(stream.get())
{
}

spkg::json::JsonNode spkg::json::Parser::ParseNode()
{
    SkipWhitespace();

    JsonNode node;
    switch (m_Buffer)
    {
    case '[':
        node = ParseArrayNode();
        break;
    case '{':
        node = ParseObjectNode();
        break;
    default:
        node = ParseValueNode();
        break;
    }

    SkipWhitespace();
    return node;
}

spkg::json::JsonNode spkg::json::Parser::ParseValueNode()
{
    if (m_Buffer == '"')
        return ParseStringNode();

    if (m_Buffer == 't' || m_Buffer == 'f')
        return ParseBooleanNode();

    if (m_Buffer == 'n')
        return ParseNullNode();

    if (m_Buffer == '-' || ('0' <= m_Buffer && m_Buffer <= '9'))
        return ParseNumberNode();

    return {};
}

spkg::json::JsonNode spkg::json::Parser::ParseNullNode()
{
    if (!Skip('n'))
        return {};
    if (!Skip('u'))
        return {};
    if (!Skip('l'))
        return {};
    if (!Skip('l'))
        return {};
    return JsonNullNode();
}

spkg::json::JsonNode spkg::json::Parser::ParseBooleanNode()
{
    if (Skip('t'))
    {
        if (!Skip('r'))
            return {};
        if (!Skip('u'))
            return {};
        if (!Skip('e'))
            return {};
        return JsonBooleanNode(true);
    }

    if (!Skip('f'))
        return {};
    if (!Skip('a'))
        return {};
    if (!Skip('l'))
        return {};
    if (!Skip('s'))
        return {};
    if (!Skip('e'))
        return {};
    return JsonBooleanNode(false);
}

spkg::json::JsonNode spkg::json::Parser::ParseNumberNode()
{
    std::string buffer;

    if (Skip('-'))
        buffer += '-';

    if (Skip('0'))
    {
        buffer += '0';
    }
    else if ('1' <= m_Buffer && m_Buffer <= '9')
    {
        do
        {
            buffer += static_cast<char>(m_Buffer);
            Next();
        }
        while ('0' <= m_Buffer && m_Buffer <= '9');
    }
    else
        return {};

    if (Skip('.'))
    {
        buffer += '.';

        if (!('0' <= m_Buffer && m_Buffer <= '9'))
            return {};

        do
        {
            buffer += static_cast<char>(m_Buffer);
            Next();
        }
        while ('0' <= m_Buffer && m_Buffer <= '9');
    }

    if (m_Buffer == 'e' || m_Buffer == 'E')
    {
        buffer += static_cast<char>(m_Buffer);
        Next();

        if (m_Buffer == '-' || m_Buffer == '+')
        {
            buffer += static_cast<char>(m_Buffer);
            Next();
        }

        if (!('0' <= m_Buffer && m_Buffer <= '9'))
            return {};

        do
        {
            buffer += static_cast<char>(m_Buffer);
            Next();
        }
        while ('0' <= m_Buffer && m_Buffer <= '9');
    }

    const auto value = std::stold(buffer, nullptr);

    return JsonNumberNode(value);
}

spkg::json::JsonNode spkg::json::Parser::ParseStringNode()
{
    std::string value;

    if (!Skip('"'))
        return {};

    while (!Skip('"'))
    {
        if (!Skip('\\'))
        {
            value += static_cast<char>(m_Buffer);
            Next();
            continue;
        }

        if (Skip('"'))
        {
            value += '"';
            continue;
        }
        if (Skip('\\'))
        {
            value += '\\';
            continue;
        }
        if (Skip('/'))
        {
            value += '/';
            continue;
        }
        if (Skip('b'))
        {
            value += '\b';
            continue;
        }
        if (Skip('f'))
        {
            value += '\f';
            continue;
        }
        if (Skip('n'))
        {
            value += '\n';
            continue;
        }
        if (Skip('r'))
        {
            value += '\r';
            continue;
        }
        if (Skip('t'))
        {
            value += '\t';
            continue;
        }
        if (Skip('u'))
        {
            char buffer[5];
            buffer[0] = static_cast<char>(m_Buffer);
            Next();
            buffer[1] = static_cast<char>(m_Buffer);
            Next();
            buffer[2] = static_cast<char>(m_Buffer);
            Next();
            buffer[3] = static_cast<char>(m_Buffer);
            Next();
            buffer[4] = 0;
            value += static_cast<char>(std::stoi(buffer, nullptr, 0x10));
            continue;
        }

        return {};
    }

    return JsonStringNode(std::move(value));
}

spkg::json::JsonNode spkg::json::Parser::ParseArrayNode()
{
    std::vector<JsonNode> elements;

    if (!Skip('['))
        return {};

    SkipWhitespace();

    if (!Skip(']'))
    {
        do
        {
            auto element = ParseNode();
            if (IsUndefined(element))
                return {};

            elements.push_back(std::move(element));
        }
        while (Skip(','));

        if (!Skip(']'))
            return {};
    }

    return JsonArrayNode(std::move(elements));
}

spkg::json::JsonNode spkg::json::Parser::ParseObjectNode()
{
    std::map<std::string, JsonNode> elements;

    if (!Skip('{'))
        return {};

    SkipWhitespace();

    if (!Skip('}'))
    {
        do
        {
            SkipWhitespace();

            const auto key = ParseStringNode();
            if (IsUndefined(key))
                return {};

            SkipWhitespace();

            if (!Skip(':'))
                return {};

            auto value = ParseNode();
            if (IsUndefined(value))
                return {};

            elements[AsString(key)] = std::move(value);
        }
        while (Skip(','));

        if (!Skip('}'))
            return {};
    }

    return JsonObjectNode(std::move(elements));
}

void spkg::json::Parser::Next()
{
    m_Buffer = m_Stream.get();
}

bool spkg::json::Parser::Skip(const int c)
{
    if (m_Buffer != c)
        return false;
    Next();
    return true;
}

static bool is_whitespace(const int c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

bool spkg::json::Parser::SkipWhitespace()
{
    if (!is_whitespace(m_Buffer))
        return false;

    do
        Next();
    while (is_whitespace(m_Buffer));

    return true;
}
