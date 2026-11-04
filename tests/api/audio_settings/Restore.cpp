#include "Restore.h"

#include "Profiles.h"
#include "RestoreSupport.h"

namespace ApiTest::AudioSettings
{

void runRestore()
{
    runProfileRestore();
    runStoredDeviceRestart();
    runMissingStoredFallback();
    runFallbackOrder();
    runUnopenableStoredFallback();
    runCliPrecedence();
    runCliTypePrecedence();
    runCliTypeDropsStoredName();
    runCliFormatPrecedence();
    runCliFormatOnFallback();
    runCliDeviceDropsStoredType();
    runCliFailureDoesNotFallBack();
    runBuiltInDefault();
}

}
