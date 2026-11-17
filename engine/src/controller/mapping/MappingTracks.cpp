#include "controller/mapping/MappingDecode.h"

#include "controller/mapping/MappingDiagnostics.h"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>

namespace Controller::MappingDetail {

bool decodeTracks(const YamlValue& root, SetMapping& out, MappingError& error) {
    const auto* tracks = root.find("tracks");

    if (tracks == nullptr) return fail(error, root, "tracks is required");

    if (tracks->kind != YamlKind::sequence)
        return fail(error, *tracks, "tracks must be a sequence");

    if (static_cast<int>(tracks->sequence.size()) != SetMapping::maximumTracks)
        return fail(error, *tracks,
                    "tracks must declare exactly " + std::to_string(SetMapping::maximumTracks) +
                        " entries");

    std::array<std::optional<TrackSpec>, SetMapping::maximumTracks> slots;
    std::set<std::string> ids;

    for (const auto& trackValue : tracks->sequence) {
        if (trackValue.kind != YamlKind::mapping)
            return fail(error, trackValue, "a track entry must be a mapping");

        if (!checkKeys(trackValue, {"id", "name", "slot", "hotFx"}, error)) return false;

        const auto* id = trackValue.find("id");
        const auto* name = trackValue.find("name");
        const auto* slot = trackValue.find("slot");

        if (id == nullptr || name == nullptr || slot == nullptr)
            return fail(error, trackValue, "a track entry needs id, name and slot");

        TrackSpec track;

        if (!readString(*id, track.id, error, "track id") ||
            !readString(*name, track.name, error, "track name") ||
            !readInt(*slot, track.slot, error, "track slot"))
            return false;

        if (track.slot < 0 || track.slot >= SetMapping::maximumTracks)
            return fail(
                error, *slot,
                "track slot must be within 0.." + std::to_string(SetMapping::maximumTracks - 1));

        if (ids.find(track.id) != ids.end())
            return fail(error, *id, "duplicate track id '" + track.id + "'");

        if (slots[static_cast<std::size_t>(track.slot)].has_value())
            return fail(error, *slot, "duplicate track slot " + std::to_string(track.slot));

        if (const auto* hotFx = trackValue.find("hotFx")) {
            if (hotFx->kind != YamlKind::sequence)
                return fail(error, *hotFx, "track hotFx must be a sequence");

            if (static_cast<int>(hotFx->sequence.size()) > SetMapping::maximumHotFx)
                return fail(error, *hotFx,
                            "track hotFx takes at most " +
                                std::to_string(SetMapping::maximumHotFx) + " insert ids");

            for (const auto& fxValue : hotFx->sequence) {
                std::string fxId;

                if (!readString(fxValue, fxId, error, "track hotFx insert id", true)) return false;

                if (fxId.empty()) {
                    track.hotFx.push_back({});
                    continue;
                }

                if (std::find(track.hotFx.begin(), track.hotFx.end(), fxId) != track.hotFx.end())
                    return fail(error, fxValue, "duplicate track hotFx insert '" + fxId + "'");

                track.hotFx.push_back(std::move(fxId));
            }
        }

        ids.insert(track.id);
        slots[static_cast<std::size_t>(track.slot)] = std::move(track);
    }

    out.tracks.clear();
    out.tracks.reserve(static_cast<std::size_t>(SetMapping::maximumTracks));

    for (auto& track : slots)
        if (track.has_value()) out.tracks.push_back(std::move(*track));

    return true;
}

}  // namespace Controller::MappingDetail
