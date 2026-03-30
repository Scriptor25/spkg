#include <log.hxx>
#include <spkg.hxx>

int spkg::Remove(Config &config, Specifier arg)
{
    if (!config.Installed.contains(arg))
        return Error("'{}' is not installed", arg);

    return Install(config, arg, false, true);
}
