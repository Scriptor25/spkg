#pragma once

#include <json.hxx>

namespace spkg::json
{
    class Parser final
    {
    public:
        explicit Parser(std::istream &stream);

        JsonNode ParseNode();
        JsonNode ParseValueNode();

        JsonNode ParseNullNode();
        JsonNode ParseBooleanNode();
        JsonNode ParseNumberNode();
        JsonNode ParseStringNode();
        JsonNode ParseArrayNode();
        JsonNode ParseObjectNode();

    protected:
        void Next();

        bool Skip(int c);
        bool SkipWhitespace();

    private:
        std::istream &m_Stream;
        int m_Buffer;
    };
}
