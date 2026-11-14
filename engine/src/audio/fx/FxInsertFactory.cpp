#include "audio/FxRack.h"

#include "dsp/Compressor.h"
#include "dsp/Delay.h"
#include "dsp/Eq.h"
#include "dsp/Reverb.h"
#include "dsp/drive/Drive.h"
#include "dsp/tuner/Tuner.h"

#include <utility>

namespace AudioEngine
{

    namespace
    {
        juce::String toJuce (const std::string& text)
        {
            return juce::String (text);
        }

        std::string toStd (const juce::String& text)
        {
            return text.toStdString ();
        }
    }

    bool FxRack::create (const FxInsertConfig& config,
                         double sampleRate,
                         int maximumBlockSize,
                         int numChannels,
                         std::unique_ptr<InsertEffect>& destination,
                         std::string& error) const
    {
        error.clear ();

        std::unique_ptr<juce::AudioProcessor> inner;
        std::string name = config.name;
        std::string id = config.id;

        switch (config.type)
        {
            case FxInsertType::equalizer:
            {
                auto effect = std::make_unique<ihub::dsp::Eq> ();
                inner = std::move (effect);
                break;
            }

            case FxInsertType::compressor:
            {
                inner = std::make_unique<ihub::dsp::Compressor> ();
                break;
            }

            case FxInsertType::delay:
            {
                auto effect = std::make_unique<ihub::dsp::Delay> ();
                effect->setMix (1.0f);
                inner = std::move (effect);
                break;
            }

            case FxInsertType::reverb:
            {
                auto effect = std::make_unique<ihub::dsp::Reverb> ();
                effect->setMix (1.0f);
                inner = std::move (effect);
                break;
            }

            case FxInsertType::drive:
            {
                inner = std::make_unique<ihub::dsp::Drive> ();
                break;
            }

            case FxInsertType::tuner:
            {
                inner = std::make_unique<ihub::dsp::Tuner> ();
                break;
            }

            case FxInsertType::vst3:
            {
                if (formatManager == nullptr)
                {
                    error = "no plugin format manager is attached";
                    return false;
                }

                if (config.path.empty ())
                {
                    error = "a vst3 insert needs a bundle path";
                    return false;
                }

                const juce::File bundle (toJuce (config.path));

                if (!bundle.exists ())
                {
                    error = "plugin bundle does not exist: " + config.path;
                    return false;
                }

                juce::OwnedArray<juce::PluginDescription> types;
                const auto path = bundle.getFullPathName ();

                for (auto* format : formatManager->getFormats ())
                    if (format->fileMightContainThisPluginType (path))
                        format->findAllTypesForFile (types, path);

                if (types.isEmpty ())
                {
                    error = "no plugin types found in " + config.path;
                    return false;
                }

                juce::String pluginError;
                auto instance = formatManager->createPluginInstance (
                    *types.getFirst (), sampleRate, juce::jmax (1, maximumBlockSize), pluginError);

                if (instance == nullptr)
                {
                    error = "could not instantiate " + config.path + ": " + toStd (pluginError);
                    return false;
                }

                const auto applyLayout = [&instance] (const juce::AudioChannelSet& set)
                {
                    auto layout = instance->getBusesLayout ();

                    if (layout.inputBuses.isEmpty () || layout.outputBuses.isEmpty ())
                        return false;

                    layout.inputBuses.getReference (0) = set;
                    layout.outputBuses.getReference (0) = set;

                    if (!instance->setBusesLayout (layout))
                        return false;

                    return instance->getTotalNumInputChannels () > 0 &&
                           instance->getTotalNumOutputChannels () > 0;
                };

                const auto target =
                    juce::AudioChannelSet::canonicalChannelSet (juce::jmax (1, numChannels));

                if (!applyLayout (target) &&
                    !(numChannels != 1 && applyLayout (juce::AudioChannelSet::mono ())))
                {
                    error = "plugin " + config.path +
                            " does not provide an FX bus layout (main input + output)";
                    return false;
                }

                if (id.empty ())
                    id = config.path;

                if (name.empty ())
                    name = toStd (instance->getName ());

                inner = std::move (instance);
                break;
            }
        }

        if (inner == nullptr)
        {
            error = "could not create the effect";
            return false;
        }

        if (config.type != FxInsertType::vst3 && id.empty ())
            id = toString (config.type);

        if (name.empty ())
            name = id;

        auto effect = std::make_unique<InsertEffect> (std::move (inner), config.type, id, name);

        effect->setBypassed (config.bypassed);
        effect->setWet (config.wet);

        if (!config.parameters.empty () &&
            !applyParameters (*effect, config.parameters, false, error))
            return false;

        applyTransportTo (*effect);

        effect->prepareToPlay (sampleRate, juce::jmax (1, maximumBlockSize));
        effect->refreshLatency ();

        destination = std::move (effect);
        return true;
    }

}
