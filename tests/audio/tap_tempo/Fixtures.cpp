#include "Fixtures.h"

namespace AudioTest::TapTempo
{

double fourTaps (const double* seconds)
{
    AudioEngine::TapTempo tempo;

    for (int index = 0; index < 4; ++index)
        tempo.tapAt (seconds[index]);

    return tempo.getBpm();
}

}
