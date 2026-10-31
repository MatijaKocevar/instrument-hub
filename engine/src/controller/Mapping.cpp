#include "Mapping.h"

#include "LaunchpadProtocol.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace Controller
{

//==============================================================================
// SetMapping
//==============================================================================

const InstrumentSpec* FamilySpec::findInstrument (const std::string& instrumentId) const noexcept
{
    for (const auto& instrument : instruments)
        if (instrument.id == instrumentId)
            return &instrument;

    return nullptr;
}

const PadBinding* SetMapping::padBinding (int pad) const noexcept
{
    if (pad < 0 || pad >= SetMapping::maximumPads)
        return nullptr;

    const auto& entry = pads[static_cast<std::size_t> (pad)];
    return entry.has_value() ? &*entry : nullptr;
}

const EncoderBinding* SetMapping::encoderBinding (int encoder) const noexcept
{
    if (encoder < 0 || encoder >= SetMapping::maximumEncoders)
        return nullptr;

    const auto& entry = encoders[static_cast<std::size_t> (encoder)];
    return entry.has_value() ? &*entry : nullptr;
}

const MacroBankSpec* SetMapping::bankBinding (int bank) const noexcept
{
    if (bank < 0 || bank >= SetMapping::maximumBanks)
        return nullptr;

    const auto& entry = banks[static_cast<std::size_t> (bank)];
    return entry.has_value() ? &*entry : nullptr;
}

const SceneSpec* SetMapping::sceneBinding (int scene) const noexcept
{
    if (scene < 0 || scene >= SetMapping::maximumScenes)
        return nullptr;

    const auto& entry = scenes[static_cast<std::size_t> (scene)];
    return entry.has_value() ? &*entry : nullptr;
}

const InstrumentSpec* SetMapping::instrumentFor (const PadBinding& binding) const noexcept
{
    const auto family = families.find (binding.family);

    if (family == families.end())
        return nullptr;

    return family->second.findInstrument (binding.instrument);
}

const MacroSpec* SetMapping::macroFor (const EncoderBinding& binding) const noexcept
{
    const auto macro = macros.find (binding.macro);
    return macro == macros.end() ? nullptr : &macro->second;
}

int SetMapping::countPads() const noexcept
{
    int count = 0;

    for (const auto& entry : pads)
        if (entry.has_value())
            ++count;

    return count;
}

int SetMapping::countEncoders() const noexcept
{
    int count = 0;

    for (const auto& entry : encoders)
        if (entry.has_value())
            ++count;

    return count;
}

int SetMapping::countBanks() const noexcept
{
    int count = 0;

    for (const auto& entry : banks)
        if (entry.has_value())
            ++count;

    return count;
}

int SetMapping::countScenes() const noexcept
{
    int count = 0;

    for (const auto& entry : scenes)
        if (entry.has_value())
            ++count;

    return count;
}

std::vector<const InstrumentSpec*> SetMapping::allInstruments() const
{
    std::vector<const InstrumentSpec*> result;

    for (const auto& [familyId, family] : families)
        for (const auto& instrument : family.instruments)
            result.push_back (&instrument);

    return result;
}

//==============================================================================
// Minimal YAML reader
//
// `set.yaml` deliberately uses a small dialect so that the engine needs no
// third-party dependency (plan/DECISIONS.md: header-only or vendored). The
// reader below understands exactly what the schema uses: block mappings,
// block sequences, `- key: value` sequence items, plain and quoted scalars,
// comments, and the `---`/`...` document markers. It rejects everything else
// instead of guessing, because a silently half-parsed routing file is worse
// than a reported error that keeps the previous mapping live.
//==============================================================================

namespace
{

enum class YamlKind
{
    nullValue,
    scalar,
    mapping,
    sequence
};

struct YamlValue
{
    YamlKind kind = YamlKind::nullValue;
    std::string scalar;
    std::vector<std::pair<std::string, YamlValue>> mapping;
    std::vector<YamlValue> sequence;
    int line = 0;

    const YamlValue* find (std::string_view key) const noexcept
    {
        for (const auto& [entryKey, value] : mapping)
            if (entryKey == key)
                return &value;

        return nullptr;
    }
};

struct SourceLine
{
    int indent = 0;
    std::string text;
    int number = 0;
};

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

//==============================================================================
// Document validation
//==============================================================================

bool fail (MappingError& error, const YamlValue& value, const std::string& message)
{
    error = { value.line, message };
    return false;
}

bool checkKeys (const YamlValue& value,
                std::initializer_list<std::string_view> allowed,
                MappingError& error)
{
    for (const auto& [key, child] : value.mapping)
    {
        (void) child;

        bool known = false;

        for (const auto entry : allowed)
            if (key == entry)
                known = true;

        if (! known)
            return fail (error, value, "unknown key '" + key + "'");
    }

    return true;
}

bool readString (const YamlValue& value, std::string& out, MappingError& error,
                 const std::string& what, bool allowEmpty = false)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be a string");

    if (! allowEmpty && value.scalar.empty())
        return fail (error, value, what + " must not be empty");

    out = value.scalar;
    return true;
}

bool readInt (const YamlValue& value, int& out, MappingError& error, const std::string& what)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be an integer");

    std::string text = value.scalar;

    if (! text.empty() && text.front() == '+')
        text.erase (text.begin());

    if (text.empty())
        return fail (error, value, what + " must be an integer");

    int parsed = 0;
    const auto result = std::from_chars (text.data(), text.data() + text.size(), parsed);

    if (result.ec != std::errc() || result.ptr != text.data() + text.size())
        return fail (error, value, what + " must be an integer");

    out = parsed;
    return true;
}

