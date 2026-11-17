#include "controller/mapping/YamlParser.h"

#include "controller/mapping/YamlScalar.h"
#include "controller/mapping/YamlScanner.h"

#include <utility>

namespace Controller::MappingDetail {

namespace {

class YamlParser {
public:
    YamlParser(std::vector<SourceLine> sourceLines, MappingError& errorToFill)
        : lines(std::move(sourceLines)), error(errorToFill) {}

    bool parse(YamlValue& out) {
        if (lines.empty()) return fail(1, "the document is empty");

        if (lines.front().indent != 0)
            return fail(lines.front().number, "the document must start at indentation 0");

        if (!parseBlock(0, out)) return false;

        if (position != lines.size())
            return fail(lines[position].number, "unexpected content after the root mapping");

        return true;
    }

private:
    bool fail(int line, const std::string& message) {
        error = {line, message};
        return false;
    }

    bool parseBlock(int indent, YamlValue& out) {
        if (position >= lines.size() || lines[position].indent < indent) {
            out.kind = YamlKind::nullValue;
            return true;
        }

        if (lines[position].indent > indent)
            return fail(lines[position].number, "unexpected indentation");

        if (isSequenceEntry(lines[position].text)) return parseSequence(indent, out);

        return parseMapping(indent, out);
    }

    bool parseMapping(int indent, YamlValue& out) {
        out.kind = YamlKind::mapping;
        out.line = lines[position].number;

        while (position < lines.size()) {
            const auto& line = lines[position];

            if (line.indent < indent) break;

            if (line.indent > indent) return fail(line.number, "unexpected indentation");

            if (isSequenceEntry(line.text))
                return fail(line.number, "a sequence entry is not allowed in a mapping");

            std::string key;
            std::string rest;

            if (!splitKeyValue(line.text, key, rest))
                return fail(line.number, "expected 'key: value'");

            for (const auto& [existingKey, value] : out.mapping) {
                (void)value;

                if (existingKey == key) return fail(line.number, "duplicate key '" + key + "'");
            }

            const int keyLine = line.number;
            ++position;

            YamlValue value;

            if (rest.empty()) {
                if (position < lines.size() && lines[position].indent > indent) {
                    if (!parseBlock(lines[position].indent, value)) return false;
                } else {
                    value.kind = YamlKind::nullValue;
                    value.line = keyLine;
                }
            } else {
                if (!decodeScalar(rest, value, error, keyLine)) return false;

                if (position < lines.size() && lines[position].indent > indent)
                    return fail(lines[position].number,
                                "a scalar value cannot have an indented child");
            }

            out.mapping.emplace_back(std::move(key), std::move(value));
        }

        return true;
    }

    bool parseSequence(int indent, YamlValue& out) {
        out.kind = YamlKind::sequence;
        out.line = lines[position].number;

        while (position < lines.size() && lines[position].indent == indent &&
               isSequenceEntry(lines[position].text)) {
            const SourceLine line = lines[position];
            const std::string rest = trim(std::string_view(line.text).substr(1));
            ++position;

            YamlValue item;
            item.line = line.number;

            std::string key;
            std::string value;

            if (rest.empty()) {
                if (position < lines.size() && lines[position].indent > indent) {
                    if (!parseBlock(lines[position].indent, item)) return false;
                } else {
                    item.kind = YamlKind::nullValue;
                }
            } else if (splitKeyValue(rest, key, value)) {
                int keyOffset = 1;

                while (keyOffset < static_cast<int>(line.text.size()) &&
                       line.text[static_cast<std::size_t>(keyOffset)] == ' ')
                    ++keyOffset;

                const int childIndent = indent + keyOffset;

                lines[position - 1] = SourceLine{childIndent, rest, line.number};
                --position;

                if (!parseMapping(childIndent, item)) return false;
            } else {
                if (!decodeScalar(rest, item, error, line.number)) return false;

                if (position < lines.size() && lines[position].indent > indent)
                    return fail(lines[position].number,
                                "a scalar sequence item cannot have an indented child");
            }

            out.sequence.push_back(std::move(item));
        }

        return true;
    }

    std::vector<SourceLine> lines;
    MappingError& error;
    std::size_t position = 0;
};

}  // namespace

bool parseYaml(const std::string& yaml, YamlValue& out, MappingError& error) {
    std::vector<SourceLine> lines;

    if (!tokenize(yaml, lines, error)) return false;

    YamlParser parser(std::move(lines), error);
    return parser.parse(out);
}

}  // namespace Controller::MappingDetail
