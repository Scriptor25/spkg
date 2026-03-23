#pragma once

#include <format>
#include <iostream>

namespace spkg
{
    template<typename... A>
    auto Info(std::format_string<A...> &&format, A &&... args)
    {
        std::cerr << "Info: " << std::format<A...>(
            std::forward<std::format_string<A...>>(format),
            std::forward<A>(args)...) << std::endl;
    }

    template<typename... A>
    auto Warning(std::format_string<A...> &&format, A &&... args)
    {
        std::cerr << "Warning: " << std::format<A...>(
            std::forward<std::format_string<A...>>(format),
            std::forward<A>(args)...) << std::endl;
    }

    template<typename... A>
    auto Error(std::format_string<A...> &&format, A &&... args)
    {
        std::cerr << "Error: " << std::format<A...>(
            std::forward<std::format_string<A...>>(format),
            std::forward<A>(args)...) << std::endl;
        return 1;
    }
}
