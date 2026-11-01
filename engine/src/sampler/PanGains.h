#pragma once

namespace ihub::sampler
{

/** Constant-power pan gain for the left channel.

    `pan` is -1 (left) .. +1 (right), `gain` is a linear multiplier applied
    after the pan law.
*/
float panGainLeft (float pan, float gain = 1.0f) noexcept;

/** Constant-power pan gain for the right channel. See `panGainLeft()`. */
float panGainRight (float pan, float gain = 1.0f) noexcept;

} // namespace ihub::sampler
