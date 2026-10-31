#include "LatencyProbe.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#if defined(__linux__)
 #include <alsa/asoundlib.h>
#endif

namespace AudioEngine
{

namespace
{
#if defined(__linux__)
void silentAlsaError (const char*, int, const char*, int, const char*, ...)
{
}

struct AlsaErrorSilencer
{
    AlsaErrorSilencer()  { snd_lib_error_set_handler (&silentAlsaError); }
    ~AlsaErrorSilencer() { snd_lib_error_set_handler (nullptr); }
};

struct AlsaPcmPair
{
    juce::String playback;
    juce::String capture;
};

bool findLoopbackPair (AlsaPcmPair& pair)
{
    void** hints = nullptr;

    if (snd_device_name_hint (-1, "pcm", &hints) < 0)
        return false;

    std::vector<std::pair<juce::String, juce::String>> names;

    for (char** h = reinterpret_cast<char**> (hints); *h != nullptr; ++h)
    {
        char* name = snd_device_name_get_hint (*h, "NAME");
        char* desc = snd_device_name_get_hint (*h, "DESC");

        if (name != nullptr)
            names.emplace_back (juce::String (name),
                                desc != nullptr ? juce::String (desc) : juce::String());

        std::free (name);
        std::free (desc);
    }

    snd_device_name_free_hint (hints);

    for (const auto& entry : names)
    {
        const auto& name = entry.first;

        if (! entry.second.containsIgnoreCase ("loopback")
            || ! name.startsWith ("hw:CARD=")
            || ! name.contains (",DEV=0"))
            continue;

        const auto capture = name.replace (",DEV=0", ",DEV=1");

        for (const auto& other : names)
        {
            if (other.first == capture)
            {
                pair.playback = name;
                pair.capture = capture;
                return true;
            }
        }
    }

    return false;
}

bool configurePcm (snd_pcm_t* pcm,
                   double& sampleRate,
                   int& periodSize,
                   int& bufferSize)
{
    snd_pcm_hw_params_t* hw = nullptr;
    snd_pcm_hw_params_alloca (&hw);

    if (snd_pcm_hw_params_any (pcm, hw) < 0)
        return false;

    if (snd_pcm_hw_params_set_access (pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0
        || snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16_LE) < 0
        || snd_pcm_hw_params_set_channels (pcm, hw, 1) < 0)
        return false;

    unsigned int rate = static_cast<unsigned int> (sampleRate);

    if (snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, nullptr) < 0)
        return false;

    snd_pcm_uframes_t period = static_cast<snd_pcm_uframes_t> (juce::jmax (1, periodSize));
    snd_pcm_uframes_t buffer = static_cast<snd_pcm_uframes_t> (juce::jmax (period * 2, period));

    int dir = 0;

    if (snd_pcm_hw_params_set_period_size (pcm, hw, period, dir) < 0
        && snd_pcm_hw_params_set_period_size_near (pcm, hw, &period, &dir) < 0)
        return false;

    // Two periods of buffering keeps the probe close to the `2 * buffer`
    // figure used by the acceptance check.
    buffer = period * 2;

    if (snd_pcm_hw_params_set_buffer_size (pcm, hw, buffer) < 0
        && snd_pcm_hw_params_set_buffer_size_near (pcm, hw, &buffer) < 0)
        return false;

    if (snd_pcm_hw_params (pcm, hw) < 0)
        return false;

    snd_pcm_sw_params_t* sw = nullptr;
    snd_pcm_sw_params_alloca (&sw);

    if (snd_pcm_sw_params_current (pcm, sw) < 0
        || snd_pcm_sw_params_set_start_threshold (pcm, sw, 1) < 0
        || snd_pcm_sw_params (pcm, sw) < 0)
        return false;

    snd_pcm_uframes_t actualPeriod = 0;
    snd_pcm_uframes_t actualBuffer = 0;

