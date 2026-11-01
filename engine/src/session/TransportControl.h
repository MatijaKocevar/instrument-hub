#pragma once

#include "audio/FxRack.h"
#include "audio/Transport.h"

namespace ihub::session
{

/**
    Owns the session transport and the adapter that publishes tempo to the
    tempo-synced insert effects.

    `Session` remains the façade: `getTransport()` returns the reference this
    class owns, and the render path forwards its block to `process()`. The
    transport itself is lock-free on the audio thread.
*/
class TransportControl
{
public:
    void prepare (double sampleRate, int maximumBlockSize) noexcept
    {
        transport.prepare (sampleRate, maximumBlockSize);
    }

    void reset() noexcept { transport.reset(); }
    void rewind() noexcept { transport.rewind(); }
    void process (int numSamples) noexcept { transport.process (numSamples); }

    AudioEngine::Transport& getTransport() noexcept { return transport; }
    const AudioEngine::Transport& getTransport() const noexcept { return transport; }

    /** Snapshot in the subset an `FxRack` consumes. */
    AudioEngine::FxTransport getFxTransport() const noexcept;

private:
    AudioEngine::Transport transport;
};

} // namespace ihub::session
