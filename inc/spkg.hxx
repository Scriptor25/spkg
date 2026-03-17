#pragma once

#include <filesystem>
#include <format>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace spkg
{
    enum class file_reference_type
    {
        file,
        manifest,
    };

    struct file_reference_t
    {
        file_reference_type type{};
        std::string path;
    };

    struct command_step_t
    {
        std::vector<std::string> command;

        std::string dir;
        std::map<std::string, std::string> env;
    };

    using command_list_t = std::vector<command_step_t>;

    struct fragment_t
    {
        std::string name;
        std::string description;
        
        std::string dir;
        std::map<std::string, std::string> env;
        
        command_list_t configure;
        command_list_t build;
        command_list_t install;

        std::vector<std::string> cache;
        std::vector<file_reference_t> files;
    };

    struct package_t
    {
        std::string id;
        std::string version;

        std::string name;
        std::string description;
        
        std::map<std::string, std::string> env;

        command_list_t setup;
        std::vector<std::string> cache;

        fragment_t main;
        std::map<std::string, fragment_t> fragments;
    };

    struct config_t
    {
        std::set<std::filesystem::path> packages;
        std::filesystem::path cache;

        std::map<std::string, std::set<std::string>> installed;
    };

    struct specifier_t
    {
        specifier_t() = default;

        explicit specifier_t(const std::string_view s)
        {
            const auto fragment_begin = s.find_last_of('/');
            const auto version_begin = s.find_last_of(':');

            const auto id_length = fragment_begin == std::string_view::npos
                                    ? version_begin == std::string_view::npos
                                            ? std::string_view::npos
                                            : version_begin
                                    : fragment_begin;
            id = s.substr(0, id_length);

            if (fragment_begin != std::string_view::npos)
            {
                const auto fragment_length = version_begin == std::string_view::npos
                                                ? std::string_view::npos
                                                : version_begin - fragment_begin - 1;
                fragment = s.substr(fragment_begin + 1, fragment_length);
            }

            if (version_begin != std::string_view::npos)
                version = s.substr(version_begin + 1);
        }

        std::string_view id;
        std::string_view fragment;
        std::string_view version;
    };

    std::ostream &operator<<(std::ostream &stream, const command_step_t &step)
    {
        if (!step.dir.empty())
            stream << "[" << step.dir << "] ";
        for (auto &[key, val] : step.env)
            stream << key << "=" << val << " ";
        for (auto it = step.command.begin(); it != step.command.end(); ++it)
        {
            if (it != step.command.begin())
                stream << ' ';
            stream << "'" << *it << "'";
        }
        return stream;
    }

    template<typename... A>
    auto log_warning(std::format_string<A...> &&format, A &&... args)
    {
        std::cerr << "Warning: " << std::format<A...>(std::forward<std::format_string<A...>>(format), std::forward<A>(args)...) << std::endl;
    }

    template<typename... A>
    auto log_error(std::format_string<A...> &&format, A &&... args)
    {
        std::cerr << "Error: " << std::format<A...>(std::forward<std::format_string<A...>>(format), std::forward<A>(args)...) << std::endl;
        return 1;
    }
}

template<>
struct std::formatter<spkg::command_step_t> : std::formatter<std::string>
{
    template<typename C>
    auto format(const spkg::command_step_t &step, C &context) const
    {
        std::string str;
        if (!step.dir.empty())
            str += "[" + step.dir + "] ";
        for (auto &[key, val] : step.env)
            str += key + "=" + val + " ";
        for (auto it = step.command.begin(); it != step.command.end(); ++it)
        {
            if (it != step.command.begin())
                str += ' ';
            str += "'" + *it + "'";
        }

        return std::formatter<std::string>::format(str, context);
    }
};
