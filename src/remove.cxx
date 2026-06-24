#include <config.hxx>
#include <log.hxx>
#include <spkg.hxx>

int spkg::Remove(Config &config, Specifier spec)
{
    if (!config.Installed.contains(spec))
        return Error("'{}' is not installed", spec);

    return Install(config, spec, {}, false, true);
}
