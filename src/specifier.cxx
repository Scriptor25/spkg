#include <specifier.hxx>

spkg::Specifier::Specifier(const std::string &s)
{
    if (auto pos = s.find(':'); pos != std::string::npos)
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
    : Id(id), Fragment(fragment)
{
}

spkg::Specifier::operator std::string() const
{
    if (Fragment)
        return Id + ':' + *Fragment;
    return Id;
}
