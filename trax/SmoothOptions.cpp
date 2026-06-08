#include "SmoothOptions.h"
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>

namespace Trax
{
SmoothMethod to_smooth_method(const std::string& str)
{
  if (str == "none")
    return SmoothMethod::None;
  if (str == "box")
    return SmoothMethod::Box;
  if (str == "pyramid")
    return SmoothMethod::Pyramid;
  throw Fmi::Exception(BCP, "Unknown smoothing method '" + str + "'");
}

SmoothBoundary to_smooth_boundary(const std::string& str)
{
  if (str == "normalized")
    return SmoothBoundary::Normalized;
  if (str == "replicate" || str == "clamp")
    return SmoothBoundary::Replicate;
  if (str == "reflect" || str == "mirror")
    return SmoothBoundary::Reflect;
  throw Fmi::Exception(BCP, "Unknown smoothing boundary '" + str + "'");
}

std::size_t SmoothOptions::hash() const
{
  // Every no-op configuration hashes identically, so a caller's cache treats
  // "no smoothing" as one key regardless of how it was spelled.
  if (!active())
    return Fmi::hash_value(std::string("Trax::SmoothOptions::none"));

  std::size_t h = Fmi::hash_value(std::string("Trax::SmoothOptions"));
  Fmi::hash_combine(h, Fmi::hash_value(static_cast<int>(method)));
  Fmi::hash_combine(h, Fmi::hash_value(static_cast<int>(boundary)));
  Fmi::hash_combine(h, Fmi::hash_value(radius));
  Fmi::hash_combine(h, Fmi::hash_value(passes));
  Fmi::hash_combine(h, Fmi::hash_value(levels));
  Fmi::hash_combine(h, Fmi::hash_value(preserve_missing));
  return h;
}

}  // namespace Trax
