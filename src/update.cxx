#include <config.hxx>
#include <specifier.hxx>
#include <spkg.hxx>

#include <ranges>

int spkg::Update(Config &config, const std::optional<Specifier> &spec)
{
    if (spec)
        return Install(config, *spec, {}, false, false);

    for (auto &key : config.Installed | std::views::keys)
        if (const auto error = Install(config, key, {}, false, false))
            return error;
    return 0;
}
