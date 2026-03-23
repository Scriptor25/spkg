#include <ranges>

#include <spkg.hxx>

int spkg::Update(Config &config, const std::optional<Specifier> &arg)
{
    if (arg)
        return Install(config, *arg, false, false);

    for (auto &key : config.Installed | std::views::keys)
        if (const auto error = Install(config, key, false, false))
            return error;
    return 0;
}
