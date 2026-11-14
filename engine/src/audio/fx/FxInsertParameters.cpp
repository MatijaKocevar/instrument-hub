#include "audio/FxRack.h"

#include "dsp/Compressor.h"
#include "dsp/Delay.h"
#include "dsp/Eq.h"
#include "dsp/Reverb.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace AudioEngine
{

    namespace
    {
        juce::String toJuce (const std::string& text)
        {
            return juce::String (text);
        }

        juce::String toJuce (const juce::String& text)
        {
            return text;
        }

        std::string toStd (const juce::String& text)
        {
            return text.toStdString ();
        }

        juce::AudioProcessorParameter* findPluginParameter (juce::AudioProcessor& processor,
                                                            const std::string& idOrName)
        {
            if (idOrName.empty ())
                return nullptr;

            const auto wanted = toJuce (idOrName);

            for (auto* parameter : processor.getParameters ())
            {
                if (parameter == nullptr)
                    continue;

                if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
                    if (toJuce (hosted->getParameterID ()).equalsIgnoreCase (wanted))
                        return parameter;

                if (parameter->getName (64).equalsIgnoreCase (wanted))
                    return parameter;
            }

            return nullptr;
        }

        std::string pluginParameterKey (juce::AudioProcessorParameter& parameter)
        {
            if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (&parameter))
                if (const auto id = hosted->getParameterID (); id.isNotEmpty ())
                    return toStd (id);

            return toStd (parameter.getName (64));
        }

        bool applyEqParameters (ihub::dsp::Eq& eq,
                                const std::map<std::string, float>& parameters,
                                std::string& error)
        {
            auto bands = eq.getAllBands ();

            for (const auto& [key, value] : parameters)
            {
                int band = -1;
                std::string field;

                if (key.rfind ("low", 0) == 0)
                {
                    band = 0;
                    field = key.substr (3);
                }
                else if (key.rfind ("mid", 0) == 0)
                {
                    band = 1;
                    field = key.substr (3);
                }
                else if (key.rfind ("high", 0) == 0)
                {
                    band = 2;
                    field = key.substr (4);
                }

                if (band < 0)
                {
                    error = "unknown eq parameter \"" + key + "\"";
                    return false;
                }

                auto& settings = bands[static_cast<std::size_t> (band)];

                if (field == "FrequencyHz")
                    settings.frequencyHz = juce::jlimit (
                        ihub::dsp::Eq::minFrequencyHz, ihub::dsp::Eq::maxFrequencyHz, value);
                else if (field == "GainDb" || field.empty ())
                    settings.gainDb =
                        juce::jlimit (-ihub::dsp::Eq::maxGainDb, ihub::dsp::Eq::maxGainDb, value);
                else if (field == "Q")
                    settings.q = juce::jlimit (ihub::dsp::Eq::minQ, ihub::dsp::Eq::maxQ, value);
                else if (field == "Enabled")
                    settings.enabled = value >= 0.5f;
                else
                {
                    error = "unknown eq parameter \"" + key + "\"";
                    return false;
                }
            }

            eq.setAllBands (bands);
            return true;
        }

        bool applyCompressorParameters (ihub::dsp::Compressor& compressor,
                                        const std::map<std::string, float>& parameters,
                                        std::string& error)
        {
            auto settings = compressor.getSettings ();

            for (const auto& [key, value] : parameters)
            {
                if (key == "thresholdDb" || key == "threshold")
                    settings.thresholdDb = value;
                else if (key == "ratio")
                    settings.ratio = value;
                else if (key == "attackMs")
                    settings.attackMs = value;
                else if (key == "releaseMs")
                    settings.releaseMs = value;
                else if (key == "makeupDb")
                    settings.makeupDb = value;
                else
                {
                    error = "unknown compressor parameter \"" + key + "\"";
                    return false;
                }
            }

            compressor.setSettings (settings);
            return true;
        }

        bool applyDelayParameters (ihub::dsp::Delay& delay,
                                   const std::map<std::string, float>& parameters,
                                   std::string& error)
        {
            auto settings = delay.getSettings ();

            settings.mix = 1.0f;

            for (const auto& [key, value] : parameters)
            {
                if (key == "tempoSync")
                    settings.tempoSync = value >= 0.5f;
                else if (key == "division")
                {
                    const int division = static_cast<int> (std::lround (value));

                    if (division < 0 ||
                        division >= static_cast<int> (ihub::dsp::DelayDivision::numDivisions))
                    {
                        error = "delay division must be 0.." +
                                std::to_string (
                                    static_cast<int> (ihub::dsp::DelayDivision::numDivisions) - 1);
                        return false;
                    }

                    settings.division = static_cast<ihub::dsp::DelayDivision> (division);
                }
                else if (key == "timeMs")
                    settings.timeMs = value;
                else if (key == "feedback")
                    settings.feedback = value;
                else if (key == "dampingHz")
                    settings.dampingHz = value;
                else
                {
                    error = "unknown delay parameter \"" + key + "\"";
                    return false;
                }
            }

            delay.setSettings (settings);
            return true;
        }

        bool applyReverbParameters (ihub::dsp::Reverb& reverb,
                                    const std::map<std::string, float>& parameters,
                                    std::string& error)
        {
            auto settings = reverb.getSettings ();

            settings.mix = 1.0f;

            for (const auto& [key, value] : parameters)
            {
                if (key == "roomSize")
                    settings.roomSize = value;
                else if (key == "damping")
                    settings.damping = value;
                else if (key == "width")
                    settings.width = value;
                else if (key == "preDelayMs")
                    settings.preDelayMs = value;
                else
                {
                    error = "unknown reverb parameter \"" + key + "\"";
                    return false;
                }
            }

            reverb.setSettings (settings);
            return true;
        }

        std::map<std::string, float> readEqParameters (const ihub::dsp::Eq& eq)
        {
            const auto bands = eq.getAllBands ();

            return {{"lowFrequencyHz", bands[0].frequencyHz},
                    {"lowGainDb", bands[0].gainDb},
                    {"lowQ", bands[0].q},
                    {"lowEnabled", bands[0].enabled ? 1.0f : 0.0f},
                    {"midFrequencyHz", bands[1].frequencyHz},
                    {"midGainDb", bands[1].gainDb},
                    {"midQ", bands[1].q},
                    {"midEnabled", bands[1].enabled ? 1.0f : 0.0f},
                    {"highFrequencyHz", bands[2].frequencyHz},
                    {"highGainDb", bands[2].gainDb},
                    {"highQ", bands[2].q},
                    {"highEnabled", bands[2].enabled ? 1.0f : 0.0f}};
        }

        std::map<std::string, float>
        readCompressorParameters (const ihub::dsp::Compressor& compressor)
        {
            const auto settings = compressor.getSettings ();

            return {{"thresholdDb", settings.thresholdDb},
                    {"ratio", settings.ratio},
                    {"attackMs", settings.attackMs},
                    {"releaseMs", settings.releaseMs},
                    {"makeupDb", settings.makeupDb}};
        }

        std::map<std::string, float> readDelayParameters (const ihub::dsp::Delay& delay)
        {
            const auto settings = delay.getSettings ();

            return {{"tempoSync", settings.tempoSync ? 1.0f : 0.0f},
                    {"division", static_cast<float> (settings.division)},
                    {"timeMs", settings.timeMs},
                    {"feedback", settings.feedback},
                    {"dampingHz", settings.dampingHz}};
        }

        std::map<std::string, float> readReverbParameters (const ihub::dsp::Reverb& reverb)
        {
            const auto settings = reverb.getSettings ();

            return {{"roomSize", settings.roomSize},
                    {"damping", settings.damping},
                    {"width", settings.width},
                    {"preDelayMs", settings.preDelayMs}};
        }
    }

    bool FxRack::applyParameters (InsertEffect& effect,
                                  const std::map<std::string, float>& parameters,
                                  bool replaceAll,
                                  std::string& error) const
    {
        juce::ignoreUnused (replaceAll);

        auto* processor = effect.getWrappedProcessor ();

        if (processor == nullptr)
        {
            error = "the insert has no processor";
            return false;
        }

        std::map<std::string, float> effectParameters;

        for (const auto& [key, value] : parameters)
        {
            if (key == "mix")
                effect.setWet (value);
            else
                effectParameters[key] = value;
        }

        switch (effect.getType ())
        {
            case FxInsertType::equalizer:
                return applyEqParameters (
                    *static_cast<ihub::dsp::Eq*> (processor), effectParameters, error);

            case FxInsertType::compressor:
                return applyCompressorParameters (
                    *static_cast<ihub::dsp::Compressor*> (processor), effectParameters, error);

            case FxInsertType::delay:
                return applyDelayParameters (
                    *static_cast<ihub::dsp::Delay*> (processor), effectParameters, error);

            case FxInsertType::reverb:
                return applyReverbParameters (
                    *static_cast<ihub::dsp::Reverb*> (processor), effectParameters, error);

            case FxInsertType::vst3:
            {
                for (const auto& [key, value] : effectParameters)
                {
                    auto* parameter = findPluginParameter (*processor, key);

                    if (parameter == nullptr)
                    {
                        error = "unknown plugin parameter \"" + key + "\"";
                        return false;
                    }

                    parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
                }

                return true;
            }
        }

        error = "unknown insert type";
        return false;
    }

    std::map<std::string, float> FxRack::readParameters (const InsertEffect& effect) const
    {
        const auto* processor = effect.getWrappedProcessor ();

        if (processor == nullptr)
            return {};

        switch (effect.getType ())
        {
            case FxInsertType::equalizer:
                return readEqParameters (*static_cast<const ihub::dsp::Eq*> (processor));

            case FxInsertType::compressor:
                return readCompressorParameters (
                    *static_cast<const ihub::dsp::Compressor*> (processor));

            case FxInsertType::delay:
                return readDelayParameters (*static_cast<const ihub::dsp::Delay*> (processor));

            case FxInsertType::reverb:
                return readReverbParameters (*static_cast<const ihub::dsp::Reverb*> (processor));

            case FxInsertType::vst3:
            {
                std::map<std::string, float> result;

                for (auto* parameter : processor->getParameters ())
                {
                    if (parameter == nullptr)
                        continue;

                    result[pluginParameterKey (*parameter)] = parameter->getValue ();

                    if (result.size () >= 128)
                        break;
                }

                return result;
            }
        }

        return {};
    }

}
