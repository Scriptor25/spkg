#pragma once

#include <json/json.hxx>

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace spkg
{
    using PersistVal = std::string;
    using PersistVec = std::vector<PersistVal>;
    using PersistEntry = std::variant<PersistVal, PersistVec>;
    using PersistMap = std::map<std::string, PersistEntry>;
}
