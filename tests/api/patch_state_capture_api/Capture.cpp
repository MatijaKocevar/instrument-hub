#include "Capture.h"

#include "Fixtures.h"

#include "../JsonSupport.h"
#include "../RequestDispatch.h"
#include "../SessionRig.h"
#include "../TestSupport.h"

#include <string>

namespace ApiTest::PatchStateCaptureApi
{

bool runCapture (const juce::File& stateDirectory, const juce::String& bundlePath)
{
    SessionRig rig (makeConfig (bundlePath), 20, "patch-state-capture-api-test",
                    stateDirectory);

    if (! rig.started())
        return false;

    auto& session = rig.session();
    auto& client = rig.client();

    const auto stateFile = stateDirectory.getChildFile ("slot-0.bin");

    auto response = postWithDispatch (client, "/arm", "{\"slot\":0}");
    check (response && response->status == 200, "POST /arm 0 reaches the engine");
    check (session.waitForSlotActive (kPluginSlot, 15000),
           "the plugin patch warms and becomes active");

    applyPreset (session, kPluginSlot, kFrequencyA, kStateMarkerA);

    response = postWithDispatch (client, "/patches/0/state/capture", "{}");
    check (response && response->status == 200,
           "POST /patches/0/state/capture returns 200");

    auto body = response ? parseJson (response->body) : juce::var {};

    const auto sizeA = static_cast<juce::int64> (property (body, "sizeBytes"));

    check (intProperty (body, "slot") == kPluginSlot,
           "the capture answer names the patch slot");
    check (boolProperty (body, "captured"),
           "the capture answer confirms the sound was saved");
    check (sizeA > 0, "the capture answer reports a non-empty state blob");
    check (session.hasPluginState (kPluginSlot)
               && session.getPluginStateSize (kPluginSlot) == sizeA,
           "the session holds exactly the reported blob");
    check (stateFile.existsAsFile() && stateFile.getSize() > 0,
           "the explicit save wrote the sidecar without switching away");
    check (stateFile.getSize() > sizeA,
           "the sidecar wraps the reported blob with its identity header");

    const auto bytesA = readBytes (stateFile);

    applyPreset (session, kPluginSlot, kFrequencyB, kStateMarkerB);

    response = postWithDispatch (client, "/patches/0/state/capture", "{}");
    body = response ? parseJson (response->body) : juce::var {};
    const auto sizeB = static_cast<juce::int64> (property (body, "sizeBytes"));

    check (response && response->status == 200 && sizeB > 0,
           "a second, different preset saves in place");

    const auto bytesB = readBytes (stateFile);

    check (bytesA != bytesB, "the saved state changed with the preset");

    response = postWithDispatch (client, "/patches/0/state/capture", "{}");
    body = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200
               && boolProperty (body, "captured"),
           "saving the same sound again is an idempotent confirmation");
    check (readBytes (stateFile) == bytesB,
           "an unchanged save leaves the stored blob intact");

    response = postWithDispatch (client, "/patches/1/state/capture", "{}");
    check (response && response->status == 400,
           "a generated patch rejects an explicit save");
    check (response
               && response->body.find ("resident plugin") != std::string::npos,
           "the rejection explains that there is no resident plugin");

    response = postWithDispatch (client, "/patches/99/state/capture", "{}");
    check (response && response->status == 404,
           "an out-of-range slot is rejected");

    response = postWithDispatch (client, "/arm", "{\"slot\":1}");
    check (response && response->status == 200 && session.waitForSlotActive (kToneSlot, 5000),
           "switching to the tone patch succeeds");

    response = postWithDispatch (client, "/arm", "{\"slot\":0}");
    check (response && response->status == 200 && session.waitForSlotActive (kPluginSlot, 15000),
           "switching back arms the plugin patch");

    const auto markerAfterRoundTrip = stateMarker (session, kPluginSlot);
    const auto pitchAfterRoundTrip = measurePitch (session);

    check (approximately (markerAfterRoundTrip, kStateMarkerB, 0.01),
           "switching away and back restores the explicitly saved marker");
    check (approximately (pitchAfterRoundTrip, kFrequencyB, 40.0),
           "switching away and back restores the explicitly saved sound ("
               + std::to_string (pitchAfterRoundTrip) + " Hz)");

    rig.stop();
    return true;
}

}
