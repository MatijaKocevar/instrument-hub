#include "DeviceCards.h"

#if defined(__linux__)
#include <alsa/asoundlib.h>
#endif

#include <utility>

namespace AudioEngine::detail {

std::vector<AlsaCard> enumerateAlsaCards() {
    std::vector<AlsaCard> result;

#if defined(__linux__)
    snd_ctl_card_info_t* cardInfo = nullptr;
    snd_ctl_card_info_alloca(&cardInfo);

    int cardNumber = -1;

    while (snd_card_next(&cardNumber) == 0 && cardNumber >= 0) {
        snd_ctl_t* control = nullptr;
        const auto controlName = "hw:" + juce::String(cardNumber);

        if (snd_ctl_open(&control, controlName.toRawUTF8(), SND_CTL_NONBLOCK) < 0) continue;

        if (snd_ctl_card_info(control, cardInfo) >= 0) {
            AlsaCard card;
            card.id = snd_ctl_card_info_get_id(cardInfo);
            card.name = snd_ctl_card_info_get_name(cardInfo);

            if (card.name.isEmpty()) card.name = card.id;

            int chosenDevice = -1;
            juce::String chosenPcm;
            bool chosenInput = false;
            bool chosenAnalog = false;

            snd_pcm_info_t* pcmInfo = nullptr;
            snd_pcm_info_alloca(&pcmInfo);

            int device = -1;

            while (snd_ctl_pcm_next_device(control, &device) == 0 && device >= 0) {
                snd_pcm_info_set_device(pcmInfo, static_cast<unsigned int>(device));
                snd_pcm_info_set_subdevice(pcmInfo, 0);
                snd_pcm_info_set_stream(pcmInfo, SND_PCM_STREAM_PLAYBACK);

                if (snd_ctl_pcm_info(control, pcmInfo) < 0) continue;

                const juce::String pcmName(snd_pcm_info_get_name(pcmInfo));
                const bool analog = pcmName.containsIgnoreCase("analog");

                snd_pcm_info_set_stream(pcmInfo, SND_PCM_STREAM_CAPTURE);
                const bool input = snd_ctl_pcm_info(control, pcmInfo) >= 0;

                if (chosenDevice < 0 || (analog && !chosenAnalog)) {
                    chosenDevice = device;
                    chosenPcm = pcmName;
                    chosenInput = input;
                    chosenAnalog = analog;
                }
            }

            if (chosenDevice >= 0) {
                card.device = chosenDevice;
                card.pcmName = chosenPcm;
                card.hasInput = chosenInput;
                result.push_back(std::move(card));
            }
        }

        snd_ctl_close(control);
    }
#endif

    return result;
}

juce::String friendlyCardName(const AlsaCard& card) {
    auto name = card.name.trim();

    if (name.isEmpty()) name = card.id;

    if (card.pcmName.containsIgnoreCase("analog") && !name.containsIgnoreCase("analog"))
        name << " (analog)";
    else if (card.pcmName.containsIgnoreCase("hdmi") && !name.containsIgnoreCase("hdmi"))
        name << " (HDMI)";

    return name;
}

juce::String cardDescription(const AlsaCard& card) {
    if (card.pcmName.containsIgnoreCase("analog")) return "Onboard analog output";

    if (card.pcmName.containsIgnoreCase("hdmi")) return "Onboard HDMI output";

    return "USB audio interface";
}

std::vector<juce::String> cardPcmCandidates(const AlsaCard& card) {
    const auto device = juce::String(card.device);

    return {"hw:CARD=" + card.id + ",DEV=" + device, "plughw:CARD=" + card.id + ",DEV=" + device,
            "front:CARD=" + card.id + ",DEV=" + device, "default:CARD=" + card.id,
            "sysdefault:CARD=" + card.id};
}

int alsaPlaybackChannelCount(const juce::String& pcmName) {
#if defined(__linux__)
    snd_pcm_t* pcm = nullptr;

    if (snd_pcm_open(&pcm, pcmName.toRawUTF8(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0)
        return 0;

    int channels = 0;
    snd_pcm_hw_params_t* params = nullptr;
    snd_pcm_hw_params_alloca(&params);

    if (snd_pcm_hw_params_any(pcm, params) >= 0) {
        unsigned int minimum = 0;
        unsigned int maximum = 0;

        if (snd_pcm_hw_params_get_channels_min(params, &minimum) == 0 &&
            snd_pcm_hw_params_get_channels_max(params, &maximum) == 0) {
            channels = static_cast<int>(juce::jmin(maximum, 256u));
            channels = juce::jmax(channels, static_cast<int>(minimum));
        }
    }

    snd_pcm_close(pcm);
    return channels;
#else
    juce::ignoreUnused(pcmName);
    return 0;
#endif
}

int alsaPcmOpenError(const juce::String& pcmName) {
#if defined(__linux__)
    snd_pcm_t* pcm = nullptr;
    const int error =
        snd_pcm_open(&pcm, pcmName.toRawUTF8(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);

    if (error < 0) return error;

    snd_pcm_close(pcm);
    return 0;
#else
    juce::ignoreUnused(pcmName);
    return 0;
#endif
}

}  // namespace AudioEngine::detail