bool readFloat (const YamlValue& value, float& out, MappingError& error, const std::string& what)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be a number");

    if (value.scalar.empty())
        return fail (error, value, what + " must be a number");

    char* end = nullptr;
    const float parsed = std::strtof (value.scalar.c_str(), &end);

    if (end == value.scalar.c_str() || *end != '\0' || ! std::isfinite (parsed))
        return fail (error, value, what + " must be a finite number");

    out = parsed;
    return true;
}

bool decodeDocument (const YamlValue& root, SetMapping& out, MappingError& error)
{
    if (root.kind != YamlKind::mapping)
        return fail (error, root, "the document root must be a mapping");

    if (! checkKeys (root, { "schemaVersion", "name", "families", "macros",
                             "pads", "encoders", "banks", "scenes" }, error))
        return false;

    const auto* version = root.find ("schemaVersion");

    if (version == nullptr)
        return fail (error, root, "schemaVersion is required");

    int schemaVersion = 0;

    if (! readInt (*version, schemaVersion, error, "schemaVersion"))
        return false;

    if (schemaVersion != SetMapping::supportedSchemaVersion)
        return fail (error, *version,
                     "unsupported schemaVersion " + std::to_string (schemaVersion)
                         + " (expected " + std::to_string (SetMapping::supportedSchemaVersion) + ")");

    out.schemaVersion = schemaVersion;

    if (const auto* name = root.find ("name"))
        if (! readString (*name, out.name, error, "name"))
            return false;

    // --- families --------------------------------------------------------
    const auto* families = root.find ("families");

    if (families == nullptr)
        return fail (error, root, "families is required");

    if (families->kind != YamlKind::mapping)
        return fail (error, *families, "families must be a mapping of family ids");

    if (families->mapping.empty())
        return fail (error, *families, "families must not be empty");

    for (const auto& [familyId, familyValue] : families->mapping)
    {
        if (familyId.empty())
            return fail (error, familyValue, "a family id must not be empty");

        if (familyValue.kind != YamlKind::mapping)
            return fail (error, familyValue, "family '" + familyId + "' must be a mapping");

        if (! checkKeys (familyValue, { "name", "instruments" }, error))
            return false;

        FamilySpec family;
        family.id = familyId;

        const auto* familyName = familyValue.find ("name");

        if (familyName == nullptr)
            return fail (error, familyValue, "family '" + familyId + "' needs a name");

        if (! readString (*familyName, family.name, error, "family name"))
            return false;

        const auto* instruments = familyValue.find ("instruments");

        if (instruments == nullptr || instruments->kind != YamlKind::sequence
            || instruments->sequence.empty())
            return fail (error, familyValue,
                         "family '" + familyId + "' needs a non-empty instruments list");

        for (const auto& instrumentValue : instruments->sequence)
        {
            if (instrumentValue.kind != YamlKind::mapping)
                return fail (error, instrumentValue,
                             "an instrument in family '" + familyId + "' must be a mapping");

            if (! checkKeys (instrumentValue, { "id", "name", "slot" }, error))
                return false;

            InstrumentSpec instrument;

            const auto* id = instrumentValue.find ("id");
            const auto* name = instrumentValue.find ("name");
            const auto* slot = instrumentValue.find ("slot");

            if (id == nullptr || name == nullptr || slot == nullptr)
                return fail (error, instrumentValue,
                             "an instrument needs id, name and slot");

            if (! readString (*id, instrument.id, error, "instrument id")
                || ! readString (*name, instrument.name, error, "instrument name")
                || ! readInt (*slot, instrument.slot, error, "instrument slot"))
                return false;

            if (instrument.slot < 0 || instrument.slot > SetMapping::maximumSlot)
                return fail (error, *slot,
                             "instrument slot must be within 0.."
                                 + std::to_string (SetMapping::maximumSlot));

            if (family.findInstrument (instrument.id) != nullptr)
                return fail (error, *id,
                             "duplicate instrument id '" + instrument.id
                                 + "' in family '" + familyId + "'");

            family.instruments.push_back (std::move (instrument));
        }

        out.families.emplace (family.id, std::move (family));
    }

    // --- macros ----------------------------------------------------------
    const auto* macros = root.find ("macros");

    if (macros == nullptr)
        return fail (error, root, "macros is required");

    if (macros->kind != YamlKind::mapping)
        return fail (error, *macros, "macros must be a mapping of macro ids");

    for (const auto& [macroId, macroValue] : macros->mapping)
    {
        if (macroId.empty())
            return fail (error, macroValue, "a macro id must not be empty");

        if (macroValue.kind != YamlKind::mapping)
            return fail (error, macroValue, "macro '" + macroId + "' must be a mapping");

        if (! checkKeys (macroValue, { "name", "parameter", "min", "max", "default" }, error))
            return false;

        MacroSpec macro;
        macro.id = macroId;

        const auto* name = macroValue.find ("name");
        const auto* parameter = macroValue.find ("parameter");

        if (name == nullptr || parameter == nullptr)
            return fail (error, macroValue,
                         "macro '" + macroId + "' needs name and parameter");

        if (! readString (*name, macro.name, error, "macro name")
            || ! readString (*parameter, macro.parameter, error, "macro parameter"))
            return false;

        if (const auto* minimum = macroValue.find ("min"))
            if (! readFloat (*minimum, macro.minimum, error, "macro min"))
                return false;

        if (const auto* maximum = macroValue.find ("max"))
            if (! readFloat (*maximum, macro.maximum, error, "macro max"))
                return false;

        if (! (macro.minimum < macro.maximum))
            return fail (error, macroValue,
                         "macro '" + macroId + "' needs min < max");

        macro.defaultValue = macro.minimum;

        if (const auto* defaultValue = macroValue.find ("default"))
        {
            if (! readFloat (*defaultValue, macro.defaultValue, error, "macro default"))
                return false;

            if (macro.defaultValue < macro.minimum || macro.defaultValue > macro.maximum)
                return fail (error, *defaultValue,
                             "macro '" + macroId + "' default is outside its range");
        }

        out.macros.emplace (macro.id, std::move (macro));
    }

    // --- pads ------------------------------------------------------------
    const auto* pads = root.find ("pads");

    if (pads == nullptr)
        return fail (error, root, "pads is required");

    if (pads->kind != YamlKind::sequence)
        return fail (error, *pads, "pads must be a sequence");

    for (const auto& padValue : pads->sequence)
    {
        if (padValue.kind != YamlKind::mapping)
            return fail (error, padValue, "a pad entry must be a mapping");

        if (! checkKeys (padValue, { "pad", "family", "instrument" }, error))
            return false;

        const auto* padNumber = padValue.find ("pad");
        const auto* family = padValue.find ("family");
        const auto* instrument = padValue.find ("instrument");

        if (padNumber == nullptr || family == nullptr || instrument == nullptr)
            return fail (error, padValue, "a pad entry needs pad, family and instrument");

        PadBinding binding;

        if (! readInt (*padNumber, binding.pad, error, "pad")
            || ! readString (*family, binding.family, error, "pad family")
            || ! readString (*instrument, binding.instrument, error, "pad instrument"))
            return false;

        if (binding.pad < 0 || binding.pad >= SetMapping::maximumPads)
            return fail (error, *padNumber,
                         "pad must be within 0.." + std::to_string (SetMapping::maximumPads - 1));

        if (out.padBinding (binding.pad) != nullptr)
            return fail (error, *padNumber,
                         "duplicate pad binding for pad " + std::to_string (binding.pad));

        const auto familyEntry = out.families.find (binding.family);

        if (familyEntry == out.families.end())
            return fail (error, *family,
                         "pad " + std::to_string (binding.pad) + " names unknown family '"
                             + binding.family + "'");

        const auto* instrumentSpec = familyEntry->second.findInstrument (binding.instrument);

        if (instrumentSpec == nullptr)
            return fail (error, *instrument,
                         "pad " + std::to_string (binding.pad) + " names unknown instrument '"
                             + binding.family + "/" + binding.instrument + "'");

        binding.slot = instrumentSpec->slot;

        out.pads[static_cast<std::size_t> (binding.pad)] = std::move (binding);
    }

    // --- encoders --------------------------------------------------------
    const auto* encoders = root.find ("encoders");

    if (encoders == nullptr)
        return fail (error, root, "encoders is required");

    if (encoders->kind != YamlKind::sequence)
        return fail (error, *encoders, "encoders must be a sequence");

    for (const auto& encoderValue : encoders->sequence)
    {
        if (encoderValue.kind != YamlKind::mapping)
            return fail (error, encoderValue, "an encoder entry must be a mapping");

        if (! checkKeys (encoderValue, { "encoder", "macro" }, error))
            return false;

        const auto* encoderNumber = encoderValue.find ("encoder");
        const auto* macro = encoderValue.find ("macro");

        if (encoderNumber == nullptr || macro == nullptr)
            return fail (error, encoderValue, "an encoder entry needs encoder and macro");

        EncoderBinding binding;

        if (! readInt (*encoderNumber, binding.encoder, error, "encoder")
            || ! readString (*macro, binding.macro, error, "encoder macro"))
            return false;

        if (binding.encoder < 0 || binding.encoder >= SetMapping::maximumEncoders)
            return fail (error, *encoderNumber,
                         "encoder must be within 0.."
                             + std::to_string (SetMapping::maximumEncoders - 1));

        if (out.encoderBinding (binding.encoder) != nullptr)
            return fail (error, *encoderNumber,
                         "duplicate encoder binding for encoder "
                             + std::to_string (binding.encoder));

        if (out.macros.find (binding.macro) == out.macros.end())
            return fail (error, *macro,
                         "encoder " + std::to_string (binding.encoder)
                             + " names unknown macro '" + binding.macro + "'");

        out.encoders[static_cast<std::size_t> (binding.encoder)] = std::move (binding);
    }

    // --- macro banks (top row) -------------------------------------------
    if (const auto* banks = root.find ("banks"))
    {
        if (banks->kind != YamlKind::sequence)
            return fail (error, *banks, "banks must be a sequence");

        for (const auto& bankValue : banks->sequence)
        {
            if (bankValue.kind != YamlKind::mapping)
                return fail (error, bankValue, "a bank entry must be a mapping");

            if (! checkKeys (bankValue, { "bank", "name", "macros" }, error))
                return false;

            const auto* bankNumber = bankValue.find ("bank");
            const auto* name = bankValue.find ("name");
            const auto* macroList = bankValue.find ("macros");

            if (bankNumber == nullptr || name == nullptr || macroList == nullptr)
                return fail (error, bankValue, "a bank entry needs bank, name and macros");

            MacroBankSpec bank;

            if (! readInt (*bankNumber, bank.bank, error, "bank")
                || ! readString (*name, bank.name, error, "bank name"))
                return false;

            if (bank.bank < 0 || bank.bank >= SetMapping::maximumBanks)
                return fail (error, *bankNumber,
                             "bank must be within 0.."
                                 + std::to_string (SetMapping::maximumBanks - 1));

            if (out.bankBinding (bank.bank) != nullptr)
                return fail (error, *bankNumber,
                             "duplicate bank binding for bank "
                                 + std::to_string (bank.bank));

            if (macroList->kind != YamlKind::sequence || macroList->sequence.empty())
                return fail (error, *macroList,
                             "bank " + std::to_string (bank.bank)
                                 + " needs a non-empty macros list");

            for (const auto& macroValue : macroList->sequence)
            {
                std::string macroId;

                if (! readString (macroValue, macroId, error, "bank macro"))
                    return false;

                if (out.macros.find (macroId) == out.macros.end())
                    return fail (error, macroValue,
                                 "bank " + std::to_string (bank.bank)
                                     + " names unknown macro '" + macroId + "'");

                bank.macros.push_back (std::move (macroId));
            }

            out.banks[static_cast<std::size_t> (bank.bank)] = std::move (bank);
        }
    }

    // --- scenes (right column) --------------------------------------------
    if (const auto* scenes = root.find ("scenes"))
    {
        if (scenes->kind != YamlKind::sequence)
            return fail (error, *scenes, "scenes must be a sequence");

        for (const auto& sceneValue : scenes->sequence)
        {
            if (sceneValue.kind != YamlKind::mapping)
                return fail (error, sceneValue, "a scene entry must be a mapping");

            if (! checkKeys (sceneValue, { "scene", "name", "family" }, error))
                return false;

            const auto* sceneNumber = sceneValue.find ("scene");
            const auto* name = sceneValue.find ("name");
            const auto* family = sceneValue.find ("family");

            if (sceneNumber == nullptr || name == nullptr || family == nullptr)
                return fail (error, sceneValue, "a scene entry needs scene, name and family");

            SceneSpec scene;

            if (! readInt (*sceneNumber, scene.scene, error, "scene")
                || ! readString (*name, scene.name, error, "scene name")
                || ! readString (*family, scene.family, error, "scene family"))
                return false;

            if (scene.scene < 0 || scene.scene >= SetMapping::maximumScenes)
                return fail (error, *sceneNumber,
                             "scene must be within 0.."
                                 + std::to_string (SetMapping::maximumScenes - 1));

            if (out.sceneBinding (scene.scene) != nullptr)
                return fail (error, *sceneNumber,
                             "duplicate scene binding for scene "
                                 + std::to_string (scene.scene));

            if (out.families.find (scene.family) == out.families.end())
                return fail (error, *family,
                             "scene " + std::to_string (scene.scene)
                                 + " names unknown family '" + scene.family + "'");

            out.scenes[static_cast<std::size_t> (scene.scene)] = std::move (scene);
        }
    }

    return true;
}

