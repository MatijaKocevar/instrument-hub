#pragma once

#include "audio/FxRack.h"
#include "audio/Transport.h"

namespace ihub::session
{

    class TransportControl
    {
    public:
        void prepare (double sampleRate, int maximumBlockSize) noexcept
        {
            transport.prepare (sampleRate, maximumBlockSize);
        }

        void reset () noexcept
        {
            transport.reset ();
        }
        void rewind () noexcept
        {
            transport.rewind ();
        }
        void process (int numSamples) noexcept
        {
            transport.process (numSamples);
        }

        AudioEngine::Transport& getTransport () noexcept
        {
            return transport;
        }
        const AudioEngine::Transport& getTransport () const noexcept
        {
            return transport;
        }

        AudioEngine::FxTransport getFxTransport () const noexcept;

    private:
        AudioEngine::Transport transport;
    };

}
