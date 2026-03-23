#include <specifier.hxx>

spkg::Specifier::Specifier(const std::string &s)
{
    if (const auto pos = s.find(':'); pos != std::string::npos)
    {
        Id = s.substr(0, pos);
        Fragment = s.substr(pos + 1);
    }
    else
    {
        Id = s;
    }
}

spkg::Specifier::Specifier(std::string id, std::optional<std::string> fragment)
    : Id(std::move(id)),
      Fragment(std::move(fragment))
{
}

spkg::Specifier::operator std::string() const
{
    if (Fragment)
        return Id + ':' + *Fragment;
    return Id;
}
