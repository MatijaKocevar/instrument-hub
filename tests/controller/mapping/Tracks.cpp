#include "Tracks.h"

#include "TrackDocument.h"
#include "TrackValidation.h"

namespace MappingTest
{

void runTrackChecks()
{
    runTrackDocumentChecks();
    runTrackValidationChecks();
}

}
