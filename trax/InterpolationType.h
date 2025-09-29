#pragma once

#include <cstdint>
#include <string>

namespace Trax
{
enum class InterpolationType : std::uint8_t
{
  Linear,      // linear interpolation
  Midpoint,    // nearest neighbour midpoint style, produces smoother and shorter paths
  Logarithmic  // for radar data with exponential behaviour in values
};

InterpolationType to_interpolation_type(const std::string& str);

}  // namespace Trax
