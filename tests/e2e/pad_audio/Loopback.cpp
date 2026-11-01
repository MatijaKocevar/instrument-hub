#include "Check.h"
#include "pad_audio/AlsaPcm.h"
#include "pad_audio/Constants.h"
#include "pad_audio/Loopback.h"

#include <cstdint>
#include <vector>

#if defined(__linux__)
 #include <alsa/asoundlib.h>
#endif

namespace PadAudio
{

#if defined(__linux__)
namespace
{
void silentAlsaError (const char*, int, const char*, int, const char*, ...)
{
}

struct AlsaErrorSilencer
{
    AlsaErrorSilencer()  { snd_lib_error_set_handler (&silentAlsaError); }
    ~AlsaErrorSilencer() { snd_lib_error_set_handler (nullptr); }
};
}
#endif

namespace
{
std::vector<std::int16_t> toPcm16 (const juce::AudioBuffer<float>& buffer, int frames)
{
    std::vector<std::int16_t> result (static_cast<std::size_t> (frames), 0);

    for (int frame = 0; frame < frames; ++frame)
    {
        const float value = juce::jlimit (-1.0f, 1.0f, buffer.getSample (0, frame));
        result[static_cast<std::size_t> (frame)] =
            static_cast<std::int16_t> (juce::roundToInt (value * 32767.0f));
    }

    return result;
}
}

LoopbackResult runLoopback (ihub::Session& session, PadRig& rig)
{
    LoopbackResult result;

#if ! defined(__linux__)
    result.error = "the ALSA Loopback pass is Linux-only";
    return result;
#else
    AlsaPcm pcm;

    const AlsaErrorSilencer silencer;
    juce::ignoreUnused (silencer);

    if (! pcm.open (result.error))
        return result;

    result.playbackName = pcm.playbackName;
    result.captureName = pcm.captureName;

    juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
    buffer.clear();

    std::vector<std::int16_t> pcmBlock (static_cast<std::size_t> (kBlockSize), 0);
    std::vector<std::int16_t> silence (static_cast<std::size_t> (kBlockSize), 0);

    for (int index = 0; index < 4; ++index)
    {
        if (! pcm.write (silence.data(), kBlockSize))
        {
            result.error = "loopback playback did not start";
            return result;
        }

        if (! pcm.read (pcmBlock.data(), kBlockSize))
        {
            result.error = "loopback capture did not start";
            return result;
        }
    }

    for (int block = 0; block < kLoopbackBlocks; ++block)
    {
        if (! pcm.read (pcmBlock.data(), kBlockSize))
        {
            result.error = "loopback capture failed at block " + juce::String (block);
            return result;
        }

        if (block >= kLoopbackWarmupBlocks)
        {
            for (int frame = 0; frame < kBlockSize; ++frame)
                result.captured.push_back (
                    static_cast<float> (pcmBlock[static_cast<std::size_t> (frame)]) / 32768.0f);
        }

        if (block == kLoopbackArmFirstBlock)
        {
            rig.press (kFirstPad);
            result.armedAfterFirst = session.getArmedSlot();
        }

        if (block == kLoopbackArmSecondBlock)
        {
            rig.press (kSecondPad);
            result.armedAfterSecond = session.getArmedSlot();
        }

        juce::MidiBuffer midi;
        session.render (buffer, midi, kBlockSize);

        const auto output = toPcm16 (buffer, kBlockSize);

        if (! pcm.write (output.data(), kBlockSize))
        {
            result.error = "loopback playback failed at block " + juce::String (block);
            return result;
        }
    }

    result.ran = true;
    return result;
#endif
}

}
