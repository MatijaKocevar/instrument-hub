#pragma once

#include <juce_core/juce_core.h>

#include <memory>

namespace ihub
{
class Session;
}

namespace ihub::app
{

/** Builds and opens the composed audio session for the normal app and
    `--serve`.

    Returns null when `--no-audio` disabled audio or the graph could not be
    built; a build failure has already been reported on stderr. When the
    device cannot be opened the session is returned anyway, exactly as the
    application always did: the API still serves state and the user can fix
    the device later.
*/
std::unique_ptr<ihub::Session> startSession (const juce::String& commandLine);

} // namespace ihub::app
