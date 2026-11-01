#pragma once

namespace ihub
{

/** Measurements from an offline armed-slot / slot-switch render. */
struct SessionSwitchReport
{
    int fromSlot = -1;
    int toSlot = -1;

    /** RMS of the armed slot before the switch, linear and in dBFS. */
    double rms = 0.0;
    double rmsDbfs = -1000.0;

    /** Largest sample-to-sample step seen anywhere in the measured span.

        This is the signal's ordinary slew plus any discontinuity; it is
        reported for context, not used as the click decision.
    */
    double maxSampleDelta = 0.0;
    double maxSampleDeltaDbfs = -1000.0;

    /** Largest discontinuity across the switch.

        The rendered switch is compared with the ideal equal-power crossfade
        of the same two source signals: each source is rendered in isolation
        from the same prepared state, then blended with the 3 ms product law.
        The value is the peak absolute difference between the real session
        output and that reference, in dBFS. A click, a hard switch or a broken
        ramp shows up as a large residual; ordinary signal curvature cancels.
    */
    double discontinuity = 0.0;
    double discontinuityDbfs = -1000.0;

    int samplesMeasured = 0;
    bool rendered = false;
};

} // namespace ihub