    if (snd_pcm_hw_params_get_period_size (hw, &actualPeriod, &dir) < 0
        || snd_pcm_hw_params_get_buffer_size (hw, &actualBuffer) < 0)
        return false;

    sampleRate = static_cast<double> (rate);
    periodSize = static_cast<int> (actualPeriod);
    bufferSize = static_cast<int> (actualBuffer);
    return true;
}

int recover (snd_pcm_t* pcm, int error)
{
    return snd_pcm_recover (pcm, error, 0);
}
#endif
} // namespace

//==============================================================================
LatencyProbe::LatencyProbe() = default;
LatencyProbe::~LatencyProbe() = default;

LatencyResult LatencyProbe::measure (int bufferSize, double sampleRate)
{
    LatencyProbeOptions options;
    options.bufferSize = bufferSize;
    options.sampleRate = sampleRate;
    return measure (options);
}

LatencyResult LatencyProbe::measure (const LatencyProbeOptions& options)
{
    // The loopback occasionally drops the first capture block while the two
    // streams start. One retry is enough and keeps the call bounded.
    LatencyResult last;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        last = measureOnce (options);

        if (last.valid)
            return last;
    }

    return last;
}

LatencyResult LatencyProbe::measureOnce (const LatencyProbeOptions& options)
{
    LatencyResult result;
    result.sampleRate = options.sampleRate;
    result.bufferSize = options.bufferSize;

#if ! defined(__linux__)
    result.error = "the latency probe is only implemented for ALSA on Linux";
    return result;
#else
    if (options.sampleRate <= 0.0 || options.bufferSize <= 0)
    {
        result.error = "sample rate and buffer size must be positive";
        return result;
    }

    AlsaPcmPair pair;

    if (! findLoopbackPair (pair))
    {
        result.error = "no ALSA Loopback playback/capture pair was found";
        return result;
    }

    if (options.outputDeviceName.isNotEmpty())
        pair.playback = options.outputDeviceName;

    if (options.inputDeviceName.isNotEmpty())
        pair.capture = options.inputDeviceName;

    if (options.subdevice >= 0)
    {
        const auto suffix = ",SUBDEV=" + juce::String (options.subdevice);

        if (! pair.playback.contains (",SUBDEV="))
            pair.playback += suffix;

        if (! pair.capture.contains (",SUBDEV="))
            pair.capture += suffix;
    }

    result.outputDeviceName = pair.playback;
    result.inputDeviceName = pair.capture;

    AlsaErrorSilencer silencer;

    snd_pcm_t* playback = nullptr;
    snd_pcm_t* capture = nullptr;

    auto closeAll = [&playback, &capture]
    {
        if (playback != nullptr)
        {
            snd_pcm_drop (playback);
            snd_pcm_close (playback);
            playback = nullptr;
        }

        if (capture != nullptr)
        {
            snd_pcm_drop (capture);
            snd_pcm_close (capture);
            capture = nullptr;
        }
    };

    auto fail = [&result, &closeAll] (const juce::String& message)
    {
        closeAll();
        result.error = message;
        return result;
    };

    if (snd_pcm_open (&playback, pair.playback.toRawUTF8(),
                      SND_PCM_STREAM_PLAYBACK, 0) < 0)
        return fail ("could not open playback device " + pair.playback);

    if (snd_pcm_open (&capture, pair.capture.toRawUTF8(),
                      SND_PCM_STREAM_CAPTURE, 0) < 0)
        return fail ("could not open capture device " + pair.capture);

    double rate = options.sampleRate;
    int period = options.bufferSize;
    int buffer = options.bufferSize * 2;

    if (! configurePcm (playback, rate, period, buffer)
        || ! configurePcm (capture, rate, period, buffer))
        return fail ("could not configure the Loopback devices");

    if (snd_pcm_prepare (playback) < 0 || snd_pcm_prepare (capture) < 0)
        return fail ("could not prepare the Loopback devices");

    result.sampleRate = rate;
    result.bufferSize = period;

    const int captureSamples = juce::jmax (options.captureSamples,
                                           options.impulseOffset + period * 8);
    const int impulseOffset = juce::jlimit (0, captureSamples - 1,
                                            options.impulseOffset);

    std::vector<std::int16_t> emitted (static_cast<size_t> (captureSamples), 0);
    std::vector<std::int16_t> captured (static_cast<size_t> (captureSamples), 0);

    emitted[static_cast<size_t> (impulseOffset)] = 16000;

    for (int offset = 0; offset < captureSamples; offset += period)
    {
        const int want = juce::jmin (period, captureSamples - offset);

        // Read first, then write. This is the same order as a full-duplex
        // audio callback and gives the loopback a chance to fill its capture
        // ring before the impulse is queued.
        auto read = snd_pcm_readi (capture, captured.data() + offset,
                                   static_cast<snd_pcm_uframes_t> (want));

        if (read < 0)
            recover (capture, static_cast<int> (read));

        auto written = snd_pcm_writei (playback, emitted.data() + offset,
                                       static_cast<snd_pcm_uframes_t> (want));

        if (written < 0)
            recover (playback, static_cast<int> (written));
    }

    closeAll();

    std::vector<float> reference (static_cast<size_t> (captureSamples), 0.0f);
    std::vector<float> captureFloat (static_cast<size_t> (captureSamples), 0.0f);

    reference[static_cast<size_t> (impulseOffset)] = 0.5f;

    for (int i = 0; i < captureSamples; ++i)
        captureFloat[static_cast<size_t> (i)] =
            static_cast<float> (captured[static_cast<size_t> (i)]) / 32768.0f;

    const int maxLag = juce::jmin (captureSamples - impulseOffset - 1,
                                   captureSamples - 1);

    if (maxLag < 0)
    {
        result.error = "capture window is shorter than the impulse offset";
        return result;
    }

    float confidence = 0.0f;
    const int lag = findLagSamples (reference.data(),
                                    captureSamples,
                                    captureFloat.data(),
                                    captureSamples,
                                    maxLag,
                                    &confidence);

    if (lag < 0 || confidence <= 0.0f)
    {
        result.error = "no impulse was found in the capture";
        return result;
    }

    result.valid = true;
    result.roundTripSamples = lag;
    result.roundTripMs = static_cast<double> (lag) * 1000.0 / result.sampleRate;
    result.confidence = confidence;
    return result;
#endif
}

