#include "dsp/tuner/Tuner.h"

#include <algorithm>
#include <cmath>

namespace ihub::dsp
{

    namespace
    {
        constexpr double twoPi = 6.28318530717958647692;
        constexpr float yinThreshold = 0.15f;
        constexpr float fallbackThreshold = 0.35f;
        constexpr float minimumConfidence = 0.35f;
        constexpr float levelDecay = 0.85f;
    }

    Tuner::Tuner ()
    {
        setLatencySamples (0);
    }

    void Tuner::prepareToPlay (double newSampleRate, int)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        decimation = juce::jmax (1, juce::roundToInt (sampleRate / 12000.0));
        decimatedRate = juce::jmax (1, juce::roundToInt (sampleRate / decimation));
        minimumLag =
            juce::jmax (1, static_cast<int> (std::floor (decimatedRate / maximumFrequencyHz)));
        maximumLag = juce::jmin (maximumLagSamples,
                                 static_cast<int> (std::ceil (decimatedRate / minimumFrequencyHz)));
        maximumLag = juce::jmax (minimumLag + 1, maximumLag);

        const float cutoff = juce::jmin (1800.0f, static_cast<float> (decimatedRate) * 0.45f);
        lowPassCoefficient = static_cast<float> (1.0 - std::exp (-twoPi * cutoff / sampleRate));

