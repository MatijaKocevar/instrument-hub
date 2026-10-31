#include "FxRack.h"

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
    return text.toStdString();
}

/** Finds a parameter by id or display name, the same rule the plugin page uses. */
juce::AudioProcessorParameter* findPluginParameter (juce::AudioProcessor& processor,
                                                    const std::string& idOrName)
{
    if (idOrName.empty())
        return nullptr;

    const auto wanted = toJuce (idOrName);

    for (auto* parameter : processor.getParameters())
    {
        if (parameter == nullptr)
            continue;

        if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
            if (toJuce (hosted->getParameterID()).equalsIgnoreCase (wanted))
                return parameter;

        if (parameter->getName (64).equalsIgnoreCase (wanted))
            return parameter;
    }

    return nullptr;
}

std::string pluginParameterKey (juce::AudioProcessorParameter& parameter)
{
    if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (&parameter))
        if (const auto id = hosted->getParameterID(); id.isNotEmpty())
            return toStd (id);

    return toStd (parameter.getName (64));
}

bool applyEqParameters (ihub::dsp::Eq& eq,
                        const std::map<std::string, float>& parameters,
                        std::string& error)
{
    auto bands = eq.getAllBands();

    for (const auto& [key, value] : parameters)
    {
        int band = -1;
        std::string field;

        if (key.rfind ("low", 0) == 0)        { band = 0; field = key.substr (3); }
        else if (key.rfind ("mid", 0) == 0)   { band = 1; field = key.substr (3); }
        else if (key.rfind ("high", 0) == 0)  { band = 2; field = key.substr (4); }

        if (band < 0)
        {
            error = "unknown eq parameter \"" + key + "\"";
            return false;
        }

        auto& settings = bands[static_cast<std::size_t> (band)];

        // `low`, `mid` and `high` are the legacy set-file names for a band's
        // gain; keep accepting them so older sets load.
        if (field == "FrequencyHz")
            settings.frequencyHz = juce::jlimit (ihub::dsp::Eq::minFrequencyHz,
                                                 ihub::dsp::Eq::maxFrequencyHz, value);
        else if (field == "GainDb" || field.empty())
            settings.gainDb = juce::jlimit (-ihub::dsp::Eq::maxGainDb,
                                            ihub::dsp::Eq::maxGainDb, value);
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
    auto settings = compressor.getSettings();

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
    auto settings = delay.getSettings();

    // The insert wrapper owns the wet/dry balance, so the built-in always runs
    // fully wet and the API's `wet` is the single balance control.
    settings.mix = 1.0f;

    for (const auto& [key, value] : parameters)
    {
        if (key == "tempoSync")
            settings.tempoSync = value >= 0.5f;
        else if (key == "division")
        {
            const int division = static_cast<int> (std::lround (value));

            if (division < 0
                || division >= static_cast<int> (ihub::dsp::DelayDivision::numDivisions))
            {
                error = "delay division must be 0.."
                        + std::to_string (static_cast<int> (ihub::dsp::DelayDivision::numDivisions) - 1);
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
    auto settings = reverb.getSettings();

    // As with the delay, the insert wrapper is the wet/dry control.
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
    const auto bands = eq.getAllBands();

    return {
        { "lowFrequencyHz", bands[0].frequencyHz },
        { "lowGainDb", bands[0].gainDb },
        { "lowQ", bands[0].q },
        { "lowEnabled", bands[0].enabled ? 1.0f : 0.0f },
        { "midFrequencyHz", bands[1].frequencyHz },
        { "midGainDb", bands[1].gainDb },
        { "midQ", bands[1].q },
        { "midEnabled", bands[1].enabled ? 1.0f : 0.0f },
        { "highFrequencyHz", bands[2].frequencyHz },
        { "highGainDb", bands[2].gainDb },
        { "highQ", bands[2].q },
        { "highEnabled", bands[2].enabled ? 1.0f : 0.0f }
    };
}

std::map<std::string, float> readCompressorParameters (const ihub::dsp::Compressor& compressor)
{
    const auto settings = compressor.getSettings();

    return {
        { "thresholdDb", settings.thresholdDb },
        { "ratio", settings.ratio },
        { "attackMs", settings.attackMs },
        { "releaseMs", settings.releaseMs },
        { "makeupDb", settings.makeupDb }
    };
}

std::map<std::string, float> readDelayParameters (const ihub::dsp::Delay& delay)
{
    const auto settings = delay.getSettings();

    return {
        { "tempoSync", settings.tempoSync ? 1.0f : 0.0f },
        { "division", static_cast<float> (settings.division) },
        { "timeMs", settings.timeMs },
        { "feedback", settings.feedback },
        { "dampingHz", settings.dampingHz }
    };
}

std::map<std::string, float> readReverbParameters (const ihub::dsp::Reverb& reverb)
{
    const auto settings = reverb.getSettings();

    return {
        { "roomSize", settings.roomSize },
        { "damping", settings.damping },
        { "width", settings.width },
        { "preDelayMs", settings.preDelayMs }
    };
}
} // namespace

//==============================================================================
const char* toString (FxInsertType type) noexcept
{
    switch (type)
    {
        case FxInsertType::equalizer:  return "eq";
        case FxInsertType::compressor: return "compressor";
        case FxInsertType::delay:      return "delay";
        case FxInsertType::reverb:     return "reverb";
        case FxInsertType::vst3:       return "vst3";
    }

    return "eq";
}

bool fxInsertTypeFromString (std::string_view text, FxInsertType& type) noexcept
{
    if (text == "eq" || text == "equalizer")     { type = FxInsertType::equalizer; return true; }
    if (text == "compressor" || text == "comp")  { type = FxInsertType::compressor; return true; }
    if (text == "delay")                         { type = FxInsertType::delay; return true; }
    if (text == "reverb")                        { type = FxInsertType::reverb; return true; }
    if (text == "vst3" || text == "plugin")      { type = FxInsertType::vst3; return true; }

    return false;
}

//==============================================================================
InsertEffect::InsertEffect (std::unique_ptr<juce::AudioProcessor> processorToWrap,
                            FxInsertType typeToUse,
                            std::string idToUse,
                            std::string nameToUse)
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      processor (std::move (processorToWrap)),
      type (typeToUse),
      id (std::move (idToUse)),
      effectName (nameToUse.empty() && processor != nullptr
                      ? processor->getName()
                      : juce::String (nameToUse))
{
    jassert (processor != nullptr);

    if (processor != nullptr)
    {
        const int latency = juce::jmax (0, processor->getLatencySamples());
        cachedLatency.store (latency, std::memory_order_relaxed);
        setLatencySamples (latency);
    }
}

void InsertEffect::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    if (processor == nullptr)
        return;

    maximumBlockSize = juce::jmax (1, maximumExpectedSamplesPerBlock);
    numChannels = juce::jmax (1, getTotalNumOutputChannels());

    dry.setSize (numChannels, maximumBlockSize, false, true, true);
    dry.clear();

    // Give the wrapped processor the same channel layout the wrapper presents,
    // with a mono fallback for an effect that cannot do stereo.
    const auto target = juce::AudioChannelSet::canonicalChannelSet (numChannels);
    auto layout = processor->getBusesLayout();
    bool changed = false;

    if (! layout.inputBuses.isEmpty()
        && layout.inputBuses.getReference (0) != target)
    {
        layout.inputBuses.getReference (0) = target;
        changed = true;
    }

    if (! layout.outputBuses.isEmpty()
        && layout.outputBuses.getReference (0) != target)
    {
        layout.outputBuses.getReference (0) = target;
        changed = true;
    }

    if (changed && ! processor->setBusesLayout (layout))
    {
        auto fallback = processor->getBusesLayout();

        if (! fallback.outputBuses.isEmpty())
            fallback.outputBuses.getReference (0) = target;

        processor->setBusesLayout (fallback);
    }

    processor->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);
    processor->prepareToPlay (sampleRate, maximumBlockSize);

    refreshLatency();
}

void InsertEffect::releaseResources()
{
    if (processor != nullptr)
        processor->releaseResources();

    cachedLatency.store (0, std::memory_order_relaxed);
}

void InsertEffect::reset()
{
    if (processor != nullptr)
        processor->reset();

    dry.clear();
}

void InsertEffect::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Bypass and a fully dry balance are the same audio path: leave the buffer
    // untouched so an idle insert is bit-exact.
    if (processor == nullptr || bypassed.load (std::memory_order_relaxed))
        return;

    const float wetAmount = juce::jlimit (0.0f,
                                          1.0f,
                                          wet.load (std::memory_order_relaxed));

    if (wetAmount <= 0.0f)
        return;

    const int numSamples = buffer.getNumSamples();
    const int bufferChannels = buffer.getNumChannels();

    if (numSamples <= 0 || bufferChannels <= 0)
        return;

    const int innerChannels = juce::jmax (processor->getTotalNumInputChannels(),
                                          processor->getTotalNumOutputChannels());

    // A layout the engine cannot feed stays dry instead of reading past the
    // buffer. Every prepared buffer is at least `numChannels` wide.
    if (innerChannels <= 0 || innerChannels > bufferChannels
        || numSamples > dry.getNumSamples()
        || bufferChannels > dry.getNumChannels())
        return;

    const int dryChannels = juce::jmin (bufferChannels, dry.getNumChannels());

    for (int channel = 0; channel < dryChannels; ++channel)
        dry.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    if (innerChannels == bufferChannels)
    {
        processor->processBlock (buffer, midi);
    }
    else
    {
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(),
                                       innerChannels,
                                       numSamples);
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
            output[sample] = drySamples[sample] * dryAmount
                             + output[sample] * wetAmount;
    }
}

