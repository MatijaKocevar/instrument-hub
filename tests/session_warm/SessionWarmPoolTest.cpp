#include "Session.h"
#include "api/Server.h"
#include "api/SessionBridge.h"

#include <juce_events/juce_events.h>

#include <httplib.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr int kSlots = 64;
constexpr int kNeighbours = 2;
constexpr int kInstanceLimit = 1 + kNeighbours;
constexpr int kFamilies = 8;
constexpr int kPatchesPerFamily = 8;
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr double kMinimumRmsDbfs = -60.0;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

double toDbfs (double linear) noexcept
{
    return 20.0 * std::log10 (juce::jmax (linear, 1.0e-12));
}

double renderRms (ihub::Session& session, int blocks)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    double sumSquares = 0.0;
    int frames = 0;

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        session.render (buffer, midi, kBlockSize);

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const double sample = buffer.getSample (0, frame);
            sumSquares += sample * sample;
            ++frames;
        }
    }

    return frames > 0 ? std::sqrt (sumSquares / static_cast<double> (frames)) : 0.0;
}

juce::var property (const juce::var& value, const char* name)
{
    if (const auto* object = value.getDynamicObject())
        return object->getProperty (juce::Identifier (name));

    return {};
}

ihub::SessionConfig makeConfig()
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.device.preferLoopback = false;
    config.fadeMilliseconds = 3.0f;
    config.initialSlot = 0;
    config.neighbourCount = kNeighbours;
    config.loadDelayMilliseconds = 40;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::initTone);
    return config;
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-session-warm-test: 64 patches, K=" << kNeighbours
              << ", instance limit=" << kInstanceLimit << std::endl;

    ihub::Session session;
    juce::String error;

    if (! session.build (makeConfig(), error))
    {
        std::cerr << "ihub-session-warm-test: FAILED to build: "
                  << error << std::endl;
        return 1;
    }

    check (session.getNumSlots() == kSlots, "the fixed engine has 64 slots");
    check (static_cast<int> (session.getSlotConfigs().size()) == kSlots,
           "64 patch descriptors are configured");
    check (session.getWarmInstanceLimit() == kInstanceLimit,
           "the warm instance limit is K+1 = 3");

    int familyCount = 0;
    int patchCount = 0;
    std::string currentFamily;
    bool familiesOk = true;

    for (const auto& slot : session.getSlotConfigs())
    {
        if (slot.family != currentFamily)
        {
            currentFamily = slot.family;
            ++familyCount;
            patchCount = 0;
        }

        ++patchCount;
        familiesOk = familiesOk && ! slot.family.empty() && ! slot.patch.empty();
    }

    check (familyCount == kFamilies && familiesOk
               && static_cast<int> (session.getSlotConfigs().size())
                      == kFamilies * kPatchesPerFamily,
           "the 64 patches form 8 families x 8 patches");

    check (session.waitForSlotWarm (0, 5000), "the initial patch warms");
    session.armSlot (0);
    check (session.waitForSlotActive (0, 5000),
           "arming the initial patch reaches active");

    const double initialRms = renderRms (session, 16);
    check (initialRms > 0.0 && toDbfs (initialRms) > kMinimumRmsDbfs,
           "the active patch renders non-silent audio ("
               + std::to_string (toDbfs (initialRms)) + " dBFS)");
    check (session.getLiveInstrumentCount() <= kInstanceLimit,
           "at most K+1 instruments are instantiated at startup");

    const int coldSlot = 40;
    check (! session.isSlotWarm (coldSlot), "slot 40 starts cold");

    session.armSlot (coldSlot);

    check (session.getWarmStateName (coldSlot) == std::string ("warming"),
           "a cold arm reports warming immediately");

    std::vector<std::string> observed;
    const double deadline = juce::Time::getMillisecondCounterHiRes() + 5000.0;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        const std::string state = session.getWarmStateName (coldSlot);

        if (observed.empty() || observed.back() != state)
        {
            observed.push_back (state);
            std::cout << "      slot " << coldSlot << " state: " << state << std::endl;
        }

        if (session.getWarmState (coldSlot) == ihub::SessionWarmState::active)
            break;

        juce::Thread::sleep (2);
    }

    check (std::find (observed.begin(), observed.end(), "warming") != observed.end(),
           "the cold arm is observed as warming");
    check (session.getWarmStateName (coldSlot) == std::string ("active"),
           "the cold arm reaches active once the instance is resident");

    const double coldRms = renderRms (session, 16);
    check (coldRms > 0.0 && toDbfs (coldRms) > kMinimumRmsDbfs,
           "the cold patch crossfades in with non-silent audio ("
               + std::to_string (toDbfs (coldRms)) + " dBFS)");
    check (session.getSwitcher().getSlotGain (coldSlot) > 0.99f,
           "the cold patch reaches full crossfade gain");
    check (session.isSlotWarm (0),
           "the previous patch stays warm after the cold switch");
    check (session.getLiveInstrumentCount() <= kInstanceLimit,
           "the cold load stays within the instance bound");

    session.armSlot (0);
    check (session.getWarmStateName (0) == std::string ("active"),
           "returning to the previous patch is immediate, not warming");

    const double returnRms = renderRms (session, 16);
    check (returnRms > 0.0 && toDbfs (returnRms) > kMinimumRmsDbfs,
           "the previous patch plays immediately on return");

    int worstLive = 0;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        if (session.getWarmState (slot) != ihub::SessionWarmState::active)
            session.armSlot (slot);

        if (! session.waitForSlotActive (slot, 5000))
        {
            check (false, "slot " + std::to_string (slot) + " did not reach active");
            break;
        }

        worstLive = juce::jmax (worstLive, session.getLiveInstrumentCount());
    }

    std::cout << "      live instruments: worst=" << worstLive
              << " peak=" << session.getPeakInstrumentCount()
              << " limit=" << kInstanceLimit << std::endl;

    check (worstLive <= kInstanceLimit,
           "the live-instance count stays at or below K+1 across all 64 pads");
    check (session.getPeakInstrumentCount() <= kInstanceLimit,
           "the peak number of coexisting instruments is at most K+1");
    check (session.getPeakInstrumentCount() < kSlots,
           "cold patches are descriptors, not 64 live instances");

    Api::StateStore state (session.getNumSlots());

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;
    options.meterHz = 30;

    Api::Server server (state, options);
    Api::attachSession (state, server, session);

    if (! server.start())
    {
        check (false, "the control API starts");
    }
    else
    {
        httplib::Client client ("127.0.0.1", server.getPort());
        client.set_connection_timeout (2, 0);
        client.set_read_timeout (5, 0);

        const auto response = client.Get ("/state");
        check (response && response->status == 200, "GET /state returns 200");

        if (response && response->status == 200)
        {
            const auto body = juce::JSON::parse (juce::String (response->body));
            const auto slotsVar = property (body, "slots");
            const auto familiesVar = property (body, "families");
            const auto* slots = slotsVar.getArray();
            const auto* families = familiesVar.getArray();

            check (slots != nullptr && slots->size() == kSlots,
                   "/state reports 64 slots");
            check (families != nullptr && families->size() == kFamilies,
                   "/state reports 8 families");

            bool treeOk = families != nullptr && families->size() == kFamilies;

            if (treeOk)
            {
                for (const auto& family : *families)
                {
                    const auto* patches = property (family, "patches").getArray();

                    if (patches == nullptr
                        || patches->size() != kPatchesPerFamily)
                    {
                        treeOk = false;
                        break;
                    }
                }
            }

            check (treeOk, "/state reports 8 patches per family");

            const int armed = static_cast<int> (property (body, "armedSlot"));
            bool warmStateSeen = false;

            if (slots != nullptr)
            {
                for (const auto& slot : *slots)
                {
                    if (static_cast<int> (property (slot, "index")) == armed)
                    {
                        const std::string warmState =
                            property (slot, "warmState").toString().toStdString();
                        warmStateSeen = warmState == "active" || warmState == "warm";
                    }
                }
            }

            check (warmStateSeen, "/state reports the armed slot's warm state");
        }

        server.stop();
    }

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << failures << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
