#include "api/MidiBridge.h"

#include <utility>

namespace Api {
namespace {
constexpr const char* networkPrefix = "net:";
}

bool MidiBridge::isNetworkId(const std::string& id) {
    return id.rfind(networkPrefix, 0) == 0;
}

void MidiBridge::attachNetworkClient(const std::string& id, const std::string& name,
                                     std::uint64_t session) {
    std::array<bool, NetworkClient::noteCount> previousNotes{};
    std::uint16_t previousChannels = 0;

    {
        const std::lock_guard<std::mutex> lock(networkMutex);
        auto& client = networkClients[id];

        if (client.connected) {
            previousNotes = client.notes;
            previousChannels = client.channels;
        }

        client = NetworkClient{};
        client.session = session;
        client.name = name;
        client.connected = true;
        client.enabled = true;
    }

    releaseNetworkNotes(previousNotes, previousChannels);
}

void MidiBridge::detachNetworkClient(const std::string& id, std::uint64_t session) {
    std::array<bool, NetworkClient::noteCount> notes{};
    std::uint16_t channels = 0;

    {
        const std::lock_guard<std::mutex> lock(networkMutex);
        const auto client = networkClients.find(id);

        if (client == networkClients.end() || client->second.session != session) return;

        notes = client->second.notes;
        channels = client->second.channels;
        networkClients.erase(client);
    }

    releaseNetworkNotes(notes, channels);
}

bool MidiBridge::networkMidi(const std::string& id, std::uint64_t session,
                             const std::uint8_t* data, int size) {
    if (data == nullptr || size < 1 || size > 3) return false;

    const auto status = data[0];

    if (status >= 0xf0) return false;

    std::string name;

    {
        const std::lock_guard<std::mutex> lock(networkMutex);
        const auto client = networkClients.find(id);

        if (client == networkClients.end() || client->second.session != session ||
            !client->second.connected || !client->second.enabled) {
            return false;
        }

        const juce::MidiMessage message(data, size);
        client->second.events += 1;
        client->second.lastEvent = describeMessage(message);
        name = client->second.name;

        const auto type = status & 0xf0;
        const auto channel = status & 0x0f;

        if (size >= 3 && (type == 0x80 || type == 0x90)) {
            const auto note = data[1] & 0x7f;
            const auto index = channel * 128 + note;

            client->second.notes[static_cast<std::size_t>(index)] =
                type == 0x90 && data[2] > 0;
            client->second.channels |= static_cast<std::uint16_t>(1u << channel);
        }
    }

    return processMidiMessage(networkPrefix + id, juce::String(name),
                              juce::MidiMessage(data, size));
}

bool MidiBridge::setNetworkEnabled(const std::string& id, bool enabled) {
    std::array<bool, NetworkClient::noteCount> notes{};
    std::uint16_t channels = 0;

    {
        const std::lock_guard<std::mutex> lock(networkMutex);
        const auto client = networkClients.find(id);

        if (client == networkClients.end()) return false;

        client->second.enabled = enabled;

        if (!enabled) {
            notes = client->second.notes;
            channels = client->second.channels;
            client->second.notes.fill(false);
            client->second.channels = 0;
        }
    }

    releaseNetworkNotes(notes, channels);

    return true;
}

std::optional<MidiInputDevice> MidiBridge::findNetworkDevice(const std::string& id) const {
    if (!isNetworkId(id)) return std::nullopt;

    const auto clientId = id.substr(std::string(networkPrefix).size());
    const std::lock_guard<std::mutex> lock(networkMutex);
    const auto client = networkClients.find(clientId);

    if (client == networkClients.end() || !client->second.connected) return std::nullopt;

    return MidiInputDevice{id, client->second.name, "WiFi"};
}

void MidiBridge::releaseNetworkNotes(std::array<bool, NetworkClient::noteCount> notes,
                                     std::uint16_t channels) {
    if (channels == 0) return;

    for (int channel = 0; channel < 16; ++channel) {
        if ((channels & (1u << channel)) == 0) continue;

        for (int note = 0; note < 128; ++note) {
            if (notes[static_cast<std::size_t>(channel * 128 + note)])
                (void)sendToSession(juce::MidiMessage::noteOff(channel + 1, note));
        }

        (void)sendToSession(juce::MidiMessage::controllerEvent(channel + 1, 64, 0));
        (void)sendToSession(juce::MidiMessage::controllerEvent(channel + 1, 123, 0));
        (void)sendToSession(juce::MidiMessage::controllerEvent(channel + 1, 120, 0));
    }
}

}  // namespace Api
