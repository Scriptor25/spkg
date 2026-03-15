#include <iomanip>
#include <utility>

#include <json.hxx>
#include <json/parser.hxx>

std::ostream &spkg::json::JsonNullNode::Print(std::ostream &stream) const
{
    return stream << "null";
}

spkg::json::JsonBooleanNode::JsonBooleanNode(const bool value)
    : Value(value)
{
}

std::ostream &spkg::json::JsonBooleanNode::Print(std::ostream &stream) const
{
    return stream << (Value ? "true" : "false");
}

spkg::json::JsonBooleanNode::operator bool() const
{
    return Value;
}

spkg::json::JsonNumberNode::JsonNumberNode(const long double value)
    : Value(value)
{
}

std::ostream &spkg::json::JsonNumberNode::Print(std::ostream &stream) const
{
    return stream << std::scientific << Value;
}

spkg::json::JsonNumberNode::operator long double() const
{
    return Value;
}

spkg::json::JsonStringNode::JsonStringNode(std::string value)
    : Value(std::move(value))
{
}

std::ostream &spkg::json::JsonStringNode::Print(std::ostream &stream) const
{
    stream << '"';
    for (const auto c : Value)
    {
        switch (c)
        {
        case '"':
            stream << "\\\"";
            break;
        case '\\':
            stream << "\\\\";
            break;
        case '\b':
            stream << "\\b";
            break;
        case '\f':
            stream << "\\f";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            if (0x20 <= c && c < 0x7F)
                stream << c;
            else
                stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
            break;
        }
    }
    return stream << '"';
}

std::string::iterator spkg::json::JsonStringNode::begin()
{
    return Value.begin();
}

std::string::iterator spkg::json::JsonStringNode::end()
{
    return Value.end();
}

std::string::const_iterator spkg::json::JsonStringNode::begin() const
{
    return Value.begin();
}

std::string::const_iterator spkg::json::JsonStringNode::end() const
{
    return Value.end();
}

spkg::json::JsonStringNode::operator std::string() const
{
    return Value;
}

spkg::json::JsonArrayNode::JsonArrayNode(const std::size_t size)
    : Elements(size)
{
}

spkg::json::JsonArrayNode::JsonArrayNode(std::vector<JsonNode> elements)
    : Elements(std::move(elements))
{
}

std::ostream &spkg::json::JsonArrayNode::Print(std::ostream &stream) const
{
    stream << '[';
    for (auto it = Elements.begin(); it != Elements.end(); ++it)
    {
        if (it != Elements.begin())
            stream << ',';
        stream << *it;
    }
    return stream << ']';
}

std::vector<spkg::json::JsonNode>::iterator spkg::json::JsonArrayNode::begin()
{
    return Elements.begin();
}

std::vector<spkg::json::JsonNode>::iterator spkg::json::JsonArrayNode::end()
{
    return Elements.end();
}

std::vector<spkg::json::JsonNode>::const_iterator spkg::json::JsonArrayNode::begin() const
{
    return Elements.begin();
}

std::vector<spkg::json::JsonNode>::const_iterator spkg::json::JsonArrayNode::end() const
{
    return Elements.end();
}

spkg::json::JsonNode &spkg::json::JsonArrayNode::operator[](const std::size_t index)
{
    return Elements[index];
}

const spkg::json::JsonNode &spkg::json::JsonArrayNode::operator[](const std::size_t index) const
{
    return Elements[index];
}

std::size_t spkg::json::JsonArrayNode::size() const
{
    return Elements.size();
}

spkg::json::JsonObjectNode::JsonObjectNode(std::map<std::string, JsonNode> elements)
    : Elements(std::move(elements))
{
}

std::ostream &spkg::json::JsonObjectNode::Print(std::ostream &stream) const
{
    stream << '{';
    for (auto it = Elements.begin(); it != Elements.end(); ++it)
    {
        if (it != Elements.begin())
            stream << ',';
        JsonStringNode(it->first).Print(stream) << ':' << it->second;
    }
    return stream << '}';
}

std::map<std::string, spkg::json::JsonNode>::iterator spkg::json::JsonObjectNode::begin()
{
    return Elements.begin();
}

std::map<std::string, spkg::json::JsonNode>::iterator spkg::json::JsonObjectNode::end()
{
    return Elements.end();
}

std::map<std::string, spkg::json::JsonNode>::const_iterator spkg::json::JsonObjectNode::begin() const
{
    return Elements.begin();
}

