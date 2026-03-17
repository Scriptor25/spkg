#include <spkg.hxx>

int spkg::Update(Config &config, const std::string_view arg)
{
    const Specifier specifier(arg);

    // TODO: install without cache

    return 0;
}