//==============================================================================
// File helpers
//==============================================================================

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

bool bindingsEqual (const PadBinding* left, const PadBinding* right) noexcept
{
    if (left == nullptr || right == nullptr)
        return left == right;

    return *left == *right;
}

} // namespace

//==============================================================================
// Public loading
//==============================================================================

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

    if (! decodeDocument (root, parsed, error))
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

    if (! readFileContents (file, contents, readError))
    {
        error = { 0, readError };
        return false;
    }

    return parseSetMappingYaml (contents, result, error);
}

//==============================================================================
// Mapping
//==============================================================================

Mapping::Mapping() = default;

Mapping::~Mapping()
{
    stopWatching();
}

void Mapping::setListeners (Listeners newListeners)
{
    const std::lock_guard<std::mutex> lock (mutex);
    listeners = std::move (newListeners);
}

bool Mapping::load (const std::filesystem::path& file, MappingError* error)
{
    std::error_code ec;
    auto absolute = std::filesystem::absolute (file, ec);

    if (ec)
        absolute = file;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        path = std::move (absolute);
        lastHash = 0;
        hasAttempted = false;
        hasLoaded = false;
        activePad.store (-1, std::memory_order_release);
        activeBank.store (-1, std::memory_order_release);
        activeScene.store (-1, std::memory_order_release);
    }

    return reloadInternal (false, error);
}

