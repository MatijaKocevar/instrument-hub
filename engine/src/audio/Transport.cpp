#include "Transport.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace AudioEngine
{

    namespace
    {
        double sanitisePpq (double value) noexcept
        {
            return std::isfinite (value) && value > 0.0 ? value : 0.0;
        }

        std::int64_t clampPosition (std::int64_t value) noexcept
        {
            return value > 0 ? value : 0;
        }
    }

    void Transport::prepare (double newSampleRate, int newMaximumBlockSize) noexcept
    {
        const bool validRate = std::isfinite (newSampleRate) && newSampleRate > 0.0;

        sampleRate.store (validRate ? newSampleRate : 0.0, std::memory_order_relaxed);
        maximumBlockSize.store (newMaximumBlockSize > 0 ? newMaximumBlockSize : 0,
                                std::memory_order_relaxed);
    }

    void Transport::reset () noexcept
    {
        playing.store (false, std::memory_order_relaxed);
        bpm.store (defaultBpm, std::memory_order_relaxed);
        ppq.store (0.0, std::memory_order_relaxed);
        positionSamples.store (0, std::memory_order_relaxed);

        syncSource.store (SyncSource::internal, std::memory_order_relaxed);
        externalClockLocked.store (false, std::memory_order_relaxed);

        resetTapTempo ();
    }

    void Transport::rewind () noexcept
    {
        ppq.store (0.0, std::memory_order_relaxed);
        positionSamples.store (0, std::memory_order_relaxed);
    }

    void Transport::start () noexcept
    {
        playing.store (true, std::memory_order_relaxed);
    }

    void Transport::stop () noexcept
    {
        playing.store (false, std::memory_order_relaxed);
    }

    void Transport::setPlaying (bool shouldPlay) noexcept
    {
        playing.store (shouldPlay, std::memory_order_relaxed);
    }

    bool Transport::isPlaying () const noexcept
    {
        return playing.load (std::memory_order_relaxed);
    }

    void Transport::setBpm (double newBpm) noexcept
    {
        if (!std::isfinite (newBpm))
            return;

        bpm.store (clampBpm (newBpm), std::memory_order_relaxed);
    }

    double Transport::getBpm () const noexcept
    {
        return bpm.load (std::memory_order_relaxed);
    }

    void Transport::setPpq (double newPpq) noexcept
    {
        const double position = sanitisePpq (newPpq);
        ppq.store (position, std::memory_order_relaxed);

        const double rate = sampleRate.load (std::memory_order_relaxed);
        const double tempo = bpm.load (std::memory_order_relaxed);

        if (rate > 0.0 && tempo > 0.0)
        {
            const double frames = position * 60.0 / tempo * rate;

            if (std::isfinite (frames) &&
                frames <= static_cast<double> (std::numeric_limits<std::int64_t>::max ()))
            {
                positionSamples.store (static_cast<std::int64_t> (std::llround (frames)),
                                       std::memory_order_relaxed);
            }
        }
    }

    void Transport::setPositionSamples (std::int64_t newPositionSamples) noexcept
    {
        const auto position = clampPosition (newPositionSamples);
        positionSamples.store (position, std::memory_order_relaxed);

        const double rate = sampleRate.load (std::memory_order_relaxed);
        const double tempo = bpm.load (std::memory_order_relaxed);

        if (rate > 0.0 && tempo > 0.0)
        {
            ppq.store (static_cast<double> (position) * tempo / (60.0 * rate),
                       std::memory_order_relaxed);
        }
    }

    double Transport::getPpq () const noexcept
    {
        return ppq.load (std::memory_order_relaxed);
    }

    std::int64_t Transport::getPositionSamples () const noexcept
    {
        return positionSamples.load (std::memory_order_relaxed);
    }

    double Transport::getSampleRate () const noexcept
    {
        return sampleRate.load (std::memory_order_relaxed);
    }

    int Transport::getMaximumBlockSize () const noexcept
    {
        return maximumBlockSize.load (std::memory_order_relaxed);
    }

    void Transport::process (int numSamples) noexcept
    {
        if (numSamples <= 0 || !playing.load (std::memory_order_relaxed))
            return;

        const auto frames = positionSamples.load (std::memory_order_relaxed);

        if (frames <= std::numeric_limits<std::int64_t>::max () - numSamples)
            positionSamples.store (frames + static_cast<std::int64_t> (numSamples),
                                   std::memory_order_relaxed);

        const double rate = sampleRate.load (std::memory_order_relaxed);

        if (!(rate > 0.0))
            return;

        const double quarterNotesPerFrame = bpm.load (std::memory_order_relaxed) / (60.0 * rate);

        ppq.store (ppq.load (std::memory_order_relaxed) +
                       static_cast<double> (numSamples) * quarterNotesPerFrame,
                   std::memory_order_relaxed);
    }

    double Transport::getSamplesPerQuarterNote () const noexcept
    {
        const double rate = sampleRate.load (std::memory_order_relaxed);
        const double tempo = bpm.load (std::memory_order_relaxed);

        return rate > 0.0 && tempo > 0.0 ? 60.0 * rate / tempo : 0.0;
    }

    double Transport::getPpqAtSampleOffset (int offsetSamples) const noexcept
    {
        const double current = ppq.load (std::memory_order_relaxed);

        if (offsetSamples <= 0 || !playing.load (std::memory_order_relaxed))
            return current;

        const double rate = sampleRate.load (std::memory_order_relaxed);

        if (!(rate > 0.0))
            return current;

        const double quarterNotesPerFrame = bpm.load (std::memory_order_relaxed) / (60.0 * rate);

        return current + static_cast<double> (offsetSamples) * quarterNotesPerFrame;
    }

    double Transport::tapAt (double timestampSeconds) noexcept
    {
        const double estimate = tapTempo.tapAt (timestampSeconds);

        if (std::isfinite (timestampSeconds) && estimate > 0.0 &&
            syncSource.load (std::memory_order_relaxed) == SyncSource::internal)
        {
            bpm.store (estimate, std::memory_order_relaxed);
        }

        return estimate;
    }

    double Transport::tap () noexcept
    {
        const double estimate = tapTempo.tap ();

        if (estimate > 0.0 && syncSource.load (std::memory_order_relaxed) == SyncSource::internal)
        {
            bpm.store (estimate, std::memory_order_relaxed);
        }

        return estimate;
    }

    void Transport::resetTapTempo () noexcept
    {
        tapTempo.reset ();
    }

    double Transport::getTapTempoBpm () const noexcept
    {
        return tapTempo.getBpm ();
    }

    void Transport::setSyncSource (SyncSource newSource) noexcept
    {
        syncSource.store (newSource, std::memory_order_relaxed);

        if (newSource == SyncSource::internal)
            externalClockLocked.store (false, std::memory_order_relaxed);
    }

    Transport::SyncSource Transport::getSyncSource () const noexcept
    {
        return syncSource.load (std::memory_order_relaxed);
    }

    bool Transport::applyExternalSync (const ExternalSync& sync) noexcept
    {
        if (syncSource.load (std::memory_order_relaxed) != SyncSource::external)
            return false;

        externalClockLocked.store (sync.locked, std::memory_order_relaxed);

        if (!sync.locked)
        {
            playing.store (false, std::memory_order_relaxed);
            return true;
        }

        if (std::isfinite (sync.bpm))
            bpm.store (clampBpm (sync.bpm), std::memory_order_relaxed);

        playing.store (sync.playing, std::memory_order_relaxed);
        ppq.store (sanitisePpq (sync.ppq), std::memory_order_relaxed);
        positionSamples.store (clampPosition (sync.positionSamples), std::memory_order_relaxed);

        return true;
    }

    bool Transport::isExternalClockLocked () const noexcept
    {
        return externalClockLocked.load (std::memory_order_relaxed);
    }

    Transport::Snapshot Transport::snapshot () const noexcept
    {
        Snapshot result;

        result.playing = playing.load (std::memory_order_relaxed);
        result.bpm = bpm.load (std::memory_order_relaxed);
        result.ppq = ppq.load (std::memory_order_relaxed);
        result.positionSamples = positionSamples.load (std::memory_order_relaxed);

        result.sampleRate = sampleRate.load (std::memory_order_relaxed);
        result.maximumBlockSize = maximumBlockSize.load (std::memory_order_relaxed);

        result.syncSource = syncSource.load (std::memory_order_relaxed);
        result.externalClockLocked = externalClockLocked.load (std::memory_order_relaxed);

        result.tapTempoBpm = tapTempo.getBpm ();

        return result;
    }

    double Transport::clampBpm (double tempo) noexcept
    {
        return std::clamp (tempo, minBpm, maxBpm);
    }

}
