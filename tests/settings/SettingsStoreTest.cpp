#include "TestSupport.h"

#include <iostream>

int main()
{
    std::cout << "ihub-settings-store-test: app settings persistence" << std::endl;

    SettingsTest::runDefaultChecks();
    SettingsTest::runRoundTripChecks();
    SettingsTest::runCorruptRecoveryChecks();
    SettingsTest::runUnknownKeyChecks();
    SettingsTest::runAtomicReplaceChecks();
    SettingsTest::runNewerVersionChecks();
    SettingsTest::runMidiInputNameChecks();

    std::cout << (SettingsTest::failures == 0 ? "\nPASS" : "\nFAIL")
              << ": ihub-settings-store ("
              << SettingsTest::failures << " failure(s))"
              << std::endl;

    return SettingsTest::failures == 0 ? 0 : 1;
}
