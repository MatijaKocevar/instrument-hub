#pragma once

#include "controller/MappingModel.h"

#include <optional>

namespace Controller
{

enum class TrackPadActionKind
{
    arm,
    toggleFx,
    setVolume
};

struct TrackPadAction
{
    TrackPadActionKind kind = TrackPadActionKind::arm;
    int slot = -1;
    int hot = -1;
    int step = -1;
};

bool padRoleArms (PadRole role) noexcept;

std::optional<TrackPadAction> trackPadActionFor (const PadBinding& binding,
                                                 int currentVolumeStep) noexcept;

}