bool Mapping::reload (MappingError* error)
{
    const bool hot = [this]
    {
        const std::lock_guard<std::mutex> lock (mutex);
        return hasLoaded;
    }();

    return reloadInternal (hot, error);
}

bool Mapping::reloadInternal (bool hotReload, MappingError* error)
{
    std::filesystem::path currentPath;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        currentPath = path;
    }

    if (currentPath.empty())
    {
        const MappingError failure { 0, "no set file has been loaded" };
        reportError (failure, 0, false);

        if (error != nullptr)
            *error = failure;

        return false;
    }

    std::string contents;
    std::string readError;

    if (! readFileContents (currentPath, contents, readError))
    {
        const MappingError failure { 0, readError };
        reportError (failure, 0, false);

        if (error != nullptr)
            *error = failure;

        return false;
    }

    return applyText (contents, hotReload, hashContents (contents), true, error);
}

bool Mapping::poll()
{
    std::filesystem::path currentPath;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        currentPath = path;
    }

    if (currentPath.empty())
        return false;

    std::string contents;
    std::string readError;

    if (! readFileContents (currentPath, contents, readError))
    {
        const MappingError failure { 0, readError };
        reportError (failure, 0, false);
        return false;
    }

    const auto hash = hashContents (contents);
    bool hot = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (hasAttempted && hash == lastHash)
            return false;

        hot = hasLoaded;
    }

    return applyText (contents, hot, hash, false, nullptr);
}

