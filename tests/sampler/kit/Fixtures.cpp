#include "Fixtures.h"

namespace KitTest
{

std::string baseKitWithLayers (const std::string& firstLayer,
                               const std::string& secondLayer)
{
    return R"({
      "schemaVersion": 1,
      "name": "Test Kit",
      "pads": [
        {
          "note": 36,
          "name": "Kick",
          "layers": [)"
           + firstLayer + "," + secondLayer
           + R"(]
        }
      ]
    })";
}

const std::string softLayer = R"({
  "minVelocity": 1,
  "maxVelocity": 63,
  "roundRobin": [
    { "path": "kick-soft-a.wav" },
    { "path": "kick-soft-b.wav" }
  ]
})";

const std::string hardLayer = R"({
  "minVelocity": 64,
  "maxVelocity": 127,
  "roundRobin": [
    { "path": "kick-hard.wav" }
  ]
})";

std::string twoLayerJson()
{
    return baseKitWithLayers (softLayer, hardLayer);
}

}
