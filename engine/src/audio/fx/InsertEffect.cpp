#include "audio/fx/InsertEffect.h"

#include <utility>

namespace AudioEngine
{

    InsertEffect::InsertEffect (std::unique_ptr<juce::AudioProcessor> processorToWrap,
                                FxInsertType typeToUse,
                                std::string idToUse,
                                std::string nameToUse)
        : juce::AudioProcessor (BusesProperties ()
                                    .withInput ("Input", juce::AudioChannelSet::stereo (), true)
                                    .withOutput ("Output", juce::AudioChannelSet::stereo (), true)),
          processor (std::move (processorToWrap)), type (typeToUse), id (std::move (idToUse)),
          effectName (nameToUse.empty () && processor != nullptr ? processor->getName ()
                                                                 : juce::String (nameToUse))
    {
        jassert (processor != nullptr);

        if (processor != nullptr)
        {
            const int latency = juce::jmax (0, processor->getLatencySamples ());
            cachedLatency.store (latency, std::memory_order_relaxed);
            setLatencySamples (latency);
        }
    }

    void InsertEffect::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
    {
        if (processor == nullptr)
            return;

        maximumBlockSize = juce::jmax (1, maximumExpectedSamplesPerBlock);
        numChannels = juce::jmax (1, getTotalNumOutputChannels ());

        dry.setSize (numChannels, maximumBlockSize, false, true, true);
        dry.clear ();

        const auto target = juce::AudioChannelSet::canonicalChannelSet (numChannels);
        auto layout = processor->getBusesLayout ();
        bool changed = false;

        if (!layout.inputBuses.isEmpty () && layout.inputBuses.getReference (0) != target)
        {
            layout.inputBuses.getReference (0) = target;
            changed = true;
        }

        if (!layout.outputBuses.isEmpty () && layout.outputBuses.getReference (0) != target)
        {
            layout.outputBuses.getReference (0) = target;
            changed = true;
        }

        if (changed && !processor->setBusesLayout (layout))
        {
            auto fallback = processor->getBusesLayout ();

            if (!fallback.outputBuses.isEmpty ())
                fallback.outputBuses.getReference (0) = target;

            processor->setBusesLayout (fallback);
        }

        processor->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);
        processor->prepareToPlay (sampleRate, maximumBlockSize);

        refreshLatency ();
    }

    void InsertEffect::releaseResources ()
    {
        if (processor != nullptr)
            processor->releaseResources ();

        cachedLatency.store (0, std::memory_order_relaxed);
    }

    void InsertEffect::reset ()
    {
        if (processor != nullptr)
            processor->reset ();

        dry.clear ();
    }

    void InsertEffect::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        if (processor == nullptr)
            return;

        if (type == FxInsertType::tuner)
        {
            processor->processBlock (buffer, midi);
            return;
        }

        if (bypassed.load (std::memory_order_relaxed))
            return;

        const float wetAmount = juce::jlimit (0.0f, 1.0f, wet.load (std::memory_order_relaxed));

        if (wetAmount <= 0.0f)
            return;

        const int numSamples = buffer.getNumSamples ();
        const int bufferChannels = buffer.getNumChannels ();

        if (numSamples <= 0 || bufferChannels <= 0)
            return;

        const int innerChannels = juce::jmax (processor->getTotalNumInputChannels (),
                                              processor->getTotalNumOutputChannels ());

        if (innerChannels <= 0 || innerChannels > bufferChannels ||
            numSamples > dry.getNumSamples () || bufferChannels > dry.getNumChannels ())
            return;

        const int dryChannels = juce::jmin (bufferChannels, dry.getNumChannels ());

        for (int channel = 0; channel < dryChannels; ++channel)
            dry.copyFrom (channel, 0, buffer, channel, 0, numSamples);

        if (innerChannels == bufferChannels)
        {
            processor->processBlock (buffer, midi);
        }
        else
        {
            juce::AudioBuffer<float> view (
                buffer.getArrayOfWritePointers (), innerChannels, numSamples);
            processor->processBlock (view, midi);
        }

        if (wetAmount >= 1.0f)
            return;

        const float dryAmount = 1.0f - wetAmount;

        for (int channel = 0; channel < dryChannels; ++channel)
        {
            auto* output = buffer.getWritePointer (channel);
            const auto* drySamples = dry.getReadPointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
                output[sample] = drySamples[sample] * dryAmount + output[sample] * wetAmount;
        }
    }

    bool InsertEffect::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto output = layouts.getMainOutputChannelSet ();

        return output == juce::AudioChannelSet::mono () ||
               output == juce::AudioChannelSet::stereo ();
    }

    double InsertEffect::getTailLengthSeconds () const
    {
        return processor != nullptr ? processor->getTailLengthSeconds () : 0.0;
    }

    void InsertEffect::getStateInformation (juce::MemoryBlock& destData)
    {
        if (processor != nullptr)
            processor->getStateInformation (destData);
    }

    void InsertEffect::setStateInformation (const void* data, int sizeInBytes)
    {
        if (processor != nullptr)
            processor->setStateInformation (data, sizeInBytes);
    }

    void InsertEffect::setBypassed (bool shouldBeBypassed) noexcept
    {
        bypassed.store (shouldBeBypassed, std::memory_order_relaxed);
    }

    bool InsertEffect::isBypassed () const noexcept
    {
        return bypassed.load (std::memory_order_relaxed);
    }

    void InsertEffect::setWet (float wetAmount) noexcept
    {
        wet.store (juce::jlimit (0.0f, 1.0f, wetAmount), std::memory_order_relaxed);
    }

    float InsertEffect::getWet () const noexcept
    {
        return wet.load (std::memory_order_relaxed);
    }

}