bool Mapping::applyText (const std::string& text,
                         bool hotReload,
                         std::uint64_t contentHash,
                         bool force,
                         MappingError* error)
{
    SetMapping parsed;
    MappingError parseError;

    if (! parseSetMappingYaml (text, parsed, parseError))
    {
        reportError (parseError, contentHash, true);

        if (error != nullptr)
            *error = parseError;

        return false;
    }

    auto newMapping = std::make_shared<SetMapping> (std::move (parsed));

    Listeners listenersCopy;
    std::shared_ptr<const SetMapping> oldMapping;
    bool routeChanged = false;
    int armTarget = -1;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (! force && hasAttempted && contentHash == lastHash)
        {
            if (error != nullptr)
                error->message.clear();

            return false;
        }

        const int active = activePad.load (std::memory_order_acquire);
        oldMapping = current;

        const PadBinding* oldBinding = (oldMapping != nullptr && active >= 0)
                                           ? oldMapping->padBinding (active)
                                           : nullptr;
        const PadBinding* newBinding = active >= 0 ? newMapping->padBinding (active) : nullptr;

        routeChanged = ! bindingsEqual (oldBinding, newBinding);

        if (newBinding != nullptr)
            armTarget = newBinding->slot;

        if (newMapping->bankBinding (activeBank.load (std::memory_order_relaxed)) == nullptr)
            activeBank.store (-1, std::memory_order_release);

        if (newMapping->sceneBinding (activeScene.load (std::memory_order_relaxed)) == nullptr)
            activeScene.store (-1, std::memory_order_release);

        current = newMapping;
        lastError = {};
        lastHash = contentHash;
        hasAttempted = true;
        hasLoaded = true;
        revision.fetch_add (1, std::memory_order_release);
        listenersCopy = listeners;
    }

    if (error != nullptr)
        error->message.clear();

    if (listenersCopy.changed)
        listenersCopy.changed (newMapping, hotReload);

    // Re-arming happens after the snapshot is published, so a listener that
    // reads getMapping() sees the document it is being notified about.
    if (routeChanged && listenersCopy.armSlot)
        listenersCopy.armSlot (armTarget);

    return true;
}

