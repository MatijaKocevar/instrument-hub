#include "MidiInputBackend.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <cstdio>
#include <utility>

#if JUCE_LINUX
 #include <alsa/asoundlib.h>
#endif

namespace Api
{
namespace
{

std::string toStd (const juce::String& value)
{
    return value.toStdString();
}

#if JUCE_LINUX
int alsaClientForIdentifier (const std::string& identifier)
{
    int client = -1;
    int port = -1;

    if (std::sscanf (identifier.c_str(), "ALSA:%d:%d", &client, &port) != 2
        && std::sscanf (identifier.c_str(), "%d-%d", &client, &port) != 2)
        return -1;

    return client;
}

class AlsaClientNames
{
public:
    AlsaClientNames()
    {
        if (snd_seq_open (&sequencer, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0)
            sequencer = nullptr;
    }

    ~AlsaClientNames()
    {
        if (sequencer != nullptr)
            snd_seq_close (sequencer);
    }

    std::string nameFor (int client) const
    {
        if (sequencer == nullptr || client < 0)
            return {};

        snd_seq_client_info_t* info = nullptr;
        snd_seq_client_info_alloca (&info);

        if (snd_seq_get_any_client_info (sequencer, client, info) < 0)
            return {};

        const auto* name = snd_seq_client_info_get_name (info);
        return name != nullptr ? std::string (name) : std::string {};
    }

private:
    snd_seq_t* sequencer = nullptr;
};
#endif

class JuceMidiInputHandle final : public MidiInputHandle
{
public:
    explicit JuceMidiInputHandle (std::unique_ptr<juce::MidiInput> device)
        : input (std::move (device))
    {
    }

    void start() override
    {
        if (input != nullptr)
            input->start();
    }

    void stop() override
    {
        if (input != nullptr)
            input->stop();
    }

private:
    std::unique_ptr<juce::MidiInput> input;
};

class JuceMidiInputBackend final : public MidiInputBackend
{
public:
    explicit JuceMidiInputBackend (juce::MidiInputCallback& callback)
        : midiCallback (callback)
    {
    }

    std::vector<MidiInputDevice> list() override
    {
        return listJuceMidiInputDevices();
    }

    std::unique_ptr<MidiInputHandle> open (const std::string& id) override
    {
        auto input = juce::MidiInput::openDevice (juce::String (id), &midiCallback);

        if (input == nullptr)
            return {};

        return std::make_unique<JuceMidiInputHandle> (std::move (input));
    }

private:
    juce::MidiInputCallback& midiCallback;
};

}

std::vector<MidiInputDevice> listJuceMidiInputDevices()
{
    std::vector<MidiInputDevice> devices;

#if JUCE_LINUX
    AlsaClientNames clients;
#endif

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        MidiInputDevice entry;
        entry.id = toStd (device.identifier);
        entry.name = toStd (device.name);

#if JUCE_LINUX
        entry.client = clients.nameFor (alsaClientForIdentifier (entry.id));
#endif

        devices.push_back (std::move (entry));
    }

    return devices;
}

std::unique_ptr<MidiInputBackend> makeJuceMidiInputBackend (juce::MidiInputCallback& callback)
{
    return std::make_unique<JuceMidiInputBackend> (callback);
}

}
