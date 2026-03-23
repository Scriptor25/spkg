#include <log.hxx>
#include <spkg.hxx>

int spkg::Remove(Config &config, Specifier arg)
{
    const Specifier cache_key(arg.Id, arg.Fragment.value_or("default"));
    if (!config.Installed.contains(cache_key))
        return Error("'{}' is not installed", arg);

    return Install(config, arg, false, true);
}
