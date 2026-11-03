#include "Restore.h"

#include "RestoreSupport.h"

namespace ApiTest::AudioSettings
{

void runRestore()
{
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
