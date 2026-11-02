#include "multipatch_batch/BatchPhase.h"
#include "multipatch_batch/Parent.h"

#include <juce_events/juce_events.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc == 5 && juce::String (argv[1]) == "--batch")
        return MultiPatch::runBatchPhase (juce::String (argv[2]).getIntValue(),
                                          juce::File (juce::String (argv[3])),
                                          juce::File (juce::String (argv[4])));

    return MultiPatch::runParent();
}
