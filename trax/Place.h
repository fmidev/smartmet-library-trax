#pragma once
#include "Range.h"
#include <cmath>
#include <iostream>

namespace Trax
{
class Range;

enum class Place
{
  Below = 0,
  Inside = 1,
  Above = 2,
  Invalid = 3
};

// Isoband placement
inline Place place(float value, const Range& range)
{
  if (value < range.lo())
    return Place::Below;
  if (value >= range.hi())
    return Place::Above;
  if (!std::isnan(value))
    return Place::Inside;
  return Place::Invalid;
}

// Discrete placement is either below, inside or invalid
Place discrete_place(float value, const Range& range);

// Isovalue placement
Place place(float value, float limit);

// TODO: Need to test whether constexpr would work on RHEL7
#define TRAX_RECT_HASH(c1, c2, c3, c4) \
  (static_cast<int>((c1)) +            \
   4 * (static_cast<int>((c2)) + 4 * (static_cast<int>((c3)) + 4 * static_cast<int>((c4)))))

#define TRAX_TRIANGLE_HASH(c1, c2, c3) \
  (static_cast<int>((c1)) + 4 * (static_cast<int>((c2)) + 4 * static_cast<int>((c3))))

#define TRAX_EDGE_HASH(c1, c2) (static_cast<int>((c1)) + 4 * static_cast<int>((c2)))

inline int place_hash(Place c1, Place c2, Place c3, Place c4)
{
  return TRAX_RECT_HASH(c1, c2, c3, c4);
}

inline int place_hash(Place c1, Place c2, Place c3)
{
  return TRAX_TRIANGLE_HASH(c1, c2, c3);
}

inline int place_hash(Place c1, Place c2)
{
  return TRAX_EDGE_HASH(c1, c2);
}

inline int nan_hash(float z)
{
  return static_cast<int>(std::isnan(z) ? Place::Inside : Place::Below);
}

// The hash value must match the rect hash above for Inside and Below, hence times 4 instead of 2
inline int nan_hash(float z1, float z2, float z3, float z4)
{
  return nan_hash(z1) + 4 * (nan_hash(z2) + 4 * (nan_hash(z3) + 4 * nan_hash(z4)));
}

std::string to_string(Place place);

}  // namespace Trax