void Mapping::reportError (const MappingError& error, std::uint64_t contentHash, bool rememberHash)
{
    Listeners listenersCopy;
    bool shouldNotify = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        shouldNotify = lastError.message != error.message || lastError.line != error.line;
        lastError = error;

        if (rememberHash)
        {
            lastHash = contentHash;
            hasAttempted = true;
        }

        listenersCopy = listeners;
    }

    if (shouldNotify && listenersCopy.error)
        listenersCopy.error (error);
}

void Mapping::watchLoop (int intervalMs)
{
    while (! quitWatcher.load (std::memory_order_acquire))
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (intervalMs));

        if (quitWatcher.load (std::memory_order_acquire))
            break;

        poll();
    }

    watching.store (false, std::memory_order_release);
}

bool Mapping::startWatching (int intervalMs)
{
    if (intervalMs < 5 || intervalMs > 5000)
        return false;

    const std::lock_guard<std::mutex> lock (watchMutex);

    if (watcher.joinable())
        return true;

    quitWatcher.store (false, std::memory_order_release);
    watching.store (true, std::memory_order_release);

    try
    {
        watcher = std::thread ([this, intervalMs] { watchLoop (intervalMs); });
    }
    catch (const std::system_error&)
    {
        watching.store (false, std::memory_order_release);
        return false;
    }

    return true;
}

