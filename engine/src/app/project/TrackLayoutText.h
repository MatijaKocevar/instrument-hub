#pragma once

#include <string>
#include <vector>

namespace ihub::app {

struct TrackLayoutUpdate {
  std::string trackId;
  std::string name;
  std::vector<std::string> hotFx;
};

bool rewriteTrackLayout(const std::string& mappingYaml,
                        const std::vector<TrackLayoutUpdate>& updates,
                        std::string& destination);

}  // namespace ihub::app
