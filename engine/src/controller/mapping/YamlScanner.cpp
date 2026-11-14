#include "controller/mapping/YamlScanner.h"

#include <cctype>
#include <sstream>

namespace Controller::MappingDetail
{

    std::string trim (std::string_view text)
    {
        std::size_t begin = 0;
        std::size_t end = text.size ();

        while (begin < end && std::isspace (static_cast<unsigned char> (text[begin])) != 0)
            ++begin;

        while (end > begin && std::isspace (static_cast<unsigned char> (text[end - 1])) != 0)
            --end;

        return std::string (text.substr (begin, end - begin));
    }

    namespace
    {

        std::string stripComment (const std::string& line)
        {
            bool inSingle = false;
            bool inDouble = false;
            bool escaped = false;

            for (std::size_t i = 0; i < line.size (); ++i)
            {
                const char c = line[i];

                if (inDouble)
                {
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (c == '\\')
                    {
                        escaped = true;
                    }
                    else if (c == '"')
                    {
                        inDouble = false;
                    }

                    continue;
                }

                if (inSingle)
                {
                    if (c == '\'')
                    {
                        if (i + 1 < line.size () && line[i + 1] == '\'')
                            ++i;
                        else
                            inSingle = false;
                    }

                    continue;
                }

                if (c == '"')
                    inDouble = true;
                else if (c == '\'')
                    inSingle = true;
                else if (c == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t'))
                    return line.substr (0, i);
            }

            return line;
        }

    }

    bool isSequenceEntry (const std::string& text)
    {
        return text == "-" || (text.size () > 1 && text[0] == '-' && text[1] == ' ');
    }

    bool splitKeyValue (const std::string& text, std::string& key, std::string& value)
    {
        bool inSingle = false;
        bool inDouble = false;
        bool escaped = false;

        for (std::size_t i = 0; i < text.size (); ++i)
        {
            const char c = text[i];

            if (inDouble)
            {
                if (escaped)
                    escaped = false;
                else if (c == '\\')
                    escaped = true;
                else if (c == '"')
                    inDouble = false;

                continue;
            }

            if (inSingle)
            {
                if (c == '\'')
                    inSingle = false;

                continue;
            }

            if (c == '"')
            {
                inDouble = true;
                continue;
            }

            if (c == '\'')
            {
                inSingle = true;
                continue;
            }

            if (c != ':')
                continue;

            if (i + 1 < text.size () && text[i + 1] != ' ' && text[i + 1] != '\t')
                continue;

            key = trim (std::string_view (text).substr (0, i));
            value = trim (std::string_view (text).substr (i + 1));

            return !key.empty ();
        }

        return false;
    }

    bool tokenize (const std::string& yaml, std::vector<SourceLine>& lines, MappingError& error)
    {
        std::istringstream stream (yaml);
        std::string raw;
        int number = 0;

        while (std::getline (stream, raw))
        {
            ++number;

            if (!raw.empty () && raw.back () == '\r')
                raw.pop_back ();

            const std::string withoutComment = stripComment (raw);

            int indent = 0;

            while (indent < static_cast<int> (withoutComment.size ()))
            {
                const char c = withoutComment[static_cast<std::size_t> (indent)];

                if (c == ' ')
                {
                    ++indent;
                    continue;
                }

                if (c == '\t')
                {
                    error = {number, "tab indentation is not supported"};
                    return false;
                }

                break;
            }

            const std::string text =
                trim (std::string_view (withoutComment).substr (static_cast<std::size_t> (indent)));

            if (text.empty ())
                continue;

            if (indent == 0 && (text == "---" || text == "..."))
                continue;

            lines.push_back (SourceLine{indent, text, number});
        }

        return true;
    }

}
