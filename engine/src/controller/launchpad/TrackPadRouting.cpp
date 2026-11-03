#include "controller/launchpad/TrackPadRouting.h"

namespace Controller
{

bool padRoleArms (PadRole role) noexcept
{
    return role == PadRole::patch || role == PadRole::track;
}

std::optional<TrackPadAction> trackPadActionFor (const PadBinding& binding,
                                                 int currentVolumeStep) noexcept
{
    if (binding.slot < 0)
        return std::nullopt;

    TrackPadAction action;
    action.slot = binding.slot;

    switch (binding.role)
    {
        case PadRole::patch:
        case PadRole::track:
            action.kind = TrackPadActionKind::arm;
            return action;

        case PadRole::fx:
            if (binding.hot < 0 || binding.hot >= SetMapping::maximumHotFx)
                return std::nullopt;

            action.kind = TrackPadActionKind::toggleFx;
            action.hot = binding.hot;
            return action;

        case PadRole::volume:
            if (binding.step < 0 || binding.step > SetMapping::maximumVolumeStep)
                return std::nullopt;

            action.kind = TrackPadActionKind::setVolume;
            action.step = binding.step;

            if (binding.step == SetMapping::maximumVolumeStep)
            {
                if (currentVolumeStep == SetMapping::maximumVolumeStep)
                    action.step = SetMapping::maximumVolumeStep + 1;
                else if (currentVolumeStep == SetMapping::maximumVolumeStep + 1)
                    action.step = SetMapping::maximumVolumeStep;
            }

            return action;
    }

    return std::nullopt;
}

}
