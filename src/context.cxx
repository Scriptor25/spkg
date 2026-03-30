#include <context.hxx>

#include <ranges>

bool spkg::Context::GetVariable(const std::string &key, PersistEntry &value) const
{
    for (auto &frame : std::ranges::reverse_view(Stack))
        if (auto it = frame.find(key); it != frame.end())
        {
            value = it->second;
            return true;
        }
    return false;
}

bool spkg::Context::GetVariable(const std::string &key, PersistVal &value) const
{
    for (auto &frame : std::ranges::reverse_view(Stack))
        if (auto it = frame.find(key); it != frame.end())
        {
            if (auto p = std::get_if<PersistVal>(&it->second))
            {
                value = *p;
                return true;
            }
            return false;
        }
    return false;
}
