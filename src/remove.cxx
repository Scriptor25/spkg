#include <log.hxx>
#include <spkg.hxx>

int spkg::Remove(Config &config, Specifier arg)
{
    auto package_id = arg.Id;
    auto fragment_id = arg.GetFragmentOr("default");
    auto version = arg.GetVersionOr("latest");

    Specifier cache_key(package_id, fragment_id, version);
    if (!config.Installed.contains(cache_key))
        return Error("'{}' is not installed", arg);

    return Install(config, arg, false, true);
}
