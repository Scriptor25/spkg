#pragma once

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace spkg::json
{
    struct JsonNullNode;
    struct JsonBooleanNode;
    struct JsonNumberNode;
    struct JsonStringNode;
    struct JsonArrayNode;
    struct JsonObjectNode;

    using JsonNode = std::variant<
        std::monostate,
        JsonNullNode,
        JsonBooleanNode,
        JsonNumberNode,
        JsonStringNode,
        JsonArrayNode,
        JsonObjectNode
    >;

    struct JsonNodeBase
    {
        virtual ~JsonNodeBase() = default;

        virtual std::ostream &Print(std::ostream &stream) const = 0;
    };

    struct JsonNullNode final : JsonNodeBase
    {
        JsonNullNode() = default;

        std::ostream &Print(std::ostream &stream) const override;
    };

    struct JsonBooleanNode final : JsonNodeBase
    {
        explicit JsonBooleanNode(bool value);

        std::ostream &Print(std::ostream &stream) const override;

        operator bool() const;

        bool Value;
    };

    struct JsonNumberNode final : JsonNodeBase
    {
        explicit JsonNumberNode(long double value);

        std::ostream &Print(std::ostream &stream) const override;

        operator long double() const;

        long double Value;
    };

    struct JsonStringNode final : JsonNodeBase
    {
        explicit JsonStringNode(std::string value);

        std::ostream &Print(std::ostream &stream) const override;

        std::string::iterator begin();
        std::string::iterator end();

        [[nodiscard]] std::string::const_iterator begin() const;
        [[nodiscard]] std::string::const_iterator end() const;

        operator std::string() const;

        std::string Value;
    };

    struct JsonArrayNode final : JsonNodeBase
    {
        JsonArrayNode() = default;
        explicit JsonArrayNode(std::size_t size);
        explicit JsonArrayNode(std::vector<JsonNode> elements);

        std::ostream &Print(std::ostream &stream) const override;

        std::vector<JsonNode>::iterator begin();
        std::vector<JsonNode>::iterator end();

        [[nodiscard]] std::vector<JsonNode>::const_iterator begin() const;
        [[nodiscard]] std::vector<JsonNode>::const_iterator end() const;

        JsonNode &operator[](std::size_t index);
        const JsonNode &operator[](std::size_t index) const;

        [[nodiscard]] std::size_t size() const;

        std::vector<JsonNode> Elements;
    };

    struct JsonObjectNode final : JsonNodeBase
    {
        JsonObjectNode() = default;
        explicit JsonObjectNode(std::map<std::string, JsonNode> elements);

        std::ostream &Print(std::ostream &stream) const override;

        std::map<std::string, JsonNode>::iterator begin();
        std::map<std::string, JsonNode>::iterator end();

        [[nodiscard]] std::map<std::string, JsonNode>::const_iterator begin() const;
        [[nodiscard]] std::map<std::string, JsonNode>::const_iterator end() const;

        JsonNode &operator[](const std::string &key);

        [[nodiscard]] JsonNode operator[](const std::string &key) const;

        std::map<std::string, JsonNode> Elements;
    };

    JsonNode ParseNode(std::istream &stream);

    bool IsUndefined(const JsonNode &node);
    bool IsNull(const JsonNode &node);
    bool IsBoolean(const JsonNode &node);
    bool IsNumber(const JsonNode &node);
    bool IsString(const JsonNode &node);
    bool IsArray(const JsonNode &node);
    bool IsObject(const JsonNode &node);

    const JsonNullNode &AsNull(const JsonNode &node);
    const JsonBooleanNode &AsBoolean(const JsonNode &node);
    const JsonNumberNode &AsNumber(const JsonNode &node);
    const JsonStringNode &AsString(const JsonNode &node);
    const JsonArrayNode &AsArray(const JsonNode &node);
    const JsonObjectNode &AsObject(const JsonNode &node);

    template<typename T>
    T Parse(std::istream &stream);

    template<typename T>
    T As(const JsonNode &node);
}

std::ostream &operator<<(std::ostream &stream, const spkg::json::JsonNode &node);

template<typename T>
bool FromJson(const spkg::json::JsonNode &node, T &value) = delete;

template<typename T>
void ToJson(spkg::json::JsonNode &node, const T &value) = delete;

