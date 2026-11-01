#include "Fixtures.h"

namespace LaunchpadMappingTest
{

const std::vector<std::string> expectedFamilies {
    "keys", "drums", "bass", "pads", "lead", "fx", "perc", "vox"
};

std::vector<std::string> expectedInstruments (const std::string& family)
{
    if (family == "keys")
        return { "piano", "strings", "rhodes", "organ", "harp", "choir", "celesta", "clav" };
    if (family == "drums")
        return { "kit", "snare", "hihats", "toms", "clap", "rimshot", "cymbals", "kick" };
    if (family == "bass")
        return { "sub", "finger", "pick", "fretless", "synth", "upright", "growl", "octave" };
    if (family == "pads")
        return { "air", "warm", "glass", "choir", "motion", "dark", "shimmer", "tape" };
    if (family == "lead")
        return { "saw", "square", "sine", "supersaw", "pluck", "brass", "flute", "whistle" };
    if (family == "fx")
        return { "riser", "downlifter", "impact", "sweep", "noise", "glitch", "drone", "reverse" };
    if (family == "perc")
        return { "shaker", "tambourine", "conga", "bongo", "triangle", "woodblock", "cowbell", "tabla" };
    if (family == "vox")
        return { "ah", "oo", "eh", "whisper", "shout", "vocoder", "breath", "choir" };

    return {};
}

} // namespace LaunchpadMappingTest
