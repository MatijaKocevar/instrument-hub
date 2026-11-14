#pragma once

namespace ihub
{

    struct SessionSwitchReport
    {
        int fromSlot = -1;
        int toSlot = -1;

        double rms = 0.0;
        double rmsDbfs = -1000.0;

        double maxSampleDelta = 0.0;
        double maxSampleDeltaDbfs = -1000.0;

        double discontinuity = 0.0;
        double discontinuityDbfs = -1000.0;

        int samplesMeasured = 0;
        bool rendered = false;
    };

}
