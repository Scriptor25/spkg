#include <config.hxx>
#include <spkg.hxx>

toolkit::result<> spkg::Remove(Config &config, Specifier spec)
{
    if (!config.Installed.contains(spec))
        return toolkit::make_error("'{}' is not installed", spec);

    return Install(config, spec, {}, false, true);
}
