#include <config.hxx>
#include <specifier.hxx>
#include <spkg.hxx>

#include <ranges>

toolkit::result<> spkg::Update(Config &config, const std::optional<Specifier> &spec)
{
    if (spec)
        return Install(config, *spec, {}, false, false);

    for (auto &key : config.Installed | std::views::keys)
        if (auto res = Install(config, key, {}, false, false); !res)
            return res;
    return {};
}