        resetAnalysis ();
        prepared = true;
    }

    void Tuner::releaseResources ()
    {
        prepared = false;
        resetAnalysis ();
    }

    void Tuner::reset ()
    {
        resetAnalysis ();
    }

    void Tuner::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        const juce::ScopedNoDenormals noDenormals;

        if (!prepared)
            return;

        const int samples = buffer.getNumSamples ();
        const int channels = buffer.getNumChannels ();

        if (samples <= 0 || channels <= 0)
            return;

        const auto* data = buffer.getReadPointer (0);

        float peak = 0.0f;

        for (int i = 0; i < samples; ++i)
            peak = juce::jmax (peak, std::abs (data[i]));

        peakState = juce::jmax (peak, peakState * levelDecay);
        levelDb.store (juce::Decibels::gainToDecibels (peakState, -120.0f),
                       std::memory_order_relaxed);

        for (int i = 0; i < samples; ++i)
        {
            lowPassState += lowPassCoefficient * (data[i] - lowPassState);

            if (++decimationCounter < decimation)
                continue;

            decimationCounter = 0;
            ring[static_cast<std::size_t> (ringWrite)] = lowPassState;
            ringWrite = (ringWrite + 1) % ringSize;

            if (++samplesSinceAnalysis < analysisHop)
                continue;

            samplesSinceAnalysis = 0;
            analyse ();
        }
    }

    bool Tuner::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto input = layouts.getMainInputChannelSet ();
        const auto output = layouts.getMainOutputChannelSet ();

        if (input != output)
            return false;

        return input == juce::AudioChannelSet::mono () || input == juce::AudioChannelSet::stereo ();
    }

    void Tuner::setReferenceHz (float hz)
    {
        referenceHz.store (juce::jlimit (minReferenceHz, maxReferenceHz, hz),
                           std::memory_order_relaxed);
    }

    float Tuner::getReferenceHz () const noexcept
    {
        return referenceHz.load (std::memory_order_relaxed);
    }

    TunerReading Tuner::getReading () const noexcept
    {
        TunerReading reading;
        reading.valid = valid.load (std::memory_order_relaxed);
        reading.frequencyHz = frequencyHz.load (std::memory_order_relaxed);
        reading.midiNote = midiNote.load (std::memory_order_relaxed);
        reading.cents = cents.load (std::memory_order_relaxed);
        reading.confidence = confidence.load (std::memory_order_relaxed);
        reading.levelDb = levelDb.load (std::memory_order_relaxed);
        return reading;
    }

    void Tuner::resetAnalysis () noexcept
    {
        ring.fill (0.0f);
        ringWrite = 0;
        decimationCounter = 0;
        samplesSinceAnalysis = 0;
        lowPassState = 0.0f;
        peakState = 0.0f;
        analysis.fill (0.0f);
        difference.fill (0.0f);
        normalized.fill (0.0f);
        frequencyHistory.fill (0.0f);
        historyCount = 0;
        invalidHops = 0;
        levelDb.store (-120.0f, std::memory_order_relaxed);
        publishInvalid ();
    }

    void Tuner::analyse () noexcept
    {
        if (levelDb.load (std::memory_order_relaxed) < levelGateDb)
        {
            if (++invalidHops >= invalidHopsBeforePublish)
                publishInvalid ();

            return;
        }

        const int count = windowSize + maximumLag;
        int read = ringWrite - count;

        while (read < 0)
            read += ringSize;

        for (int i = 0; i < count; ++i)
        {
            analysis[static_cast<std::size_t> (i)] = ring[static_cast<std::size_t> (read)];
            read = (read + 1) % ringSize;
        }

        for (int lag = 1; lag <= maximumLag; ++lag)
        {
            const auto* first = analysis.data ();
            const auto* second = first + lag;
            float sum = 0.0f;

            for (int i = 0; i < windowSize; ++i)
            {
                const float delta = first[i] - second[i];
                sum += delta * delta;
            }

            difference[static_cast<std::size_t> (lag)] = sum;
        }

        float running = 0.0f;
        normalized[0] = 1.0f;

        for (int lag = 1; lag <= maximumLag; ++lag)
        {
            running += difference[static_cast<std::size_t> (lag)];
            normalized[static_cast<std::size_t> (lag)] =
                running > 1.0e-12f ? difference[static_cast<std::size_t> (lag)] *
                                         static_cast<float> (lag) / running
                                   : 1.0f;
        }

        int tau = -1;

        for (int lag = minimumLag; lag <= maximumLag; ++lag)
        {
            if (normalized[static_cast<std::size_t> (lag)] < yinThreshold)
            {
                tau = lag;
                break;
            }
        }

        if (tau < 0)
        {
            float best = 1.0f;
            int bestLag = -1;

            for (int lag = minimumLag; lag <= maximumLag; ++lag)
            {
                const float value = normalized[static_cast<std::size_t> (lag)];

                if (value < best)
                {
                    best = value;
                    bestLag = lag;
                }
            }

            if (best < fallbackThreshold)
                tau = bestLag;
        }

        if (tau < 0)
        {
            if (++invalidHops >= invalidHopsBeforePublish)
                publishInvalid ();

            return;
        }

        while (tau < maximumLag && normalized[static_cast<std::size_t> (tau + 1)] <
                                       normalized[static_cast<std::size_t> (tau)])
            ++tau;

        const float confidenceValue =
            juce::jlimit (0.0f, 1.0f, 1.0f - normalized[static_cast<std::size_t> (tau)]);

        if (confidenceValue < minimumConfidence)
        {
            if (++invalidHops >= invalidHopsBeforePublish)
                publishInvalid ();

            return;
        }

        float betterTau = static_cast<float> (tau);

        if (tau > 1 && tau < maximumLag)
        {
            const float previous = normalized[static_cast<std::size_t> (tau - 1)];
            const float current = normalized[static_cast<std::size_t> (tau)];
            const float next = normalized[static_cast<std::size_t> (tau + 1)];
            const float denominator = 2.0f * (2.0f * current - previous - next);

            if (std::abs (denominator) > 1.0e-9f)
                betterTau = static_cast<float> (tau) + (next - previous) / denominator;
        }

        if (betterTau <= 0.0f)
        {
            if (++invalidHops >= invalidHopsBeforePublish)
                publishInvalid ();

            return;
        }

        const float frequency = static_cast<float> (decimatedRate) / betterTau;

        if (frequency < minimumFrequencyHz || frequency > maximumFrequencyHz)
        {
            if (++invalidHops >= invalidHopsBeforePublish)
                publishInvalid ();

            return;
        }

        if (historyCount < 3)
        {
            frequencyHistory[static_cast<std::size_t> (historyCount++)] = frequency;
        }
        else
        {
            frequencyHistory[0] = frequencyHistory[1];
            frequencyHistory[1] = frequencyHistory[2];
            frequencyHistory[2] = frequency;
        }

        float smoothedFrequency = frequency;

        if (historyCount >= 3)
        {
            auto sorted = frequencyHistory;
            std::sort (sorted.begin (), sorted.end ());
            smoothedFrequency = sorted[1];
        }

        TunerReading reading;
        reading.valid = true;
        reading.frequencyHz = smoothedFrequency;
        reading.confidence = confidenceValue;
        reading.levelDb = levelDb.load (std::memory_order_relaxed);
        noteForFrequency (smoothedFrequency,
                          referenceHz.load (std::memory_order_relaxed),
                          reading.midiNote,
                          reading.cents);

        if (reading.midiNote < 0)
        {
            if (++invalidHops >= invalidHopsBeforePublish)
                publishInvalid ();

            return;
        }

        invalidHops = 0;
        publish (reading);
    }

    void Tuner::publish (const TunerReading& reading) noexcept
    {
        valid.store (reading.valid, std::memory_order_relaxed);
        frequencyHz.store (reading.frequencyHz, std::memory_order_relaxed);
        midiNote.store (reading.midiNote, std::memory_order_relaxed);
        cents.store (reading.cents, std::memory_order_relaxed);
        confidence.store (reading.confidence, std::memory_order_relaxed);
        levelDb.store (reading.levelDb, std::memory_order_relaxed);
    }

    void Tuner::publishInvalid () noexcept
    {
        valid.store (false, std::memory_order_relaxed);
        frequencyHz.store (0.0f, std::memory_order_relaxed);
        midiNote.store (-1, std::memory_order_relaxed);
        cents.store (0.0f, std::memory_order_relaxed);
        confidence.store (0.0f, std::memory_order_relaxed);
        frequencyHistory.fill (0.0f);
        historyCount = 0;
        invalidHops = 0;
    }

    void Tuner::noteForFrequency (float frequency,
                                  float reference,
                                  int& note,
                                  float& offsetCents) noexcept
    {
        note = -1;
        offsetCents = 0.0f;

        if (frequency <= 0.0f || reference <= 0.0f)
            return;

        const float midi = 69.0f + 12.0f * std::log2 (frequency / reference);
        const int nearest = static_cast<int> (std::lround (midi));

        if (nearest < 0 || nearest > 127)
            return;

        note = nearest;
        offsetCents = (midi - static_cast<float> (nearest)) * 100.0f;
    }

    void Tuner::getStateInformation (juce::MemoryBlock& destData)
    {
        juce::MemoryOutputStream stream (destData, false);
        stream.writeInt (1);
        stream.writeFloat (getReferenceHz ());
    }

    void Tuner::setStateInformation (const void* data, int sizeInBytes)
    {
        if (data == nullptr || sizeInBytes < 8)
            return;

        juce::MemoryInputStream stream (data, static_cast<std::size_t> (sizeInBytes), false);

        if (stream.readInt () != 1)
            return;

        setReferenceHz (stream.readFloat ());
    }

}
