#include <specifier.hxx>

spkg::Specifier::Specifier(const std::string &s)
{
    const auto fragment_begin = s.find_last_of('/');
    const auto version_begin = s.find_last_of(':');

    const auto id_length = fragment_begin == std::string::npos
                               ? version_begin == std::string::npos
                                     ? std::string::npos
                                     : version_begin
                               : fragment_begin;
    Id = s.substr(0, id_length);

    if (fragment_begin != std::string::npos)
    {
        const auto fragment_length = version_begin == std::string::npos
                                         ? std::string::npos
                                         : version_begin - fragment_begin - 1;
        Fragment = s.substr(fragment_begin + 1, fragment_length);
    }

    if (version_begin != std::string::npos)
        Version = s.substr(version_begin + 1);
}

spkg::Specifier::Specifier(std::string id, std::string fragment, std::string version)
    : Id(id), Fragment(fragment), Version(version)
{
}

spkg::Specifier::operator std::string() const
{
    std::string s = Id;
    if (Fragment.has_value())
        s += '/' + Fragment.value();
    if (Version.has_value())
        s += ':' + Version.value();
    return s;
}

spkg::Specifier spkg::Specifier::Normalized() const
{
    return Specifier(Id, Fragment.value_or("default"), Version.value_or("latest"));
}

bool spkg::Specifier::HasFragment() const
{
    return Fragment.has_value();
}

bool spkg::Specifier::HasVersion() const
{
    return Version.has_value();
}

std::string spkg::Specifier::GetFragmentOr(std::string default_value) const
{
    return Fragment.value_or(default_value);
}

std::string spkg::Specifier::GetVersionOr(std::string default_value) const
{
    return Version.value_or(default_value);
}
