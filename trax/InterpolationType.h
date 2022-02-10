#pragma once

#include <string>

namespace Trax
{
enum class InterpolationType
{
  Linear,   // linear interpolation
  Midpoint  // nearest neighbour midpoint style, produces smoother and shorter paths
};

InterpolationType to_interpolation_type(const std::string& str);

}  // namespace Trax
