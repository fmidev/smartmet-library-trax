#pragma once
#include "Range.h"

namespace Trax
{
class Range;

enum class Place
{
  Below = 0,
  Inside = 1,
  Above = 2
};

// Isoband placement
inline Place place(double value, const Range& range)
{
  if (value < range.lo())
    return Place::Below;
  if (value >= range.hi())
    return Place::Above;
  return Place::Inside;
}

// Discrete placement is either below or inside
Place discrete_place(double value, const Range& range);

// Isovalue placement
Place place(double value, double limit);

// TODO: Need to test whether constexpr would work on RHEL7
#define TRAX_PLACE_HASH(c1, c2, c3, c4) \
  static_cast<int>(c1) + 3 * (static_cast<int>(c2) + 3 * (static_cast<int>(c3) + 3 * static_cast<int>(c4)))

inline int place_hash(Place c1, Place c2, Place c3, Place c4)
{
  return TRAX_PLACE_HASH(c1, c2, c3, c4);
}

}  // namespace Trax
