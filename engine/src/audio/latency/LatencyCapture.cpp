#include "audio/latency/LatencyCapture.h"

#include "audio/latency/AlsaLoopback.h"

#include <cstdint>

namespace AudioEngine::detail {

#if defined(__linux__)
namespace {
void silentAlsaError(const char*, int, const char*, int, const char*, ...) {}

struct AlsaErrorSilencer {
    AlsaErrorSilencer() { snd_lib_error_set_handler(&silentAlsaError); }
    ~AlsaErrorSilencer() { snd_lib_error_set_handler(nullptr); }
};
}  // namespace
#endif

CapturedLoopbackImpulse captureLoopbackImpulse(const LatencyProbeOptions& options) {
    CapturedLoopbackImpulse capture;

#if !defined(__linux__)
    capture.error = "the latency probe is only implemented for ALSA on Linux";
    return capture;
#else
    if (options.sampleRate <= 0.0 || options.bufferSize <= 0) {
        capture.error = "sample rate and buffer size must be positive";
        return capture;
    }

    AlsaPcmPair pair;

    if (!findAlsaLoopbackPair(pair)) {
        capture.error = "no ALSA Loopback playback/capture pair was found";
        return capture;
    }

    if (options.outputDeviceName.isNotEmpty()) pair.playback = options.outputDeviceName;

    if (options.inputDeviceName.isNotEmpty()) pair.capture = options.inputDeviceName;

    if (options.subdevice >= 0) {
        const auto suffix = ",SUBDEV=" + juce::String(options.subdevice);

        if (!pair.playback.contains(",SUBDEV=")) pair.playback += suffix;

        if (!pair.capture.contains(",SUBDEV=")) pair.capture += suffix;
    }

    capture.playbackDevice = pair.playback;
    capture.captureDevice = pair.capture;

    AlsaErrorSilencer silencer;

    snd_pcm_t* playback = nullptr;
    snd_pcm_t* captureDevice = nullptr;

    auto closeAll = [&playback, &captureDevice] {
        if (playback != nullptr) {
            snd_pcm_drop(playback);
            snd_pcm_close(playback);
            playback = nullptr;
        }

        if (captureDevice != nullptr) {
            snd_pcm_drop(captureDevice);
            snd_pcm_close(captureDevice);
            captureDevice = nullptr;
        }
    };

    auto fail = [&capture, &closeAll](const juce::String& message) {
        closeAll();
        capture.error = message;
        return capture;
    };

    if (snd_pcm_open(&playback, pair.playback.toRawUTF8(), SND_PCM_STREAM_PLAYBACK, 0) < 0)
        return fail("could not open playback device " + pair.playback);

    if (snd_pcm_open(&captureDevice, pair.capture.toRawUTF8(), SND_PCM_STREAM_CAPTURE, 0) < 0)
        return fail("could not open capture device " + pair.capture);

    double rate = options.sampleRate;
    int period = options.bufferSize;
    int buffer = options.bufferSize * 2;

    if (!configureAlsaPcm(playback, rate, period, buffer) ||
        !configureAlsaPcm(captureDevice, rate, period, buffer))
        return fail("could not configure the Loopback devices");

    if (snd_pcm_prepare(playback) < 0 || snd_pcm_prepare(captureDevice) < 0)
        return fail("could not prepare the Loopback devices");

    capture.sampleRate = rate;
    capture.bufferSize = period;

    const int captureSamples =
        juce::jmax(options.captureSamples, options.impulseOffset + period * 8);
    const int impulseOffset = juce::jlimit(0, captureSamples - 1, options.impulseOffset);

    std::vector<std::int16_t> emitted(static_cast<size_t>(captureSamples), 0);
    std::vector<std::int16_t> recorded(static_cast<size_t>(captureSamples), 0);

    emitted[static_cast<size_t>(impulseOffset)] = 16000;

    for (int offset = 0; offset < captureSamples; offset += period) {
        const int want = juce::jmin(period, captureSamples - offset);

        auto read = snd_pcm_readi(captureDevice, recorded.data() + offset,
                                  static_cast<snd_pcm_uframes_t>(want));

        if (read < 0) recoverAlsaPcm(captureDevice, static_cast<int>(read));

        auto written =
            snd_pcm_writei(playback, emitted.data() + offset, static_cast<snd_pcm_uframes_t>(want));

        if (written < 0) recoverAlsaPcm(playback, static_cast<int>(written));
    }

    closeAll();

    capture.impulseOffset = impulseOffset;
    capture.reference.assign(static_cast<size_t>(captureSamples), 0.0f);
    capture.captured.assign(static_cast<size_t>(captureSamples), 0.0f);
    capture.reference[static_cast<size_t>(impulseOffset)] = 0.5f;

    for (int i = 0; i < captureSamples; ++i)
        capture.captured[static_cast<size_t>(i)] =
            static_cast<float>(recorded[static_cast<size_t>(i)]) / 32768.0f;

    capture.valid = true;
    return capture;
#endif
}

}  // namespace AudioEngine::detail
