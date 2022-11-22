#pragma once

#include <string>

namespace Trax
{
enum class InterpolationType
{
  Linear,      // linear interpolation
  Midpoint,    // nearest neighbour midpoint style, produces smoother and shorter paths
  Logarithmic  // for radar data with exponential behaviour in values
};

InterpolationType to_interpolation_type(const std::string& str);

}  // namespace Trax