void Mapping::stopWatching()
{
    quitWatcher.store (true, std::memory_order_release);

    const std::lock_guard<std::mutex> lock (watchMutex);

    if (! watcher.joinable())
    {
        watching.store (false, std::memory_order_release);
        return;
    }

    // A listener invoked by the watcher thread must not join itself; the
    // watcher exits its loop after this callback and a later stop/join cleans
    // the thread up.
    if (watcher.get_id() == std::this_thread::get_id())
        return;

    watcher.join();
    watching.store (false, std::memory_order_release);
}

bool Mapping::isWatching() const noexcept
{
    return watching.load (std::memory_order_acquire);
}

std::filesystem::path Mapping::getPath() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return path;
}

std::shared_ptr<const SetMapping> Mapping::getMapping() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return current;
}

std::uint64_t Mapping::getRevision() const noexcept
{
    return revision.load (std::memory_order_acquire);
}

MappingError Mapping::getLastError() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return lastError;
}

std::optional<PadBinding> Mapping::getPadBinding (int pad) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (current == nullptr)
        return std::nullopt;

    if (const auto* binding = current->padBinding (pad))
        return *binding;

    return std::nullopt;
}

std::optional<EncoderBinding> Mapping::getEncoderBinding (int encoder) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (current == nullptr)
        return std::nullopt;

    if (const auto* binding = current->encoderBinding (encoder))
        return *binding;

    return std::nullopt;
}

std::optional<MacroBankSpec> Mapping::getBank (int bank) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (current == nullptr)
        return std::nullopt;

    if (const auto* page = current->bankBinding (bank))
        return *page;

    return std::nullopt;
}

std::optional<SceneSpec> Mapping::getScene (int scene) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (current == nullptr)
        return std::nullopt;

    if (const auto* selection = current->sceneBinding (scene))
        return *selection;

    return std::nullopt;
}

int Mapping::getActivePad() const noexcept
{
    return activePad.load (std::memory_order_acquire);
}

int Mapping::getActiveBank() const noexcept
{
    return activeBank.load (std::memory_order_acquire);
}

int Mapping::getActiveScene() const noexcept
{
    return activeScene.load (std::memory_order_acquire);
}

int Mapping::getArmedSlot() const noexcept
{
    const std::lock_guard<std::mutex> lock (mutex);
    const int pad = activePad.load (std::memory_order_relaxed);

    if (current != nullptr && pad >= 0)
        if (const auto* binding = current->padBinding (pad))
            return binding->slot;

    return -1;
}

