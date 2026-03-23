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
        Specifier(std::string id, std::string fragment, std::string version);

        operator std::string() const;

        Specifier Normalized() const;

        bool HasFragment() const;
        bool HasVersion() const;

        std::string GetFragmentOr(std::string default_value = {}) const;
        std::string GetVersionOr(std::string default_value = {}) const;

        std::string Id;
        std::optional<std::string> Fragment;
        std::optional<std::string> Version;
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
