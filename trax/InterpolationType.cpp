#include "InterpolationType.h"
#include <smartmet/macgyver/Exception.h>
#include <stdexcept>

namespace Trax
{
InterpolationType to_interpolation_type(const std::string& str)
{
  if (str == "linear")
    return InterpolationType::Linear;
  if (str == "nearest" || str == "discrete" || str == "midpoint")
    return InterpolationType::Midpoint;
  if (str == "logarithmic")
    return InterpolationType::Logarithmic;
  throw Fmi::Exception(BCP, "Unknown interpolation type '" + str + "'");
}
}  // namespace Trax
