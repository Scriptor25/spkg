#include <spkg.hxx>

spkg::Specifier::Specifier(const std::string_view s)
{
    const auto fragment_begin = s.find_last_of('/');
    const auto version_begin = s.find_last_of(':');

    const auto id_length = fragment_begin == std::string_view::npos
                               ? version_begin == std::string_view::npos
                                     ? std::string_view::npos
                                     : version_begin
                               : fragment_begin;
    Id = s.substr(0, id_length);

    if (fragment_begin != std::string_view::npos)
    {
        const auto fragment_length = version_begin == std::string_view::npos
                                         ? std::string_view::npos
                                         : version_begin - fragment_begin - 1;
        Fragment = s.substr(fragment_begin + 1, fragment_length);
    }

    if (version_begin != std::string_view::npos)
        Version = s.substr(version_begin + 1);
}