std::map<std::string, spkg::json::JsonNode>::const_iterator spkg::json::JsonObjectNode::end() const
{
    return Elements.end();
}

spkg::json::JsonNode &spkg::json::JsonObjectNode::operator[](const std::string &key)
{
    return Elements[{ key.begin(), key.end() }];
}

spkg::json::JsonNode spkg::json::JsonObjectNode::operator[](const std::string &key) const
{
    if (Elements.contains({ key.begin(), key.end() }))
        return Elements.at({ key.begin(), key.end() });
    return {};
}

spkg::json::JsonNode spkg::json::ParseNode(std::istream &stream)
{
    Parser parser(stream);
    return parser.ParseNode();
}

bool spkg::json::IsUndefined(const JsonNode &node)
{
    return !!std::get_if<std::monostate>(&node);
}

bool spkg::json::IsNull(const JsonNode &node)
{
    return !!std::get_if<JsonNullNode>(&node);
}

bool spkg::json::IsBoolean(const JsonNode &node)
{
    return !!std::get_if<JsonBooleanNode>(&node);
}

bool spkg::json::IsNumber(const JsonNode &node)
{
    return !!std::get_if<JsonNumberNode>(&node);
}

bool spkg::json::IsString(const JsonNode &node)
{
    return !!std::get_if<JsonStringNode>(&node);
}

bool spkg::json::IsArray(const JsonNode &node)
{
    return !!std::get_if<JsonArrayNode>(&node);
}

bool spkg::json::IsObject(const JsonNode &node)
{
    return !!std::get_if<JsonObjectNode>(&node);
}

const spkg::json::JsonNullNode &spkg::json::AsNull(const JsonNode &node)
{
    return *std::get_if<JsonNullNode>(&node);
}

const spkg::json::JsonBooleanNode &spkg::json::AsBoolean(const JsonNode &node)
{
    return *std::get_if<JsonBooleanNode>(&node);
}

const spkg::json::JsonNumberNode &spkg::json::AsNumber(const JsonNode &node)
{
    return *std::get_if<JsonNumberNode>(&node);
}

const spkg::json::JsonStringNode &spkg::json::AsString(const JsonNode &node)
{
    return *std::get_if<JsonStringNode>(&node);
}

const spkg::json::JsonArrayNode &spkg::json::AsArray(const JsonNode &node)
{
    return *std::get_if<JsonArrayNode>(&node);
}

const spkg::json::JsonObjectNode &spkg::json::AsObject(const JsonNode &node)
{
    return *std::get_if<JsonObjectNode>(&node);
}

std::ostream &operator<<(std::ostream &stream, const spkg::json::JsonNode &node)
{
    if (std::get_if<std::monostate>(&node))
        return stream << "<undefined>";

    if (const auto cast = std::get_if<spkg::json::JsonNullNode>(&node))
        return cast->Print(stream);
    if (const auto cast = std::get_if<spkg::json::JsonBooleanNode>(&node))
        return cast->Print(stream);
    if (const auto cast = std::get_if<spkg::json::JsonNumberNode>(&node))
        return cast->Print(stream);
    if (const auto cast = std::get_if<spkg::json::JsonStringNode>(&node))
        return cast->Print(stream);
    if (const auto cast = std::get_if<spkg::json::JsonArrayNode>(&node))
        return cast->Print(stream);
    if (const auto cast = std::get_if<spkg::json::JsonObjectNode>(&node))
        return cast->Print(stream);

    return stream;
}

template<>
bool FromJson<bool>(const spkg::json::JsonNode &node, bool &value)
{
    if (!spkg::json::IsBoolean(node))
        return false;

    value = spkg::json::AsBoolean(node);
    return true;
}

template<>
void ToJson<bool>(spkg::json::JsonNode &node, const bool &value)
{
    node = spkg::json::JsonBooleanNode(value);
}

template<>
bool FromJson<long double>(const spkg::json::JsonNode &node, long double &value)
{
    if (!spkg::json::IsNumber(node))
        return false;

    value = spkg::json::AsNumber(node);
    return true;
}

template<>
void ToJson<long double>(spkg::json::JsonNode &node, const long double &value)
{
    node = spkg::json::JsonNumberNode(value);
}

template<>
bool FromJson<std::string>(const spkg::json::JsonNode &node, std::string &value)
{
    if (!spkg::json::IsString(node))
        return false;

    value = spkg::json::AsString(node);
    return true;
}

template<>
void ToJson<std::string>(spkg::json::JsonNode &node, const std::string &value)
{
    node = spkg::json::JsonStringNode(value);
}
