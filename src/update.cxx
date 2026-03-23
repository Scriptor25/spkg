#include <ranges>

#include <spkg.hxx>

int spkg::Update(Config &config, const std::optional<Specifier> arg)
{
    if (arg.has_value())
        return Install(config, arg.value(), false, false);

    for (auto &key : config.Installed | std::views::keys)
        if (auto error = Install(config, key, false, false))
            return error;
    return 0;
}
