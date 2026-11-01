#include "MappingYamlInternal.h"

#include <cctype>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace Controller
{

using MappingDetail::YamlKind;
using MappingDetail::YamlValue;
using MappingDetail::SourceLine;

namespace
{

std::string trim (std::string_view text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();

    while (begin < end && std::isspace (static_cast<unsigned char> (text[begin])) != 0)
        ++begin;

    while (end > begin && std::isspace (static_cast<unsigned char> (text[end - 1])) != 0)
        --end;

    return std::string (text.substr (begin, end - begin));
}

/** Drops a trailing `# comment`, respecting quoted scalars. */
std::string stripComment (const std::string& line)
{
    bool inSingle = false;
    bool inDouble = false;
    bool escaped = false;

    for (std::size_t i = 0; i < line.size(); ++i)
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
                if (i + 1 < line.size() && line[i + 1] == '\'')
                    ++i; // doubled quote inside a single-quoted scalar
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

bool isSequenceEntry (const std::string& text)
{
    return text == "-" || (text.size() > 1 && text[0] == '-' && text[1] == ' ');
}

/** Splits `key: value` at the first unquoted colon that ends a key. */
bool splitKeyValue (const std::string& text, std::string& key, std::string& value)
{
    bool inSingle = false;
    bool inDouble = false;
    bool escaped = false;

    for (std::size_t i = 0; i < text.size(); ++i)
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

        if (i + 1 < text.size() && text[i + 1] != ' ' && text[i + 1] != '\t')
            continue;

        key = trim (std::string_view (text).substr (0, i));
        value = trim (std::string_view (text).substr (i + 1));

        return ! key.empty();
    }

    return false;
}

bool decodeQuotedScalar (const std::string& raw, std::string& out, MappingError& error, int line)
{
    if (raw.size() < 2 || raw.back() != raw.front())
    {
        error = { line, "unterminated quoted scalar" };
        return false;
    }

    const char quote = raw.front();

    for (std::size_t i = 1; i + 1 < raw.size(); ++i)
    {
        const char c = raw[i];

        if (quote == '\'')
        {
            if (c == '\'' && i + 2 < raw.size() && raw[i + 1] == '\'')
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

        if (i + 2 >= raw.size())
        {
            error = { line, "unterminated escape in a quoted scalar" };
            return false;
        }

        const char escape = raw[++i];

        switch (escape)
        {
            case 'n':  out.push_back ('\n'); break;
            case 't':  out.push_back ('\t'); break;
            case 'r':  out.push_back ('\r'); break;
            case '0':  out.push_back ('\0'); break;
            case '\\': out.push_back ('\\'); break;
            case '"':  out.push_back ('"'); break;
            case '\'': out.push_back ('\''); break;
            default:   out.push_back (escape); break;
        }
    }

    return true;
}

bool decodeScalar (const std::string& raw, YamlValue& out, MappingError& error, int line)
{
    out.kind = YamlKind::scalar;
    out.line = line;

    if (raw.empty())
    {
        out.kind = YamlKind::nullValue;
        return true;
    }

    const char first = raw.front();

    if (first == '"' || first == '\'')
        return decodeQuotedScalar (raw, out.scalar, error, line);

    if (raw == "~" || raw == "null" || raw == "Null" || raw == "NULL")
    {
        out.kind = YamlKind::nullValue;
        return true;
    }

    // Empty flow collections are the one flow-style form the schema needs:
    // they are how a document says "no pads" or "no macros".
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

class YamlParser
{
public:
    YamlParser (std::vector<SourceLine> sourceLines, MappingError& errorToFill)
        : lines (std::move (sourceLines)), error (errorToFill)
    {
    }

    bool parse (YamlValue& out)
    {
        if (lines.empty())
            return fail (1, "the document is empty");

        if (lines.front().indent != 0)
            return fail (lines.front().number, "the document must start at indentation 0");

        if (! parseBlock (0, out))
            return false;

        if (position != lines.size())
            return fail (lines[position].number, "unexpected content after the root mapping");

        return true;
    }

private:
    bool fail (int line, const std::string& message)
    {
        error = { line, message };
        return false;
    }

    bool parseBlock (int indent, YamlValue& out)
    {
        if (position >= lines.size() || lines[position].indent < indent)
        {
            out.kind = YamlKind::nullValue;
            return true;
        }

        if (lines[position].indent > indent)
            return fail (lines[position].number, "unexpected indentation");

        if (isSequenceEntry (lines[position].text))
            return parseSequence (indent, out);

        return parseMapping (indent, out);
    }

    bool parseMapping (int indent, YamlValue& out)
    {
        out.kind = YamlKind::mapping;
        out.line = lines[position].number;

        while (position < lines.size())
        {
            const auto& line = lines[position];

            if (line.indent < indent)
                break;

            if (line.indent > indent)
                return fail (line.number, "unexpected indentation");

            if (isSequenceEntry (line.text))
                return fail (line.number, "a sequence entry is not allowed in a mapping");

            std::string key;
            std::string rest;

            if (! splitKeyValue (line.text, key, rest))
                return fail (line.number, "expected 'key: value'");

            for (const auto& [existingKey, value] : out.mapping)
            {
                (void) value;

                if (existingKey == key)
                    return fail (line.number, "duplicate key '" + key + "'");
            }

            const int keyLine = line.number;
            ++position;

            YamlValue value;

            if (rest.empty())
            {
                if (position < lines.size() && lines[position].indent > indent)
                {
                    if (! parseBlock (lines[position].indent, value))
                        return false;
                }
                else
                {
                    value.kind = YamlKind::nullValue;
                    value.line = keyLine;
                }
            }
            else
            {
                if (! decodeScalar (rest, value, error, keyLine))
                    return false;

                if (position < lines.size() && lines[position].indent > indent)
                    return fail (lines[position].number,
                                 "a scalar value cannot have an indented child");
            }

            out.mapping.emplace_back (std::move (key), std::move (value));
        }

        return true;
    }

    bool parseSequence (int indent, YamlValue& out)
    {
        out.kind = YamlKind::sequence;
        out.line = lines[position].number;

        while (position < lines.size()
               && lines[position].indent == indent
               && isSequenceEntry (lines[position].text))
        {
            const SourceLine line = lines[position];
            const std::string rest = trim (std::string_view (line.text).substr (1));
            ++position;

            YamlValue item;
            item.line = line.number;

            std::string key;
            std::string value;

            if (rest.empty())
            {
                if (position < lines.size() && lines[position].indent > indent)
                {
                    if (! parseBlock (lines[position].indent, item))
                        return false;
                }
                else
                {
                    item.kind = YamlKind::nullValue;
                }
            }
            else if (splitKeyValue (rest, key, value))
            {
                // `- key: value` starts a mapping. The key's real column is
                // where the text after the dash begins, so further keys of the
                // same item line up there and a nested value of the first key
                // stays deeper.
                int keyOffset = 1;

                while (keyOffset < static_cast<int> (line.text.size())
                       && line.text[static_cast<std::size_t> (keyOffset)] == ' ')
                    ++keyOffset;

                const int childIndent = indent + keyOffset;

                lines[position - 1] = SourceLine { childIndent, rest, line.number };
                --position;

                if (! parseMapping (childIndent, item))
                    return false;
            }
            else
            {
                if (! decodeScalar (rest, item, error, line.number))
                    return false;

                if (position < lines.size() && lines[position].indent > indent)
                    return fail (lines[position].number,
                                 "a scalar sequence item cannot have an indented child");
            }

            out.sequence.push_back (std::move (item));
        }

        return true;
    }

    std::vector<SourceLine> lines;
    MappingError& error;
    std::size_t position = 0;
};

bool tokenize (const std::string& yaml, std::vector<SourceLine>& lines, MappingError& error)
{
    std::istringstream stream (yaml);
    std::string raw;
    int number = 0;

    while (std::getline (stream, raw))
    {
        ++number;

        if (! raw.empty() && raw.back() == '\r')
            raw.pop_back();

        const std::string withoutComment = stripComment (raw);

        int indent = 0;

        while (indent < static_cast<int> (withoutComment.size()))
        {
            const char c = withoutComment[static_cast<std::size_t> (indent)];

            if (c == ' ')
            {
                ++indent;
                continue;
            }

            if (c == '\t')
            {
                error = { number, "tab indentation is not supported" };
                return false;
            }

            break;
        }

        const std::string text = trim (std::string_view (withoutComment).substr (
            static_cast<std::size_t> (indent)));

        if (text.empty())
            continue;

        if (indent == 0 && (text == "---" || text == "..."))
            continue;

        lines.push_back (SourceLine { indent, text, number });
    }

    return true;
}

} // namespace

namespace MappingDetail
{

bool readFileContents (const std::filesystem::path& file, std::string& out, std::string& errorText)
{
    std::ifstream stream (file, std::ios::binary);

    if (! stream)
    {
        errorText = "cannot open '" + file.string() + "'";
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();

    if (stream.bad())
    {
        errorText = "cannot read '" + file.string() + "'";
        return false;
    }

    out = buffer.str();
    return true;
}

std::uint64_t hashContents (const std::string& contents)
{
    return static_cast<std::uint64_t> (std::hash<std::string> {} (contents));
}

} // namespace MappingDetail

bool parseSetMappingYaml (const std::string& yaml, SetMapping& result, MappingError& error)
{
    error = {};

    std::vector<SourceLine> lines;

    if (! tokenize (yaml, lines, error))
        return false;

    YamlValue root;
    YamlParser parser (std::move (lines), error);

    if (! parser.parse (root))
        return false;

    SetMapping parsed;

    if (! MappingDetail::decodeDocument (root, parsed, error))
        return false;

    result = std::move (parsed);
    return true;
}

bool loadSetMappingFile (const std::filesystem::path& file,
                         SetMapping& result,
                         MappingError& error)
{
    error = {};

    std::string contents;
    std::string readError;

    if (! MappingDetail::readFileContents (file, contents, readError))
    {
        error = { 0, readError };
        return false;
    }

    return parseSetMappingYaml (contents, result, error);
}

} // namespace Controller
