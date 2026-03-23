#include <package.hxx>

std::ostream &spkg::operator<<(std::ostream &stream, const Command &command)
{
    if (!command.Dir.empty())
        stream << '[' << command.Dir << "] ";
    for (auto &[key, val] : command.Env)
        stream << key << '=' << val << ' ';
    for (auto it = command.Args.begin(); it != command.Args.end(); ++it)
    {
        if (it != command.Args.begin())
            stream << ' ';
        stream << '\'' << *it << '\'';
    }
    return stream;
}