bool Mapping::handlePad (int pad, bool pressed)
{
    if (pad < 0 || pad >= SetMapping::maximumPads)
        return false;

    Listeners listenersCopy;
    std::optional<PadBinding> binding;
    int armedSlot = -1;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        listenersCopy = listeners;

        if (current != nullptr)
            if (const auto* currentBinding = current->padBinding (pad))
                binding = *currentBinding;

        if (pressed && binding.has_value())
        {
            activePad.store (pad, std::memory_order_release);
            armedSlot = binding->slot;
        }
    }

    if (pressed && binding.has_value() && listenersCopy.armSlot)
        listenersCopy.armSlot (armedSlot);

    if (listenersCopy.padEvent)
        listenersCopy.padEvent (pad, binding.has_value() ? &*binding : nullptr, pressed);

    return pressed && binding.has_value();
}

bool Mapping::handleEncoder (int encoder, int value)
{
    if (encoder < 0 || encoder >= SetMapping::maximumEncoders)
        return false;

    Listeners listenersCopy;
    MacroChange change;
    bool mapped = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        listenersCopy = listeners;

        if (current != nullptr)
        {
            if (const auto* binding = current->encoderBinding (encoder))
            {
                if (const auto* macro = current->macroFor (*binding))
                {
                    change.macro = *macro;
                    mapped = true;

                    const int pad = activePad.load (std::memory_order_relaxed);

                    if (pad >= 0)
                        if (const auto* padBinding = current->padBinding (pad))
                            change.slot = padBinding->slot;
                }
            }
        }
    }

    if (! mapped)
        return false;

    change.rawValue = std::clamp (value, 0, 127);
    change.normalized = static_cast<float> (change.rawValue) / 127.0f;
    change.value = change.macro.minimum
                   + change.normalized * (change.macro.maximum - change.macro.minimum);

    if (listenersCopy.macroEvent)
        listenersCopy.macroEvent (change);

    return true;
}

bool Mapping::handleButton (int cc, bool pressed)
{
    if (const auto topRow = Launchpad::topRowIndexForCC (cc))
        return handleTopRow (*topRow, pressed);

    if (const auto rightColumn = Launchpad::rightColumnIndexForCC (cc))
        return handleRightColumn (*rightColumn, pressed);

    return false;
}

bool Mapping::handleTopRow (int index, bool pressed)
{
    if (index < 0 || index >= SetMapping::maximumBanks)
        return false;

    Listeners listenersCopy;
    std::optional<MacroBankSpec> page;
    bool mapped = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        listenersCopy = listeners;

        if (current != nullptr)
        {
            if (const auto* binding = current->bankBinding (index))
            {
                page = *binding;
                mapped = true;

                if (pressed)
                    activeBank.store (index, std::memory_order_release);
            }
        }
    }

    if (! pressed || ! mapped)
        return false;

    if (listenersCopy.bankEvent)
        listenersCopy.bankEvent (index, &*page);

    return true;
}

bool Mapping::handleRightColumn (int index, bool pressed)
{
    if (index < 0 || index >= SetMapping::maximumScenes)
        return false;

    Listeners listenersCopy;
    std::optional<SceneSpec> selection;
    bool mapped = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        listenersCopy = listeners;

        if (current != nullptr)
        {
            if (const auto* binding = current->sceneBinding (index))
            {
                selection = *binding;
                mapped = true;

                if (pressed)
                    activeScene.store (index, std::memory_order_release);
            }
        }
    }

    if (! pressed || ! mapped)
        return false;

    if (listenersCopy.sceneEvent)
        listenersCopy.sceneEvent (index, &*selection);

    return true;
}

std::filesystem::path Mapping::findDefaultSetFile()
{
    if (const char* environment = std::getenv ("INSTRUMENT_HUB_SET"))
    {
        const std::filesystem::path fromEnvironment (environment);

        if (! fromEnvironment.empty())
            return fromEnvironment;
    }

    std::error_code ec;
    const auto directory = std::filesystem::current_path (ec);

    if (ec)
        return {};

    const std::filesystem::path candidates[] {
        directory / "set.yaml",
        directory / "shared" / "set.yaml"
    };

    for (const auto& candidate : candidates)
        if (std::filesystem::exists (candidate, ec))
            return candidate;

    return {};
}

} // namespace Controller