template<typename T>
bool FromJson(const spkg::json::JsonNode &node, std::vector<T> &value);

template<typename T>
void ToJson(spkg::json::JsonNode &node, const std::vector<T> &value);

template<typename T, std::size_t N>
bool FromJson(const spkg::json::JsonNode &node, std::array<T, N> &value);

template<typename T, std::size_t N>
void ToJson(spkg::json::JsonNode &node, const std::array<T, N> &value);

template<typename T>
bool FromJson(const spkg::json::JsonNode &node, std::optional<T> &value);

template<typename T>
void ToJson(spkg::json::JsonNode &node, const std::optional<T> &value);

template<typename T>
bool FromJson(const spkg::json::JsonNode &node, std::map<std::string, T> &value);

template<typename T>
void ToJson(spkg::json::JsonNode &node, const std::map<std::string, T> &value);

template<>
bool FromJson(const spkg::json::JsonNode &node, bool &value);

template<>
void ToJson(spkg::json::JsonNode &node, const bool &value);

template<>
bool FromJson(const spkg::json::JsonNode &node, long double &value);

template<>
void ToJson(spkg::json::JsonNode &node, const long double &value);

template<>
bool FromJson(const spkg::json::JsonNode &node, std::string &value);

template<>
void ToJson(spkg::json::JsonNode &node, const std::string &value);

template<typename T>
bool FromJson(const spkg::json::JsonNode &node, std::vector<T> &value)
{
    if (!spkg::json::IsArray(node))
        return false;

    auto &array_node = spkg::json::AsArray(node);

    value.resize(array_node.size());
    for (std::size_t i = 0; i < array_node.size(); ++i)
        if (!FromJson(array_node[i], value[i]))
            return false;
    return true;
}

template<typename T>
void ToJson(spkg::json::JsonNode &node, const std::vector<T> &value)
{
    spkg::json::JsonArrayNode array_node(value.size());
    for (std::size_t i = 0; i < value.size(); ++i)
        ToJson(array_node[i], value[i]);
    node = std::move(array_node);
}

template<typename T, std::size_t N>
bool FromJson(const spkg::json::JsonNode &node, std::array<T, N> &value)
{
    if (!spkg::json::IsArray(node))
        return false;

    auto &array_node = spkg::json::AsArray(node);
    if (array_node.size() != N)
        return false;

    for (std::size_t i = 0; i < N; ++i)
        if (!FromJson(array_node[i], value[i]))
            return false;
    return true;
}

template<typename T, std::size_t N>
void ToJson(spkg::json::JsonNode &node, const std::array<T, N> &value)
{
    spkg::json::JsonArrayNode array_node(N);
    for (std::size_t i = 0; i < N; ++i)
        ToJson<T>(array_node[i], value[i]);
    node = std::move(array_node);
}

template<typename T>
bool FromJson(const spkg::json::JsonNode &node, std::optional<T> &value)
{
    if (spkg::json::IsUndefined(node))
    {
        value = std::nullopt;
        return true;
    }

    T element;
    if (!FromJson(node, element))
        return false;

    value = std::move(element);
    return true;
}

template<typename T>
void ToJson(spkg::json::JsonNode &node, const std::optional<T> &value)
{
    if (value.has_value())
        ToJson<T>(node, value.value());
}

template<typename T>
bool FromJson(const spkg::json::JsonNode &node, std::map<std::string, T> &value)
{
    if (!spkg::json::IsObject(node))
        return false;

    auto &object_node = spkg::json::AsObject(node);
    for (auto &[key, val] : object_node)
        if (!FromJson(val, value[key]))
            return false;

    return true;
}

template<typename T>
void ToJson(spkg::json::JsonNode &node, const std::map<std::string, T> &value)
{
    spkg::json::JsonObjectNode object_node;
    for (auto &[key, val] : value)
        ToJson(object_node[key], val);
    node = std::move(object_node);
}

namespace spkg::json
{
    template<typename T>
    T Parse(std::istream &stream)
    {
        auto node = ParseNode(stream);
        if (T value; FromJson(node, value))
            return value;
        throw std::runtime_error("conversion error");
    }

    template<typename T>
    T As(const JsonNode &node)
    {
        if (T value; FromJson(node, value))
            return value;
        throw std::runtime_error("conversion error");
    }
}
