#include "dsp/drive/Drive.h"

#include <algorithm>
#include <cmath>

namespace ihub::dsp
{

    Drive::Drive ()
    {
        setLatencySamples (0);
    }

    void Drive::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        maximumBlockSize = juce::jmax (1, maximumExpectedSamplesPerBlock);
        numChannels = juce::jlimit (1, 2, getTotalNumOutputChannels ());

        filters.prepare (currentSampleRate, numChannels);

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            static_cast<size_t> (numChannels),
            2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true);
        oversampling->initProcessing (static_cast<size_t> (maximumBlockSize));

        const auto settings = getSettings ();
        appliedModel = settings.model;
        filters.setVoicing (voicingFor (static_cast<DriveModel> (appliedModel)));
        filters.setTone (settings.tone);

        driveGain.reset (currentSampleRate, 0.02);
        levelGain.reset (currentSampleRate, 0.02);
        cabMix.reset (currentSampleRate, 0.02);
        modelFade.reset (currentSampleRate, 0.005);

        driveGain.setCurrentAndTargetValue (gainForDecibels (driveDbFor (settings)));
        levelGain.setCurrentAndTargetValue (gainForDecibels (settings.levelDb));
        cabMix.setCurrentAndTargetValue (settings.cab ? 1.0f : 0.0f);
        modelFade.setCurrentAndTargetValue (1.0f);

        setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples ()));
        prepared = true;
    }

    void Drive::releaseResources ()
    {
        prepared = false;
        oversampling.reset ();
        setLatencySamples (0);
        filters.reset ();
    }

    void Drive::reset ()
    {
        if (oversampling != nullptr)
            oversampling->reset ();

        filters.reset ();

        const auto settings = getSettings ();
        appliedModel = settings.model;
        filters.setVoicing (voicingFor (static_cast<DriveModel> (appliedModel)));
        filters.setTone (settings.tone);

        driveGain.setCurrentAndTargetValue (gainForDecibels (driveDbFor (settings)));
        levelGain.setCurrentAndTargetValue (gainForDecibels (settings.levelDb));
        cabMix.setCurrentAndTargetValue (settings.cab ? 1.0f : 0.0f);
        modelFade.setCurrentAndTargetValue (1.0f);
    }

    void Drive::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        const juce::ScopedNoDenormals noDenormals;

        const int samples = buffer.getNumSamples ();
        const int bufferChannels = buffer.getNumChannels ();

        if (samples <= 0 || bufferChannels <= 0)
            return;

        if (!prepared || oversampling == nullptr)
        {
            buffer.clear ();
            return;
        }

        const int channels = juce::jmin (bufferChannels, numChannels);

        if (channels != numChannels)
        {
            buffer.clear ();
            return;
        }

        const auto settings = getSettings ();

        if (settings.model != appliedModel && modelFade.getTargetValue () > 0.0f)
            modelFade.setTargetValue (0.0f);

        updateTargets (settings);

        auto block = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (
            0, static_cast<size_t> (channels));

        for (int sample = 0; sample < samples; ++sample)
        {
            const float gain = driveGain.getNextValue ();

            for (int channel = 0; channel < channels; ++channel)
            {
                auto* data = block.getChannelPointer (static_cast<size_t> (channel));
                data[sample] = filters.processPre (channel, data[sample]) * gain;
            }
        }

        const auto upsampled = oversampling->processSamplesUp (block);
        const auto driveModel = static_cast<DriveModel> (appliedModel);

        for (size_t channel = 0; channel < upsampled.getNumChannels (); ++channel)
        {
            auto* data = upsampled.getChannelPointer (channel);

            for (size_t sample = 0; sample < upsampled.getNumSamples (); ++sample)
                data[sample] = shapeDrive (data[sample], driveModel);
        }

        oversampling->processSamplesDown (block);

        for (int sample = 0; sample < samples; ++sample)
        {
            const float level = levelGain.getNextValue ();
            const float cabAmount = cabMix.getNextValue ();
            const float fade = modelFade.getNextValue ();

            for (int channel = 0; channel < channels; ++channel)
            {
                auto* data = block.getChannelPointer (static_cast<size_t> (channel));
                float value = filters.processDc (channel, data[sample]);
                value = filters.processTone (channel, value);
                value += (filters.processCab (channel, value) - value) * cabAmount;
                data[sample] = value * level * fade;
            }
        }

        for (int channel = channels; channel < bufferChannels; ++channel)
            buffer.clear (channel, 0, samples);

        finishModelSwitch (settings);
    }

    bool Drive::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto input = layouts.getMainInputChannelSet ();
        const auto output = layouts.getMainOutputChannelSet ();

        if (input != output)
            return false;

        return input == juce::AudioChannelSet::mono () || input == juce::AudioChannelSet::stereo ();
    }

    void Drive::setSettings (const DriveSettings& settings)
    {
        const auto clamped = clampSettings (settings);

        model.store (clamped.model, std::memory_order_relaxed);
        drive.store (clamped.drive, std::memory_order_relaxed);
        tone.store (clamped.tone, std::memory_order_relaxed);
        levelDb.store (clamped.levelDb, std::memory_order_relaxed);
        cab.store (clamped.cab, std::memory_order_relaxed);
    }

    DriveSettings Drive::getSettings () const noexcept
    {
        DriveSettings settings;
        settings.model = model.load (std::memory_order_relaxed);
        settings.drive = drive.load (std::memory_order_relaxed);
        settings.tone = tone.load (std::memory_order_relaxed);
        settings.levelDb = levelDb.load (std::memory_order_relaxed);
        settings.cab = cab.load (std::memory_order_relaxed);
        return clampSettings (settings);
    }

    void Drive::setModel (int newModel)
    {
        model.store (juce::jlimit (0, numModels - 1, newModel), std::memory_order_relaxed);
    }

    void Drive::setDrive (float newDrive)
    {
        drive.store (juce::jlimit (minDrive, maxDrive, newDrive), std::memory_order_relaxed);
    }

    void Drive::setTone (float newTone)
    {
        tone.store (juce::jlimit (minTone, maxTone, newTone), std::memory_order_relaxed);
    }

    void Drive::setLevelDb (float newLevelDb)
    {
        levelDb.store (juce::jlimit (minLevelDb, maxLevelDb, newLevelDb),
                       std::memory_order_relaxed);
    }

    void Drive::setCab (bool shouldUseCab)
    {
        cab.store (shouldUseCab, std::memory_order_relaxed);
    }

    void Drive::getStateInformation (juce::MemoryBlock& destData)
    {
        juce::MemoryOutputStream stream (destData, false);
        const auto settings = getSettings ();

        stream.writeInt (1);
        stream.writeInt (settings.model);
        stream.writeFloat (settings.drive);
        stream.writeFloat (settings.tone);
        stream.writeFloat (settings.levelDb);
        stream.writeBool (settings.cab);
    }

    void Drive::setStateInformation (const void* data, int sizeInBytes)
    {
        if (data == nullptr || sizeInBytes <= 0)
            return;

        juce::MemoryInputStream stream (data, static_cast<std::size_t> (sizeInBytes), false);

        if (stream.readInt () != 1)
            return;

        constexpr int expectedBytes = 4 + 4 + 4 + 4 + 4 + 1;

        if (sizeInBytes < expectedBytes)
            return;

        DriveSettings restored;
        restored.model = stream.readInt ();
        restored.drive = stream.readFloat ();
        restored.tone = stream.readFloat ();
        restored.levelDb = stream.readFloat ();
        restored.cab = stream.readBool ();

        setSettings (restored);
    }

    DriveSettings Drive::clampSettings (const DriveSettings& settings) noexcept
    {
        DriveSettings clamped = settings;
        clamped.model = juce::jlimit (0, numModels - 1, settings.model);
        clamped.drive = juce::jlimit (minDrive, maxDrive, settings.drive);
        clamped.tone = juce::jlimit (minTone, maxTone, settings.tone);
        clamped.levelDb = juce::jlimit (minLevelDb, maxLevelDb, settings.levelDb);
        return clamped;
    }

    float Drive::driveDbFor (const DriveSettings& settings) noexcept
    {
        const auto voicing = voicingFor (static_cast<DriveModel> (settings.model));
        return voicing.minimumDriveDb +
               (voicing.maximumDriveDb - voicing.minimumDriveDb) * settings.drive;
    }

    float Drive::gainForDecibels (float decibels) noexcept
    {
        return juce::Decibels::decibelsToGain (decibels);
    }

    void Drive::updateTargets (const DriveSettings& settings) noexcept
    {
        driveGain.setTargetValue (gainForDecibels (driveDbFor (settings)));
        levelGain.setTargetValue (gainForDecibels (settings.levelDb));
        cabMix.setTargetValue (settings.cab ? 1.0f : 0.0f);
        filters.setTone (settings.tone);
    }

    void Drive::finishModelSwitch (const DriveSettings& settings) noexcept
    {
        if (settings.model == appliedModel)
            return;

        if (modelFade.isSmoothing () || modelFade.getCurrentValue () > 0.0001f)
            return;

        appliedModel = settings.model;
        filters.setVoicing (voicingFor (static_cast<DriveModel> (appliedModel)));
        filters.reset ();
        modelFade.setCurrentAndTargetValue (0.0f);
        modelFade.setTargetValue (1.0f);
    }

}
