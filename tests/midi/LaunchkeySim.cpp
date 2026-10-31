// Virtual Launchkey Mini MK3 on the ALSA sequencer.
//
// See LaunchkeySim.h. This file deliberately depends only on ALSA and the C++
// standard library so it can be linked into `ihub-selftest`, into the standalone
// `ihub-launchkey-sim` binary, and (later) into the driver integration test.

#include "LaunchkeySim.h"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

namespace LaunchkeySim
{
namespace
{
//==============================================================================
// ALSA helpers
//==============================================================================

std::string alsaMessage (const std::string& what, int error)
{
    return what + ": " + snd_strerror (error);
}

void checkAlsa (int error, const std::string& what)
{
    if (error < 0)
        throw std::runtime_error (alsaMessage (what, error));
}

std::chrono::steady_clock::time_point now()
{
    return std::chrono::steady_clock::now();
}

//==============================================================================
// Minimal JSON reader
//==============================================================================

struct JsonValue
{
    enum class Kind { Null, Bool, Number, String, Array, Object };

    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;

    const JsonValue* find (std::string_view key) const
    {
        for (const auto& entry : object)
            if (entry.first == key)
                return &entry.second;

        return nullptr;
    }
};

class JsonParser
{
public:
    explicit JsonParser (std::string_view text) : text_ (text) {}

    JsonValue parseDocument()
    {
        skipWhitespace();
        auto value = parseValue();
        skipWhitespace();

        if (position_ != text_.size())
            fail ("trailing characters after the JSON document");

        return value;
    }

private:
    [[noreturn]] void fail (const std::string& message) const
    {
        throw std::runtime_error ("JSON: " + message + " at byte "
                                  + std::to_string (position_));
    }

    char peek() const
    {
        return position_ < text_.size() ? text_[position_] : '\0';
    }

    char take()
    {
        if (position_ >= text_.size())
            fail ("unexpected end of input");

        return text_[position_++];
    }

    bool consume (char expected)
    {
        if (peek() != expected)
            return false;

        ++position_;
        return true;
    }

    void skipWhitespace()
    {
        while (position_ < text_.size())
        {
            const char c = text_[position_];

            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                ++position_;
            else
                break;
        }
    }

    JsonValue parseValue()
    {
        skipWhitespace();

        switch (peek())
        {
            case '{': return parseObject();
            case '[': return parseArray();
            case '"':
            {
                JsonValue value;
                value.kind = JsonValue::Kind::String;
                value.string = parseString();
                return value;
            }
            case 't': expectLiteral ("true"); return makeBool (true);
            case 'f': expectLiteral ("false"); return makeBool (false);
            case 'n': expectLiteral ("null"); return JsonValue {};
            default: return parseNumber();
        }
    }

    static JsonValue makeBool (bool value)
    {
        JsonValue result;
        result.kind = JsonValue::Kind::Bool;
        result.boolean = value;
        return result;
    }

    void expectLiteral (std::string_view literal)
    {
        if (text_.substr (position_, literal.size()) != literal)
            fail ("expected '" + std::string (literal) + "'");

        position_ += literal.size();
    }

    JsonValue parseObject()
    {
        JsonValue result;
        result.kind = JsonValue::Kind::Object;
        take(); // {
        skipWhitespace();

        if (consume ('}'))
            return result;

        while (true)
        {
            skipWhitespace();

            if (peek() != '"')
                fail ("expected a quoted object key");

            auto key = parseString();
            skipWhitespace();

            if (! consume (':'))
                fail ("expected ':' after an object key");

            result.object.emplace_back (std::move (key), parseValue());
            skipWhitespace();

            if (consume ('}'))
                return result;

            if (! consume (','))
                fail ("expected ',' or '}' in an object");
        }
    }

    JsonValue parseArray()
    {
        JsonValue result;
        result.kind = JsonValue::Kind::Array;
        take(); // [
        skipWhitespace();

        if (consume (']'))
            return result;

        while (true)
        {
            result.array.push_back (parseValue());
            skipWhitespace();

            if (consume (']'))
                return result;

            if (! consume (','))
                fail ("expected ',' or ']' in an array");
        }
    }

