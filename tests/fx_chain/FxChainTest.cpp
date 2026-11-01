#include "Fixtures.h"
#include "ChainOrder.h"
#include "EmptyChain.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File bundle = argc > 1
        ? juce::File (juce::File (argv[1]).getFullPathName())
        : juce::File (IHUB_TESTPLUGIN_VST3);

    std::cout << "bundle: " << bundle.getFullPathName() << std::endl;

    FxChainTest::runEmptyChainIsBitExact();
    FxChainTest::runEmptyChainThroughSlot();

    if (bundle.isDirectory())
        FxChainTest::runChainOrder (bundle);
    else
        FxChainTest::check (false, "TestPlugin.vst3 bundle exists");

    std::cout << (FxChainTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << FxChainTest::failures << " failed check(s)" << std::endl;

    return FxChainTest::failures == 0 ? 0 : 1;
}
