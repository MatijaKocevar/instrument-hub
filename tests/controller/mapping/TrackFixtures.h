#pragma once

#include <string>

namespace MappingTest
{

std::string makeTrackHeader();
std::string makeTrackPads();
std::string makeTracksYaml (bool includeEmptySections = false);

std::string replaceOnce (std::string text,
                         const std::string& from,
                         const std::string& to);

int lineOf (const std::string& text, const std::string& needle);
int lineOfOccurrence (const std::string& text, const std::string& needle, int occurrence);

}
