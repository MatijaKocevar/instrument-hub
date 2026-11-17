#include "audio/DeviceManager.h"

#include <cmath>

namespace AudioEngine {

namespace {
juce::String formatRates(const juce::Array<double>& rates) {
    juce::String text;

    for (int i = 0; i < rates.size(); ++i) {
        if (i > 0) text << " ";

        const double rate = rates[i];

        if (std::abs(rate - std::floor(rate)) < 0.001)
            text << static_cast<int>(rate);
        else
            text << juce::String(rate, 2);
    }

    return text;
}

juce::String formatBufferSizes(const juce::Array<int>& sizes) {
    juce::String text;

    for (int i = 0; i < sizes.size(); ++i) {
        if (i > 0) text << " ";

        text << sizes[i];
    }

    return text;
}
}  // namespace

juce::String DeviceManager::describeDevices() {
    juce::String text;
    const auto typeNames = getDeviceTypeNames();

    text << "audio device types: "
         << (typeNames.isEmpty() ? juce::String("none") : typeNames.joinIntoString(", ")) << "\n";

    if (listOptions.raw) {
        for (const auto& type : getDeviceTypes()) {
            text << "\ndevice type: " << type.name << "\n"
                 << "  inputs : "
                 << (type.inputDeviceNames.isEmpty() ? juce::String("none")
                                                     : type.inputDeviceNames.joinIntoString(", "))
                 << "\n"
                 << "  outputs: "
                 << (type.outputDeviceNames.isEmpty() ? juce::String("none")
                                                      : type.outputDeviceNames.joinIntoString(", "))
                 << "\n";
        }
    }

    for (const auto& info : getDevices()) {
        text << "\ndevice: " << info.name << "\n"
             << "  type   : " << info.typeName << "\n";

        if (!info.available) text << "  status : unavailable (" << info.unavailableReason << ")\n";

        text << "  input  : " << (info.hasInput ? "yes" : "no");

        if (info.isDefaultInput) text << " (default)";

        text << " channels=" << info.inputChannelNames.size()
             << " mask=" << info.inputChannelMask.toString(2) << "\n"
             << "  output : " << (info.hasOutput ? "yes" : "no");

        if (info.isDefaultOutput) text << " (default)";

        text << " channels=" << info.outputChannelNames.size()
             << " mask=" << info.outputChannelMask.toString(2) << "\n"
             << "  sample rates: " << formatRates(info.sampleRates) << "\n"
             << "  buffer sizes: " << formatBufferSizes(info.bufferSizes) << "\n";
    }

    return text;
}

}  // namespace AudioEngine