//==============================================================================
int LatencyProbe::findLagSamples (const float* reference,
                                  int referenceSamples,
                                  const float* captured,
                                  int capturedSamples,
                                  int maxLag,
                                  float* peak)
{
    if (peak != nullptr)
        *peak = 0.0f;

    if (reference == nullptr || captured == nullptr
        || referenceSamples <= 0 || capturedSamples <= 0 || maxLag < 0)
        return -1;

    maxLag = juce::jmin (maxLag, capturedSamples - 1);

    int bestLag = -1;
    double bestScore = 0.0;

    for (int lag = 0; lag <= maxLag; ++lag)
    {
        const int first = juce::jmax (0, lag);
        const int last = juce::jmin (capturedSamples, referenceSamples + lag);

        if (first >= last)
            continue;

        double sum = 0.0;
        double referenceEnergy = 0.0;
        double capturedEnergy = 0.0;

        for (int i = first; i < last; ++i)
        {
            const double ref = static_cast<double> (reference[i - lag]);
            const double cap = static_cast<double> (captured[i]);

            sum += ref * cap;
            referenceEnergy += ref * ref;
            capturedEnergy += cap * cap;
        }

        if (referenceEnergy <= 0.0 || capturedEnergy <= 0.0)
            continue;

        const double score = std::abs (sum) / std::sqrt (referenceEnergy * capturedEnergy);

        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (peak != nullptr)
        *peak = static_cast<float> (bestScore);

    return bestLag;
}

} // namespace AudioEngine
