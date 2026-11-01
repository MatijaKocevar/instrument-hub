#include "analog_lab/FirstPhase.h"
#include "analog_lab/Parent.h"
#include "analog_lab/RestartPhase.h"

#include <juce_events/juce_events.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc == 4 && juce::String (argv[1]) == "--first")
        return AnalogLab::runFirstPhase (juce::File (juce::String (argv[2])),
                                         juce::File (juce::String (argv[3])));

    if (argc == 5 && juce::String (argv[1]) == "--restart")
        return AnalogLab::runRestartPhase (juce::File (juce::String (argv[2])),
                                           std::string (argv[3]),
                                           std::string (argv[4]));

    return AnalogLab::runParent();
}
