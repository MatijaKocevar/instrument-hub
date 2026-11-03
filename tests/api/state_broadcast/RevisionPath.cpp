#include "RevisionPath.h"

#include "../EventStream.h"
#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <cstdint>
#include <string>

namespace ApiTest::StateBroadcast
{

namespace
{
bool hasEvent (const juce::var& frame, const char* kind, int slot)
{
    return isEvent (frame, kind)
           && intProperty (property (frame, "event"), "slot") == slot;
}
std::uint64_t revisionOf (const juce::var& frame)
{
    return static_cast<std::uint64_t> (numberProperty (frame, "revision"));
}
}

void runRevisionPath (Rig& rig)
{
    auto& desktop = *rig.desktop;

    httplib::Client http ("127.0.0.1", rig.server.getPort());
    http.set_connection_timeout (2, 0);
    http.set_read_timeout (2, 0);

    const auto before = rig.state.getRevision();
    const auto response = http.Post ("/arm", "{\"slot\":4}", "application/json");

    check (response && response->status == 200, "POST /arm returns 200");

    if (! response || response->status != 200)
        return;

    const auto body = parseJson (response->body);
    const auto responseRevision = revisionOf (body);

    check (responseRevision >= before,
           "GET /state carries a monotonic control-state revision");
    check (responseRevision == rig.state.getRevision(),
           "the arm response carries the current revision");

    juce::var armFrame;
    const auto armFound = readEventUntil (desktop.socket, [&] (const juce::var& candidate)
    {
        return hasEvent (candidate, "arm", 4);
    }, 1000, armFrame);

    check (armFound, "the arm event frame carries the new revision");

    if (armFound)
    {
        check (revisionOf (armFrame) == responseRevision,
               "the arm frame revision matches the snapshot it embeds");
        check (revisionOf (property (armFrame, "state")) == responseRevision,
               "the embedded state carries the same revision");
    }

    const auto warmBefore = rig.state.getRevision();
    rig.state.notifyChanged ("warm", 2);

    juce::var warmFrame;
    const auto warmFound = readEventUntil (desktop.socket, [&] (const juce::var& candidate)
    {
        return isEvent (candidate, "warm");
    }, 1000, warmFrame);

    check (warmFound, "a warm-state change is published");
    check (rig.state.getRevision() == warmBefore + 1,
           "a warm-state change advances the revision");
    check (warmFound && revisionOf (warmFrame) == rig.state.getRevision(),
           "the warm frame carries the advanced revision");

    const auto deviceBefore = rig.state.getRevision();
    rig.state.notifyChanged ("device", -1);

    juce::var deviceFrame;
    const auto deviceFound = readEventUntil (desktop.socket, [&] (const juce::var& candidate)
    {
        return isEvent (candidate, "device");
    }, 1000, deviceFrame);

    check (deviceFound, "a device change is published");
    check (deviceFound && revisionOf (deviceFrame) == deviceBefore + 1,
           "the device frame advances the revision");

    const auto padRevision = rig.state.getRevision();
    rig.server.getStateBroadcast().publishPad (3, true);

    juce::var padFrame;
    const auto padFound = readEventUntil (desktop.socket, [&] (const juce::var& candidate)
    {
        return isEvent (candidate, "pad")
               && intProperty (property (candidate, "event"), "pad") == 3;
    }, 1000, padFrame);

    check (padFound, "a pad event is still published");
    check (padFound && revisionOf (padFrame) == padRevision,
           "pad events reuse the state revision");
}

}