    std::string parseString()
    {
        take(); // opening quote
        std::string result;

        while (true)
        {
            const char c = take();

            if (c == '"')
                return result;

            if (static_cast<unsigned char> (c) < 0x20)
                fail ("control character in a string");

            if (c != '\\')
            {
                result.push_back (c);
                continue;
            }

            switch (take())
            {
                case '"':  result.push_back ('"');  break;
                case '\\': result.push_back ('\\'); break;
                case '/':  result.push_back ('/');  break;
                case 'b':  result.push_back ('\b'); break;
                case 'f':  result.push_back ('\f'); break;
                case 'n':  result.push_back ('\n'); break;
                case 'r':  result.push_back ('\r'); break;
                case 't':  result.push_back ('\t'); break;
                case 'u':  appendCodePoint (result, parseHex4()); break;
                default:   fail ("unsupported escape sequence");
            }
        }
    }

    unsigned int parseHex4()
    {
        unsigned int value = 0;

        for (int i = 0; i < 4; ++i)
        {
            const char c = take();
            value <<= 4;

            if (c >= '0' && c <= '9')
                value |= static_cast<unsigned int> (c - '0');
            else if (c >= 'a' && c <= 'f')
                value |= static_cast<unsigned int> (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                value |= static_cast<unsigned int> (c - 'A' + 10);
            else
                fail ("invalid \\u escape");
        }

        return value;
    }

    static void appendCodePoint (std::string& out, unsigned int codePoint)
    {
        if (codePoint <= 0x7F)
        {
            out.push_back (static_cast<char> (codePoint));
        }
        else if (codePoint <= 0x7FF)
        {
            out.push_back (static_cast<char> (0xC0 | (codePoint >> 6)));
            out.push_back (static_cast<char> (0x80 | (codePoint & 0x3F)));
        }
        else
        {
            out.push_back (static_cast<char> (0xE0 | (codePoint >> 12)));
            out.push_back (static_cast<char> (0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back (static_cast<char> (0x80 | (codePoint & 0x3F)));
        }
    }

    JsonValue parseNumber()
    {
        const std::size_t start = position_;

        if (peek() == '-')
            ++position_;

        while (std::isdigit (static_cast<unsigned char> (peek())))
            ++position_;

        if (peek() == '.')
        {
            ++position_;

            while (std::isdigit (static_cast<unsigned char> (peek())))
                ++position_;
        }

        if (peek() == 'e' || peek() == 'E')
        {
            ++position_;

            if (peek() == '+' || peek() == '-')
                ++position_;

            while (std::isdigit (static_cast<unsigned char> (peek())))
                ++position_;
        }

        if (position_ == start)
            fail ("expected a JSON value");

        const std::string token (text_.substr (start, position_ - start));
        char* end = nullptr;
        const double number = std::strtod (token.c_str(), &end);

        if (end == token.c_str() || *end != '\0')
            fail ("invalid number");

        JsonValue result;
        result.kind = JsonValue::Kind::Number;
        result.number = number;
        return result;
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

const JsonValue* field (const JsonValue& object, std::initializer_list<const char*> names)
{
    for (const auto* name : names)
        if (const auto* value = object.find (name))
            return value;

    return nullptr;
}

long long integerField (const JsonValue& object,
                        std::initializer_list<const char*> names,
                        long long fallback)
{
    const auto* value = field (object, names);

    if (value == nullptr)
        return fallback;

    if (value->kind != JsonValue::Kind::Number)
        throw std::runtime_error ("fixture: a numeric field has a non-numeric value");

    return static_cast<long long> (std::llround (value->number));
}

std::string stringField (const JsonValue& object,
                         std::initializer_list<const char*> names)
{
    const auto* value = field (object, names);

    if (value == nullptr)
        return {};

    if (value->kind != JsonValue::Kind::String)
        throw std::runtime_error ("fixture: a string field has a non-string value");

    return value->string;
}

std::string normaliseType (std::string type)
{
    std::string result;

    for (const char c : type)
        if (std::isalnum (static_cast<unsigned char> (c)))
            result.push_back (static_cast<char> (std::tolower (static_cast<unsigned char> (c))));

    return result;
}

std::vector<std::uint8_t> parseHex (const std::string& text)
{
    std::vector<std::uint8_t> bytes;
    std::string digits;
    digits.reserve (text.size());

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];

        if (c == '0' && i + 1 < text.size()
            && (text[i + 1] == 'x' || text[i + 1] == 'X'))
        {
            ++i; // skip the 0x prefix
            continue;
        }

        if (std::isxdigit (static_cast<unsigned char> (c)))
        {
            digits.push_back (c);
            continue;
        }

        if (std::isspace (static_cast<unsigned char> (c)) || c == ',')
            continue;

        throw std::runtime_error (std::string ("fixture: invalid hex character '") + c + "'");
    }

    if (digits.size() % 2 != 0)
        throw std::runtime_error ("fixture: SysEx hex string has an odd number of digits");

    for (std::size_t i = 0; i < digits.size(); i += 2)
    {
        const std::string byteText = digits.substr (i, 2);
        bytes.push_back (static_cast<std::uint8_t> (std::stoul (byteText, nullptr, 16)));
    }

    return bytes;
}

std::vector<std::uint8_t> parseByteArray (const JsonValue& value)
{
    std::vector<std::uint8_t> bytes;

    if (value.kind == JsonValue::Kind::String)
        return parseHex (value.string);

    if (value.kind != JsonValue::Kind::Array)
        throw std::runtime_error ("fixture: SysEx bytes must be a hex string or an array");

    for (const auto& entry : value.array)
    {
        if (entry.kind != JsonValue::Kind::Number)
            throw std::runtime_error ("fixture: SysEx byte array contains a non-number");

        const auto byte = static_cast<long long> (std::llround (entry.number));

        if (byte < 0 || byte > 255)
            throw std::runtime_error ("fixture: SysEx byte outside 0..255");

        bytes.push_back (static_cast<std::uint8_t> (byte));
    }

    return bytes;
}

void requireRange (const std::string& what, long long value, long long low, long long high)
{
    if (value < low || value > high)
        throw std::runtime_error ("fixture: " + what + " " + std::to_string (value)
                                  + " outside " + std::to_string (low) + ".."
                                  + std::to_string (high));
}

Event parseEvent (const JsonValue& object, std::size_t index)
{
    if (object.kind != JsonValue::Kind::Object)
        throw std::runtime_error ("fixture: event " + std::to_string (index) + " is not an object");

    Event event;
    event.t = integerField (object, { "t", "time", "timeMs", "ms" }, 0);
    event.type = normaliseType (stringField (object, { "type", "kind" }));

    if (event.type.empty())
        throw std::runtime_error ("fixture: event " + std::to_string (index) + " has no type");

    event.channel = static_cast<int> (integerField (object, { "ch", "channel" }, 1));

    if (event.type == "sysex")
    {
        const auto* bytes = field (object, { "hex", "bytes", "data" });

        if (bytes == nullptr)
            throw std::runtime_error ("fixture: SysEx event " + std::to_string (index)
                                      + " has no hex/bytes field");

        event.bytes = parseByteArray (*bytes);

        if (event.bytes.size() < 2 || event.bytes.front() != 0xF0
            || event.bytes.back() != 0xF7)
            throw std::runtime_error ("fixture: SysEx event " + std::to_string (index)
                                      + " must start with F0 and end with F7");

        return event;
    }

    event.note = static_cast<int> (integerField (object, { "note", "key" }, 0));
    event.velocity = static_cast<int> (integerField (object, { "vel", "velocity" },
                                                    event.type == "noteon" ? 100 : 0));
    event.controller = static_cast<int> (integerField (object,
                                                       { "cc", "controller", "control" },
                                                       -1));
    event.value = static_cast<int> (integerField (object, { "value", "val" }, 0));

    requireRange ("channel", event.channel, 1, 16);

    if (event.type == "noteon" || event.type == "noteoff")
    {
        requireRange ("note", event.note, 0, 127);
        requireRange ("velocity", event.velocity, 0, 127);
    }
    else if (event.type == "cc" || event.type == "controller")
    {
        event.type = "cc";

        if (event.controller < 0)
            throw std::runtime_error ("fixture: CC event " + std::to_string (index)
                                      + " has no cc/controller field");

        requireRange ("controller", event.controller, 0, 127);
        requireRange ("value", event.value, 0, 127);
    }
    else if (event.type == "programchange" || event.type == "program")
    {
        event.type = "programchange";
        requireRange ("program", event.value, 0, 127);
    }
    else if (event.type == "pitchbend" || event.type == "pitch")
    {
        event.type = "pitchbend";
        requireRange ("pitch bend", event.value, 0, 16383);
    }
    else if (event.type == "clock" || event.type == "start" || event.type == "stop"
             || event.type == "continue" || event.type == "sensing"
             || event.type == "reset" || event.type == "tunerequest")
    {
        // Realtime messages carry no channel or data.
    }
    else
    {
        throw std::runtime_error ("fixture: event " + std::to_string (index)
                                  + " has unknown type '" + event.type + "'");
    }

    return event;
}

//==============================================================================
// Raw message conversion
//==============================================================================

std::string hexString (const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill ('0');

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (i != 0)
            out << ' ';

        out << std::setw (2) << static_cast<unsigned int> (bytes[i]);
    }

    return out.str();
}

bool recordEvent (const snd_seq_event_t& event,
                  RecordedMessage& message)
{
    switch (event.type)
    {
        case SND_SEQ_EVENT_NOTEON:
        case SND_SEQ_EVENT_NOTEOFF:
        {
            const bool on = event.type == SND_SEQ_EVENT_NOTEON;
            const auto channel = static_cast<std::uint8_t> (event.data.note.channel & 0x0F);
            message.type = on ? "noteon" : "noteoff";
            message.channel = channel + 1;
            message.data1 = event.data.note.note;
            message.data2 = event.data.note.velocity;
            message.bytes = { static_cast<std::uint8_t> ((on ? 0x90 : 0x80) | channel),
                              static_cast<std::uint8_t> (message.data1),
                              static_cast<std::uint8_t> (message.data2) };
            return true;
        }

        case SND_SEQ_EVENT_CONTROLLER:
        {
            const auto channel = static_cast<std::uint8_t> (event.data.control.channel & 0x0F);
            message.type = "cc";
            message.channel = channel + 1;
            message.data1 = event.data.control.param;
            message.data2 = event.data.control.value;
            message.bytes = { static_cast<std::uint8_t> (0xB0 | channel),
                              static_cast<std::uint8_t> (message.data1),
                              static_cast<std::uint8_t> (message.data2) };
            return true;
        }

        case SND_SEQ_EVENT_PGMCHANGE:
        {
            const auto channel = static_cast<std::uint8_t> (event.data.control.channel & 0x0F);
            message.type = "programchange";
            message.channel = channel + 1;
            message.data1 = event.data.control.value;
            message.bytes = { static_cast<std::uint8_t> (0xC0 | channel),
                              static_cast<std::uint8_t> (message.data1) };
            return true;
        }

        case SND_SEQ_EVENT_PITCHBEND:
        {
            const auto channel = static_cast<std::uint8_t> (event.data.control.channel & 0x0F);
            const int bend = event.data.control.value + 8192;
            message.type = "pitchbend";
            message.channel = channel + 1;
            message.data1 = bend & 0x7F;
            message.data2 = (bend >> 7) & 0x7F;
            message.bytes = { static_cast<std::uint8_t> (0xE0 | channel),
                              static_cast<std::uint8_t> (message.data1),
                              static_cast<std::uint8_t> (message.data2) };
            return true;
        }

        case SND_SEQ_EVENT_SYSEX:
        {
            const auto* data = static_cast<const std::uint8_t*> (event.data.ext.ptr);
            message.type = "sysex";
            message.bytes.assign (data, data + event.data.ext.len);
            return true;
        }

        case SND_SEQ_EVENT_CLOCK:
            message.type = "clock";
            message.bytes = { 0xF8 };
            return true;

        case SND_SEQ_EVENT_START:
            message.type = "start";
            message.bytes = { 0xFA };
            return true;

        case SND_SEQ_EVENT_CONTINUE:
            message.type = "continue";
            message.bytes = { 0xFB };
            return true;

        case SND_SEQ_EVENT_STOP:
            message.type = "stop";
            message.bytes = { 0xFC };
            return true;

        case SND_SEQ_EVENT_SENSING:
            message.type = "sensing";
            message.bytes = { 0xFE };
            return true;

        case SND_SEQ_EVENT_RESET:
            message.type = "reset";
            message.bytes = { 0xFF };
            return true;

        case SND_SEQ_EVENT_TUNE_REQUEST:
            message.type = "tunerequest";
            message.bytes = { 0xF6 };
            return true;

        default:
            return false;
    }
}

//==============================================================================
// Fixture replay
//==============================================================================

void setRealtime (snd_seq_event_t& message, int type)
{
    message.type = static_cast<unsigned char> (type);
    snd_seq_ev_set_fixed (&message);
}

void sendEvent (snd_seq_t* seq, int port, const Event& event, bool& warnedNoSubscriber)
{
    snd_seq_event_t message;
    snd_seq_ev_clear (&message);
    snd_seq_ev_set_source (&message, port);
    snd_seq_ev_set_subs (&message);
    snd_seq_ev_set_direct (&message);

    const int channel = event.channel - 1;

    if (event.type == "noteon")
        snd_seq_ev_set_noteon (&message, channel, event.note, event.velocity);
    else if (event.type == "noteoff")
        snd_seq_ev_set_noteoff (&message, channel, event.note, event.velocity);
    else if (event.type == "cc")
        snd_seq_ev_set_controller (&message, channel, event.controller, event.value);
    else if (event.type == "programchange")
        snd_seq_ev_set_pgmchange (&message, channel, event.value);
    else if (event.type == "pitchbend")
        snd_seq_ev_set_pitchbend (&message, channel, event.value - 8192);
    else if (event.type == "sysex")
        snd_seq_ev_set_sysex (&message, static_cast<int> (event.bytes.size()),
                              const_cast<std::uint8_t*> (event.bytes.data()));
    else if (event.type == "clock")
        setRealtime (message, SND_SEQ_EVENT_CLOCK);
    else if (event.type == "start")
        setRealtime (message, SND_SEQ_EVENT_START);
    else if (event.type == "stop")
        setRealtime (message, SND_SEQ_EVENT_STOP);
    else if (event.type == "continue")
        setRealtime (message, SND_SEQ_EVENT_CONTINUE);
    else if (event.type == "sensing")
        setRealtime (message, SND_SEQ_EVENT_SENSING);
    else if (event.type == "reset")
        setRealtime (message, SND_SEQ_EVENT_RESET);
    else if (event.type == "tunerequest")
        setRealtime (message, SND_SEQ_EVENT_TUNE_REQUEST);
    else
        throw std::runtime_error ("simulator: cannot send unknown event type '" + event.type + "'");

    const int error = snd_seq_event_output_direct (seq, &message);

    // A fixture may be replayed before the application subscribes. The kernel
    // reports ENOSPC (no subscriber) or ENOENT (no destination) in that case;
    // neither is a simulator bug. Anything else is.
    const bool noSubscriber = error == -ENOSPC || error == -ENOENT;

    if (error < 0 && ! noSubscriber)
        throw std::runtime_error (alsaMessage ("simulator: cannot send " + event.type, error));

    if (noSubscriber && ! warnedNoSubscriber)
    {
        std::cerr << "sim: no subscriber; " << event.type
                  << " (and further events) will be dropped until the application connects"
                  << std::endl;
        warnedNoSubscriber = true;
    }
}
} // namespace

//==============================================================================
// Fixture loading
//==============================================================================

Fixture loadFixture (const std::string& path)
{
    std::ifstream file (path, std::ios::binary);

    if (! file)
        throw std::runtime_error ("cannot open fixture " + path);

    std::ostringstream contents;
    contents << file.rdbuf();

    if (! file.good() && ! file.eof())
        throw std::runtime_error ("cannot read fixture " + path);

    const JsonValue document = JsonParser (contents.str()).parseDocument();

    if (document.kind != JsonValue::Kind::Object)
        throw std::runtime_error ("fixture: top level must be an object");

    const auto* events = document.find ("events");

    if (events == nullptr || events->kind != JsonValue::Kind::Array)
        throw std::runtime_error ("fixture: missing top-level 'events' array");

    Fixture fixture;
    fixture.events.reserve (events->array.size());

    for (std::size_t i = 0; i < events->array.size(); ++i)
        fixture.events.push_back (parseEvent (events->array[i], i));

    std::stable_sort (fixture.events.begin(), fixture.events.end(),
                      [] (const Event& a, const Event& b) { return a.t < b.t; });

    return fixture;
}

//==============================================================================
// Recording
//==============================================================================

std::string RecordedMessage::toJson() const
{
    std::ostringstream out;
    out << "{\"t\":" << t << ",\"type\":\"" << type << "\"";

    if (channel > 0)
        out << ",\"ch\":" << channel;

    if (type == "noteon" || type == "noteoff")
        out << ",\"note\":" << data1 << ",\"vel\":" << data2;
    else if (type == "cc")
        out << ",\"cc\":" << data1 << ",\"value\":" << data2;
    else if (type == "programchange")
        out << ",\"program\":" << data1;
    else if (type == "pitchbend")
        out << ",\"value\":" << ((data2 << 7) | data1);

    out << ",\"bytes\":\"" << hexString (bytes) << "\"}";
    return out.str();
}

void writeRecording (const std::string& path,
                     const std::vector<RecordedMessage>& messages)
{
    std::ofstream file (path, std::ios::binary | std::ios::trunc);

    if (! file)
        throw std::runtime_error ("cannot open recording " + path + " for writing");

    for (const auto& message : messages)
        file << message.toJson() << '\n';

    if (! file)
        throw std::runtime_error ("failed while writing recording " + path);
}

//==============================================================================
// Port discovery
//==============================================================================

std::string PortInfo::id() const
{
    return std::to_string (client) + ":" + std::to_string (port);
}

std::string PortInfo::describe() const
{
    std::string caps;

    if (source)
        caps += "source";

    if (destination)
        caps += caps.empty() ? "destination" : ",destination";

    if (caps.empty())
        caps = "no-midi";

    return id() + " '" + clientName + "' / '" + portName + "' [" + caps + "]";
}

std::vector<PortInfo> enumeratePorts()
{
    snd_seq_t* seq = nullptr;
    checkAlsa (snd_seq_open (&seq, "default", SND_SEQ_OPEN_INPUT, 0),
               "cannot open ALSA sequencer for enumeration");

    struct Closer
    {
        snd_seq_t* handle;
        ~Closer() { snd_seq_close (handle); }
    } closer { seq };

    checkAlsa (snd_seq_set_client_name (seq, "ihub-midi-enumerate"),
               "cannot name the enumeration client");

    std::vector<PortInfo> ports;

    snd_seq_client_info_t* clientInfo = nullptr;
    snd_seq_client_info_alloca (&clientInfo);
    snd_seq_client_info_set_client (clientInfo, -1);

    while (snd_seq_query_next_client (seq, clientInfo) >= 0)
    {
        const int clientId = snd_seq_client_info_get_client (clientInfo);
        const char* clientName = snd_seq_client_info_get_name (clientInfo);

        snd_seq_port_info_t* portInfo = nullptr;
        snd_seq_port_info_alloca (&portInfo);
        snd_seq_port_info_set_client (portInfo, clientId);
        snd_seq_port_info_set_port (portInfo, -1);

        while (snd_seq_query_next_port (seq, portInfo) >= 0)
        {
            PortInfo info;
            info.client = clientId;
            info.port = snd_seq_port_info_get_port (portInfo);
            info.clientName = clientName != nullptr ? clientName : "";
            const char* portName = snd_seq_port_info_get_name (portInfo);
            info.portName = portName != nullptr ? portName : "";
            info.capabilities = snd_seq_port_info_get_capability (portInfo);
            info.source = (info.capabilities & SND_SEQ_PORT_CAP_READ) != 0;
            info.destination = (info.capabilities & SND_SEQ_PORT_CAP_WRITE) != 0;
            ports.push_back (std::move (info));
        }
    }

    return ports;
}

bool isSimPort (const PortInfo& port)
{
    return port.clientName == kPortName || port.portName == kPortName;
}

//==============================================================================
// Simulator
//==============================================================================

struct Simulator::Impl
{
    snd_seq_t* seq = nullptr;
    int port = -1;
    int client = -1;
    std::vector<RecordedMessage> recorded;
    std::chrono::steady_clock::time_point origin;
    bool warnedNoSubscriber = false;

