#pragma once

#include <string>

namespace KitTest
{

std::string baseKitWithLayers (const std::string& firstLayer,
                               const std::string& secondLayer);

std::string twoLayerJson();

extern const std::string softLayer;
extern const std::string hardLayer;

} // namespace KitTest