bool InsertEffect::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();

    return output == juce::AudioChannelSet::mono()
           || output == juce::AudioChannelSet::stereo();
}

double InsertEffect::getTailLengthSeconds() const
{
    return processor != nullptr ? processor->getTailLengthSeconds() : 0.0;
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

bool InsertEffect::isBypassed() const noexcept
{
    return bypassed.load (std::memory_order_relaxed);
}

void InsertEffect::setWet (float wetAmount) noexcept
{
    wet.store (juce::jlimit (0.0f, 1.0f, wetAmount), std::memory_order_relaxed);
}

float InsertEffect::getWet() const noexcept
{
    return wet.load (std::memory_order_relaxed);
}

void InsertEffect::refreshLatency() noexcept
{
    const bool audible = ! bypassed.load (std::memory_order_relaxed)
                         && wet.load (std::memory_order_relaxed) > 0.0f;

    const int latency = (processor != nullptr && audible)
                            ? juce::jmax (0, processor->getLatencySamples())
                            : 0;

    cachedLatency.store (latency, std::memory_order_relaxed);

    // JUCE's `getLatencySamples()` is a plain member, not virtual, so publish
    // the effective value through the base class. `FxChain::refreshLatency()`
    // reads it from there.
    setLatencySamples (latency);
}

//==============================================================================
FxRack::FxRack (juce::AudioPluginFormatManager* formatManagerToUse)
    : formatManager (formatManagerToUse)
{
}

bool FxRack::create (const FxInsertConfig& config,
                     double sampleRate,
                     int maximumBlockSize,
                     int numChannels,
                     std::unique_ptr<InsertEffect>& destination,
                     std::string& error) const
{
    error.clear();

    std::unique_ptr<juce::AudioProcessor> inner;
    std::string name = config.name;
    std::string id = config.id;

    switch (config.type)
    {
        case FxInsertType::equalizer:
        {
            auto effect = std::make_unique<ihub::dsp::Eq>();
            inner = std::move (effect);
            break;
        }

        case FxInsertType::compressor:
        {
            inner = std::make_unique<ihub::dsp::Compressor>();
            break;
        }

        case FxInsertType::delay:
        {
            auto effect = std::make_unique<ihub::dsp::Delay>();
            effect->setMix (1.0f);
            inner = std::move (effect);
            break;
        }

        case FxInsertType::reverb:
        {
            auto effect = std::make_unique<ihub::dsp::Reverb>();
            effect->setMix (1.0f);
            inner = std::move (effect);
            break;
        }

        case FxInsertType::vst3:
        {
            if (formatManager == nullptr)
            {
                error = "no plugin format manager is attached";
                return false;
            }

            if (config.path.empty())
            {
                error = "a vst3 insert needs a bundle path";
                return false;
            }

            const juce::File bundle (toJuce (config.path));

            if (! bundle.exists())
            {
                error = "plugin bundle does not exist: " + config.path;
                return false;
            }

            juce::OwnedArray<juce::PluginDescription> types;
            const auto path = bundle.getFullPathName();

            for (auto* format : formatManager->getFormats())
                if (format->fileMightContainThisPluginType (path))
                    format->findAllTypesForFile (types, path);

            if (types.isEmpty())
            {
                error = "no plugin types found in " + config.path;
                return false;
            }

            juce::String pluginError;
            auto instance = formatManager->createPluginInstance (*types.getFirst(),
                                                                sampleRate,
                                                                juce::jmax (1, maximumBlockSize),
                                                                pluginError);

            if (instance == nullptr)
            {
                error = "could not instantiate " + config.path + ": "
                        + toStd (pluginError);
                return false;
            }

            const auto applyLayout = [&instance] (const juce::AudioChannelSet& set)
            {
                auto layout = instance->getBusesLayout();

                if (layout.inputBuses.isEmpty() || layout.outputBuses.isEmpty())
                    return false;

                // A generator's input bus starts disabled; an insert must
                // receive the engine's audio, so enable it explicitly.
                layout.inputBuses.getReference (0) = set;
                layout.outputBuses.getReference (0) = set;

                if (! instance->setBusesLayout (layout))
                    return false;

                return instance->getTotalNumInputChannels() > 0
                       && instance->getTotalNumOutputChannels() > 0;
            };

            const auto target = juce::AudioChannelSet::canonicalChannelSet (
                juce::jmax (1, numChannels));

            if (! applyLayout (target)
                && ! (numChannels != 1 && applyLayout (juce::AudioChannelSet::mono())))
            {
                error = "plugin " + config.path
                        + " does not provide an FX bus layout (main input + output)";
                return false;
            }

            if (id.empty())
                id = config.path;

            if (name.empty())
                name = toStd (instance->getName());

            inner = std::move (instance);
            break;
        }
    }

    if (inner == nullptr)
    {
        error = "could not create the effect";
        return false;
    }

    if (config.type != FxInsertType::vst3 && id.empty())
        id = toString (config.type);

    if (name.empty())
        name = id;

    auto effect = std::make_unique<InsertEffect> (std::move (inner),
                                                  config.type,
                                                  id,
                                                  name);

    effect->setBypassed (config.bypassed);
    effect->setWet (config.wet);

    if (! config.parameters.empty()
        && ! applyParameters (*effect, config.parameters, false, error))
        return false;

    applyTransportTo (*effect);

    // Prepare now so the effect is ready before it joins the chain, and so its
    // latency is cached for the API's first read.
    effect->prepareToPlay (sampleRate, juce::jmax (1, maximumBlockSize));
    effect->refreshLatency();

    destination = std::move (effect);
    return true;
}

bool FxRack::add (const FxInsertConfig& config,
                  int position,
                  double sampleRate,
                  int maximumBlockSize,
                  int numChannels,
                  std::string& error)
{
    std::unique_ptr<InsertEffect> effect;

    if (! create (config, sampleRate, maximumBlockSize, numChannels, effect, error))
        return false;

    Insert insert;
    insert.type = config.type;
    insert.id = effect->getId();
    insert.name = toStd (effect->getName());
    insert.effect = std::move (effect);

    const int target = position < 0
                           ? size()
                           : juce::jlimit (0, size(), position);

    inserts.insert (inserts.begin() + static_cast<std::ptrdiff_t> (target),
                    std::move (insert));
    return true;
}

bool FxRack::replace (const std::vector<FxInsertConfig>& configs,
                      double sampleRate,
                      int maximumBlockSize,
                      int numChannels,
                      std::string& error)
{
    std::vector<Insert> replacement;
    replacement.reserve (configs.size());

    for (const auto& config : configs)
    {
        std::unique_ptr<InsertEffect> effect;

        if (! create (config, sampleRate, maximumBlockSize, numChannels, effect, error))
            return false;

        Insert insert;
        insert.type = config.type;
        insert.id = effect->getId();
        insert.name = toStd (effect->getName());
        insert.effect = std::move (effect);
        replacement.push_back (std::move (insert));
    }

    inserts = std::move (replacement);
    return true;
}

bool FxRack::update (int index,
                     const FxInsertPatch& patch,
                     double sampleRate,
                     int maximumBlockSize,
                     int numChannels,
                     std::string& error)
{
    juce::ignoreUnused (sampleRate, maximumBlockSize, numChannels);

    if (index < 0 || index >= size())
    {
        error = "fx index is out of range";
        return false;
    }

    auto& insert = inserts[static_cast<std::size_t> (index)];
    auto& effect = *insert.effect;

    if (patch.wet.has_value())
    {
        const float wet = *patch.wet;

        if (! std::isfinite (wet) || wet < 0.0f || wet > 1.0f)
        {
            error = "wet must be between 0 and 1";
            return false;
        }

        effect.setWet (wet);
    }

    if (patch.bypassed.has_value())
        effect.setBypassed (*patch.bypassed);

    if (patch.parameters.has_value()
        && ! applyParameters (effect, *patch.parameters, true, error))
        return false;

    if (patch.position.has_value())
    {
        const int position = *patch.position;

        if (position < 0 || position >= size())
        {
            error = "position is out of range";
            return false;
        }

        if (position != index)
        {
            Insert moved = std::move (inserts[static_cast<std::size_t> (index)]);
            inserts.erase (inserts.begin() + static_cast<std::ptrdiff_t> (index));
            inserts.insert (inserts.begin() + static_cast<std::ptrdiff_t> (position),
                            std::move (moved));
        }
    }

    return true;
}

bool FxRack::remove (int index, std::string& error)
{
    if (index < 0 || index >= size())
    {
        error = "fx index is out of range";
        return false;
    }

    inserts.erase (inserts.begin() + static_cast<std::ptrdiff_t> (index));
    return true;
}

bool FxRack::reorder (const std::vector<int>& order, std::string& error)
{
    if (order.size() != inserts.size())
    {
        error = "order must list every insert exactly once";
        return false;
    }

    std::vector<bool> seen (inserts.size(), false);
    std::vector<Insert> reordered;
    reordered.reserve (inserts.size());

    for (const int index : order)
    {
        if (index < 0 || index >= size() || seen[static_cast<std::size_t> (index)])
        {
            error = "order must be a permutation of the insert indices";
            return false;
        }

        seen[static_cast<std::size_t> (index)] = true;
        reordered.push_back (std::move (inserts[static_cast<std::size_t> (index)]));
    }

    inserts = std::move (reordered);
    return true;
}

void FxRack::clear() noexcept
{
    inserts.clear();
}

void FxRack::rebuild (FxChain& chain,
                      double sampleRate,
                      int maximumBlockSize,
                      int numChannels)
{
    juce::ignoreUnused (sampleRate, maximumBlockSize, numChannels);

    chain.clear();

    for (auto& insert : inserts)
        if (insert.effect != nullptr)
            chain.add (insert.effect.get());

    chain.refreshLatency();
}

int FxRack::latencySamples() const noexcept
{
    int total = 0;

    for (const auto& insert : inserts)
        if (insert.effect != nullptr)
            total += juce::jmax (0, insert.effect->getLatencySamples());

    return total;
}

FxInsertSnapshot FxRack::getInsert (int index) const
{
    FxInsertSnapshot snapshot;

    if (index < 0 || index >= size())
        return snapshot;

    const auto& insert = inserts[static_cast<std::size_t> (index)];

    snapshot.index = index;
    snapshot.type = insert.type;
    snapshot.kind = insert.type == FxInsertType::vst3 ? "vst3" : "builtin";
    snapshot.id = insert.id;
    snapshot.name = insert.name;

    if (insert.effect != nullptr)
    {
        snapshot.bypassed = insert.effect->isBypassed();
        snapshot.wet = insert.effect->getWet();
        snapshot.latencySamples = insert.effect->getLatencySamples();
        snapshot.parameters = readParameters (*insert.effect);
    }

    return snapshot;
}

std::vector<FxInsertSnapshot> FxRack::snapshots() const
{
    std::vector<FxInsertSnapshot> result;
    result.reserve (inserts.size());

    for (int index = 0; index < size(); ++index)
        result.push_back (getInsert (index));

    return result;
}

void FxRack::setTransport (const FxTransport& newTransport) noexcept
{
    transport = newTransport;

    for (auto& insert : inserts)
        if (insert.effect != nullptr)
            applyTransportTo (*insert.effect);
}

void FxRack::applyTransportTo (InsertEffect& effect) const
{
    if (effect.getType() != FxInsertType::delay)
        return;

    if (auto* delay = static_cast<ihub::dsp::Delay*> (effect.getWrappedProcessor()))
    {
        ihub::dsp::DelayTransport delayTransport;
        delayTransport.playing = transport.playing;
        delayTransport.bpm = transport.bpm;
        delayTransport.ppq = transport.ppq;
        delay->setTransport (delayTransport);
    }
}

bool FxRack::applyParameters (InsertEffect& effect,
                              const std::map<std::string, float>& parameters,
                              bool replaceAll,
                              std::string& error) const
{
    juce::ignoreUnused (replaceAll);

    auto* processor = effect.getWrappedProcessor();

    if (processor == nullptr)
    {
        error = "the insert has no processor";
        return false;
    }

    // `mix` is the legacy set-file name for the insert's wet/dry balance. It
    // is not a wrapped-effect parameter: the wrapper owns the balance.
    std::map<std::string, float> effectParameters;

    for (const auto& [key, value] : parameters)
    {
        if (key == "mix")
            effect.setWet (value);
        else
            effectParameters[key] = value;
    }

    switch (effect.getType())
    {
        case FxInsertType::equalizer:
            return applyEqParameters (*static_cast<ihub::dsp::Eq*> (processor),
                                      effectParameters, error);

        case FxInsertType::compressor:
            return applyCompressorParameters (*static_cast<ihub::dsp::Compressor*> (processor),
                                              effectParameters, error);

        case FxInsertType::delay:
            return applyDelayParameters (*static_cast<ihub::dsp::Delay*> (processor),
                                         effectParameters, error);

        case FxInsertType::reverb:
            return applyReverbParameters (*static_cast<ihub::dsp::Reverb*> (processor),
                                          effectParameters, error);

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
    const auto* processor = effect.getWrappedProcessor();

    if (processor == nullptr)
        return {};

    switch (effect.getType())
    {
        case FxInsertType::equalizer:
            return readEqParameters (*static_cast<const ihub::dsp::Eq*> (processor));

        case FxInsertType::compressor:
            return readCompressorParameters (*static_cast<const ihub::dsp::Compressor*> (processor));

        case FxInsertType::delay:
            return readDelayParameters (*static_cast<const ihub::dsp::Delay*> (processor));

        case FxInsertType::reverb:
            return readReverbParameters (*static_cast<const ihub::dsp::Reverb*> (processor));

        case FxInsertType::vst3:
        {
            std::map<std::string, float> result;

            for (auto* parameter : processor->getParameters())
            {
                if (parameter == nullptr)
                    continue;

                result[pluginParameterKey (*parameter)] = parameter->getValue();

                if (result.size() >= 128)
                    break;
            }

            return result;
        }
    }

    return {};
}

} // namespace AudioEngine
