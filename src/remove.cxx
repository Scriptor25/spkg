#include <spkg.hxx>

int spkg::Remove(Config &config, const std::string_view arg)
{
    const Specifier specifier(arg);

    // TODO: remove files specified by package

    return 0;
}
