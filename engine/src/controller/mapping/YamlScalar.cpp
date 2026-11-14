#include "controller/mapping/YamlScalar.h"

namespace Controller::MappingDetail
{

    namespace
    {

        bool
        decodeQuotedScalar (const std::string& raw, std::string& out, MappingError& error, int line)
        {
            if (raw.size () < 2 || raw.back () != raw.front ())
            {
                error = {line, "unterminated quoted scalar"};
                return false;
            }

            const char quote = raw.front ();

            for (std::size_t i = 1; i + 1 < raw.size (); ++i)
            {
                const char c = raw[i];

                if (quote == '\'')
                {
                    if (c == '\'' && i + 2 < raw.size () && raw[i + 1] == '\'')
                    {
                        out.push_back ('\'');
                        ++i;
                    }
                    else
                    {
                        out.push_back (c);
                    }

                    continue;
                }

                if (c != '\\')
                {
                    out.push_back (c);
                    continue;
                }

                if (i + 2 >= raw.size ())
                {
                    error = {line, "unterminated escape in a quoted scalar"};
                    return false;
                }

                const char escape = raw[++i];

                switch (escape)
                {
                    case 'n': out.push_back ('\n'); break;
                    case 't': out.push_back ('\t'); break;
                    case 'r': out.push_back ('\r'); break;
                    case '0': out.push_back ('\0'); break;
                    case '\\': out.push_back ('\\'); break;
                    case '"': out.push_back ('"'); break;
                    case '\'': out.push_back ('\''); break;
                    default: out.push_back (escape); break;
                }
            }

            return true;
        }

    }

    bool decodeScalar (const std::string& raw, YamlValue& out, MappingError& error, int line)
    {
        out.kind = YamlKind::scalar;
        out.line = line;

        if (raw.empty ())
        {
            out.kind = YamlKind::nullValue;
            return true;
        }

        const char first = raw.front ();

        if (first == '"' || first == '\'')
            return decodeQuotedScalar (raw, out.scalar, error, line);

        if (raw == "~" || raw == "null" || raw == "Null" || raw == "NULL")
        {
            out.kind = YamlKind::nullValue;
            return true;
        }

        if (raw == "[]")
        {
            out.kind = YamlKind::sequence;
            return true;
        }

        if (raw == "{}")
        {
            out.kind = YamlKind::mapping;
            return true;
        }

        out.scalar = raw;
        return true;
    }

}
