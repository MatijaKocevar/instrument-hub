#include "StateBroadcast.h"

#include "Routes.h"

#include <httplib.h>

#include <string_view>

namespace Api
{

namespace
{
constexpr int maximumPollIntervalUs = 1'000'000;

const char* eventKindName (StateEventKind kind) noexcept
{
    switch (kind)
    {
        case StateEventKind::initial:    return "initial";
        case StateEventKind::pad:        return "pad";
        case StateEventKind::arm:        return "arm";
        case StateEventKind::settings:   return "settings";
        case StateEventKind::transport:  return "transport";
        case StateEventKind::slot:       return "slot";
        case StateEventKind::patch:      return "patch";
        case StateEventKind::warm:       return "warm";
        case StateEventKind::device:     return "device";
        case StateEventKind::midi:       return "midi";
        case StateEventKind::pairing:    return "pairing";
        case StateEventKind::project:    return "project";
    }

    return "state";
}

StateEventKind kindForChange (const char* kind) noexcept
{
    if (kind == nullptr)
        return StateEventKind::slot;

    const std::string_view name (kind);

    if (name == "arm")
        return StateEventKind::arm;

    if (name == "settings")
        return StateEventKind::settings;

    if (name == "transport")
        return StateEventKind::transport;

    if (name == "patch")
        return StateEventKind::patch;

    if (name == "warm")
        return StateEventKind::warm;

    if (name == "device")
        return StateEventKind::device;

    if (name == "midi")
        return StateEventKind::midi;

    if (name == "pairing")
        return StateEventKind::pairing;

    if (name == "project")
        return StateEventKind::project;

    return StateEventKind::slot;
}
}

StateBroadcast::StateBroadcast (StateStore& stateToUse,
                                StateBroadcastOptions optionsToUse)
    : state (stateToUse),
      options (optionsToUse)
{
    state.setChangeHandler ([this] (const char* kind, int slot)
    {
        publishStateChange (kind, slot);
    });
}

StateBroadcast::~StateBroadcast()
{
    stop();
    state.setChangeHandler ({});
}

void StateBroadcast::start()
{
    if (running.exchange (true, std::memory_order_acq_rel))
        return;

    publish (StateEventKind::initial, -1, -1, false);
}

void StateBroadcast::stop()
{
    running.store (false, std::memory_order_release);
}

bool StateBroadcast::isRunning() const noexcept
{
    return running.load (std::memory_order_acquire);
}

void StateBroadcast::publishPad (int pad, bool pressed)
{
    if (pad < 0 || pad >= StateBroadcastOptions::maxPads)
        return;

    publish (StateEventKind::pad, pad, -1, pressed);
}

void StateBroadcast::publishStateChange (const char* kind, int slot)
{
    publish (kindForChange (kind), -1, slot, false);
}

void StateBroadcast::publish (StateEventKind kind,
                              int pad,
                              int slot,
                              bool pressed)
{
    const auto sequence = nextSequence.fetch_add (1, std::memory_order_acq_rel) + 1;
    const auto timestampMs = state.getUptimeMs();

    {
        const std::lock_guard<std::mutex> lock (frameMutex);

        if (kind == StateEventKind::pad
            && pad >= 0 && pad < StateBroadcastOptions::maxPads)
        {
            auto& mirror = pads[static_cast<std::size_t> (pad)];
            mirror.pressed = pressed;
            mirror.updatedMs = timestampMs;
        }

        latestFrameJson = buildFrameLocked (sequence, kind, pad, slot, pressed,
                                            timestampMs);
        latestSequence = sequence;
    }

    eventsPublished.fetch_add (1, std::memory_order_relaxed);
}

std::string StateBroadcast::buildFrameLocked (std::uint64_t sequence,
                                              StateEventKind kind,
                                              int pad,
                                              int slot,
                                              bool pressed,
                                              double timestampMs)
{
    auto frame = juce::var (new juce::DynamicObject());
    auto* root = frame.getDynamicObject();

    const auto snapshot = state.snapshot();

    root->setProperty ("type", "state");
    root->setProperty ("sequence", static_cast<juce::int64> (sequence));
    root->setProperty ("revision", static_cast<juce::int64> (snapshot.revision));
    root->setProperty ("timestampMs", timestampMs);

    auto event = juce::var (new juce::DynamicObject());
    auto* eventObject = event.getDynamicObject();
    eventObject->setProperty ("kind", eventKindName (kind));
    eventObject->setProperty ("pad", pad);
    eventObject->setProperty ("slot", slot);
    eventObject->setProperty ("pressed", pressed);
    eventObject->setProperty ("timestampMs", timestampMs);
    root->setProperty ("event", event);

    juce::Array<juce::var> padArray;

    for (int index = 0; index < StateBroadcastOptions::maxPads; ++index)
    {
        const auto& mirror = pads[static_cast<std::size_t> (index)];

        auto padVar = juce::var (new juce::DynamicObject());
        padVar.getDynamicObject()->setProperty ("index", index);
        padVar.getDynamicObject()->setProperty ("pressed", mirror.pressed);
        padVar.getDynamicObject()->setProperty ("updatedMs", mirror.updatedMs);
        padArray.add (padVar);
    }

    root->setProperty ("pads", juce::var (padArray));

    root->setProperty ("state", stateToVar (snapshot));

    return toJson (frame);
}

void StateBroadcast::handleClient (httplib::ws::WebSocket& websocket)
{
    clients.fetch_add (1, std::memory_order_acq_rel);

    const auto pollUs = juce::jlimit (1, maximumPollIntervalUs,
                                      options.clientPollIntervalUs);
    websocket.set_read_timeout (0, pollUs);

    std::uint64_t sentSequence = 0;
    std::string incoming;

    while (running.load (std::memory_order_acquire) && websocket.is_open())
    {
        std::string payload;
        std::uint64_t sequence = 0;

        {
            const std::lock_guard<std::mutex> lock (frameMutex);
            sequence = latestSequence;

            if (sequence != sentSequence)
                payload = latestFrameJson;
        }

        if (sequence != sentSequence)
        {
            if (! websocket.send (payload))
                break;

            sentSequence = sequence;
            framesSent.fetch_add (1, std::memory_order_relaxed);
        }

        if (websocket.read (incoming) == httplib::ws::ReadResult::Fail)
            break;
    }

    if (websocket.is_open())
        websocket.close (httplib::ws::CloseStatus::Normal, "state broadcast stopping");

    clients.fetch_sub (1, std::memory_order_acq_rel);
}

PadMirror StateBroadcast::getPad (int pad) const
{
    if (pad < 0 || pad >= StateBroadcastOptions::maxPads)
        return {};

    const std::lock_guard<std::mutex> lock (frameMutex);
    return pads[static_cast<std::size_t> (pad)];
}

std::uint64_t StateBroadcast::getEventCount() const noexcept
{
    return eventsPublished.load (std::memory_order_acquire);
}

std::uint64_t StateBroadcast::getFrameCount() const noexcept
{
    return framesSent.load (std::memory_order_acquire);
}

int StateBroadcast::getClientCount() const noexcept
{
    return clients.load (std::memory_order_acquire);
}

std::uint64_t StateBroadcast::getLastSequence() const noexcept
{
    const std::lock_guard<std::mutex> lock (frameMutex);
    return latestSequence;
}

std::string StateBroadcast::getLastFrameJson() const
{
    const std::lock_guard<std::mutex> lock (frameMutex);
    return latestFrameJson;
}

}
