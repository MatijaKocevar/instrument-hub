#include "TrackLayoutSave.h"

#include "TrackLayoutText.h"

#include "controller/MappingYaml.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ihub::app
{

namespace
{

bool mappingHasTrackName (const std::string& yaml,
                          const Api::TrackConfig& track,
                          juce::String& error)
{
    Controller::SetMapping mapping;
    Controller::MappingError mappingError;

    if (! Controller::parseSetMappingYaml (yaml, mapping, mappingError))
    {
        error = "track layout: line " + juce::String (mappingError.line) + ": "
                + juce::String (mappingError.message);
        return false;
    }

    const auto found = std::find_if (mapping.tracks.begin(), mapping.tracks.end(),
                                     [&track] (const Controller::TrackSpec& candidate)
                                     {
                                         return candidate.id == track.id;
                                     });

    if (found == mapping.tracks.end())
    {
        error = "track layout does not contain \"" + juce::String (track.id) + "\"";
        return false;
    }

    if (! track.name.empty() && found->name != track.name)
    {
        error = "track \"" + juce::String (track.id)
                + "\" could not persist its name";
        return false;
    }

    return true;
}

bool mappingHasTrackHotFx (const std::string& yaml,
                           const Api::TrackConfig& track,
                           juce::String& error)
{
    Controller::SetMapping mapping;
    Controller::MappingError mappingError;

    if (! Controller::parseSetMappingYaml (yaml, mapping, mappingError))
    {
        error = "track layout: line " + juce::String (mappingError.line) + ": "
                + juce::String (mappingError.message);
        return false;
    }

    const auto found = std::find_if (mapping.tracks.begin(), mapping.tracks.end(),
                                     [&track] (const Controller::TrackSpec& candidate)
                                     {
                                         return candidate.id == track.id;
                                     });

    if (found == mapping.tracks.end())
    {
        error = "track layout does not contain \"" + juce::String (track.id) + "\"";
        return false;
    }

    for (std::size_t index = 0; index < track.hotFx.size(); ++index)
    {
        const bool expected = ! track.hotFx[index].empty();
        const bool present = index < found->hotFx.size()
                             && ! found->hotFx[index].empty();

        if (expected != present
            || (expected && found->hotFx[index] != track.hotFx[index]))
        {
            error = "track \"" + juce::String (track.id)
                    + "\" could not persist its hot FX list";
            return false;
        }
    }

    return true;
}

}

bool saveTrackLayoutToMapping (const std::string& mappingYaml,
                               const std::vector<Api::TrackConfig>& tracks,
                               std::string& destination,
                               juce::String& error)
{
    destination = mappingYaml;
    error.clear();

    std::vector<TrackLayoutUpdate> updates;
    updates.reserve (tracks.size());

    for (const auto& track : tracks)
        updates.push_back ({ track.id, track.name, track.hotFx });

    if (! rewriteTrackLayout (mappingYaml, updates, destination))
    {
        error = "track layout has no tracks section";
        return false;
    }

    for (const auto& track : tracks)
    {
        if (! mappingHasTrackName (destination, track, error))
            return false;

        if (! mappingHasTrackHotFx (destination, track, error))
            return false;
    }

    return true;
}

}
