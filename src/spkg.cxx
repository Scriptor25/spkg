#include <spkg.hxx>

std::ostream &spkg::operator<<(std::ostream &stream, const CommandStep &step)
{
    if (!step.Dir.empty())
        stream << "[" << step.Dir << "] ";
    for (auto &[key, val] : step.Env)
        stream << key << "=" << val << " ";
    for (auto it = step.Command.begin(); it != step.Command.end(); ++it)
    {
        if (it != step.Command.begin())
            stream << ' ';
        stream << "'" << *it << "'";
    }
    return stream;
}