    ~Impl()
    {
        if (seq != nullptr)
            snd_seq_close (seq);
    }

    std::int64_t elapsedMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds> (now() - origin).count();
    }

    void open()
    {
        checkAlsa (snd_seq_open (&seq, "default", SND_SEQ_OPEN_DUPLEX, 0),
                   "cannot open the ALSA sequencer");

        checkAlsa (snd_seq_set_client_name (seq, kClientName),
                   "cannot set the simulator client name");

        checkAlsa (snd_seq_nonblock (seq, 1),
                   "cannot put the simulator sequencer into non-blocking mode");

        port = snd_seq_create_simple_port (
            seq, kPortName,
            SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_DUPLEX
                | SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

        if (port < 0)
            throw std::runtime_error (alsaMessage ("cannot create the '" + std::string (kPortName)
                                                      + "' port",
                                                  port));

        client = snd_seq_client_id (seq);

        if (client < 0)
            throw std::runtime_error (alsaMessage ("cannot read the simulator client id", client));

        // One second of dense MIDI at 30 messages/s is plenty; this only needs
        // to survive a fixture replay while the probe is not reading.
        snd_seq_set_input_buffer_size (seq, 16384);
        origin = now();
    }
};

Simulator::Simulator() : impl_ (std::make_unique<Impl>()) {}

Simulator::~Simulator() = default;

Simulator::Simulator (Simulator&&) noexcept = default;

Simulator& Simulator::operator= (Simulator&&) noexcept = default;

void Simulator::open()
{
    impl_->open();
}

bool Simulator::isOpen() const noexcept
{
    return impl_ != nullptr && impl_->seq != nullptr;
}

int Simulator::clientId() const noexcept
{
    return impl_ != nullptr ? impl_->client : -1;
}

int Simulator::portId() const noexcept
{
    return impl_ != nullptr ? impl_->port : -1;
}

void Simulator::replay (const Fixture& fixture, double speed)
{
    if (! isOpen())
        throw std::runtime_error ("simulator: replay() called before open()");

    if (speed <= 0.0)
        throw std::runtime_error ("simulator: replay speed must be positive");

    const auto start = now();

    for (const auto& event : fixture.events)
    {
        const auto offset = std::chrono::microseconds (
            static_cast<std::int64_t> (static_cast<double> (std::max<std::int64_t> (event.t, 0))
                                       * 1000.0 / speed));

        std::this_thread::sleep_until (start + offset);
        sendEvent (impl_->seq, impl_->port, event, impl_->warnedNoSubscriber);
    }
}

std::size_t Simulator::pump()
{
    if (! isOpen())
        return 0;

    const std::size_t before = impl_->recorded.size();

    while (true)
    {
        snd_seq_event_t* event = nullptr;
        const int result = snd_seq_event_input (impl_->seq, &event);

        if (result < 0)
        {
            if (result == -EAGAIN)
                break;

            if (result == -ENOSPC)
                break; // input overrun: keep whatever already arrived

            throw std::runtime_error (alsaMessage ("simulator: cannot read incoming MIDI",
                                                   result));
        }

        RecordedMessage message;
        message.t = impl_->elapsedMs();

        if (recordEvent (*event, message))
            impl_->recorded.push_back (std::move (message));
    }

    return impl_->recorded.size() - before;
}

const std::vector<RecordedMessage>& Simulator::drain (int timeoutMs)
{
    const auto deadline = now() + std::chrono::milliseconds (std::max (timeoutMs, 0));

    do
    {
        pump();

        if (now() >= deadline)
            break;

        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    } while (true);

    return impl_->recorded;
}

const std::vector<RecordedMessage>& Simulator::recorded() const noexcept
{
    return impl_->recorded;
}

void Simulator::clearRecording() noexcept
{
    impl_->recorded.clear();
}

} // namespace LaunchkeySim
