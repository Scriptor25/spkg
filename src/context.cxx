#include <ranges>

#include <context.hxx>

bool spkg::Context::GetVariable(std::string key, std::string &value) const
{
    for (auto &frame : std::ranges::reverse_view(Stack))
        if (auto it = frame.find(key); it != frame.end())
        {
            value = it->second;
            return true;
        }
    return false;
}
