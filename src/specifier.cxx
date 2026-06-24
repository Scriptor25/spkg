#include <specifier.hxx>

spkg::Specifier::Specifier(std::string_view s)
{
    if (const auto pos = s.find(':'); pos != std::string::npos)
    {
        Id = s.substr(0, pos);
        Fragment = s.substr(pos + 1);
    }
    else
    {
        Id = s;
        Fragment = "default";
    }
}

spkg::Specifier::Specifier(const std::string &s)
    : Specifier(std::string_view(s))
{
}

spkg::Specifier::Specifier(std::string id, std::string fragment)
    : Id(std::move(id)),
      Fragment(std::move(fragment))
{
}

spkg::Specifier::operator std::string() const
{
    return Id + ':' + Fragment;
}
