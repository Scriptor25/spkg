#pragma once

#include <format>
#include <optional>
#include <string>

namespace spkg
{
    struct Specifier
    {
        Specifier() = default;
        Specifier(const std::string &s);
        Specifier(std::string id, std::optional<std::string> fragment);

        operator std::string() const;

        std::string Id;
        std::optional<std::string> Fragment;
    };
}

template<>
struct std::formatter<spkg::Specifier> : std::formatter<std::string>
{
    template<typename C>
    auto format(const spkg::Specifier &specifier, C &context) const
    {
        return std::formatter<std::string>::format(specifier, context);
    }
};
