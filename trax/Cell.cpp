#include "Cell.h"
#include <algorithm>  // std::minmax

namespace Trax
{
// A grid cell looks like a saddle point for some value z if that value would intersect
// all the edges. Hence if the intersection of all the intervals from the edges is not empty,
// there is a saddle point.

bool is_saddle(const Cell& cell)
{
  auto lo = std::min(cell.z1, cell.z2);
  auto hi = std::max(cell.z1, cell.z2);
  lo = std::max(lo, std::min(cell.z2, cell.z3));
  hi = std::min(hi, std::max(cell.z2, cell.z3));
  if (lo >= hi)
    return false;
  lo = std::max(lo, std::min(cell.z3, cell.z4));
  hi = std::min(hi, std::max(cell.z3, cell.z4));
  if (lo >= hi)
    return false;
  lo = std::max(lo, std::min(cell.z4, cell.z1));
  hi = std::min(hi, std::max(cell.z4, cell.z1));
  return (hi > lo);
}

// Test which diagonal has larger values
bool first_diagonal_larger(const Cell& cell)
{
  return (cell.z1 + cell.z3 > cell.z2 + cell.z4);
}

// Return minmax values of a grid cell. Returns NaN if all the elements are NaN.
// Note that if a comparison returns true, neither of the arguments can be NaN.
// This can be utilized to avoid std::isnan if none of the numbers is NaN.

inline MinMax minmax(float z1, float z2)
{
  if (z1 <= z2)
    return MinMax(z1, z2);  // z1,z2 neither is NaN
  if (z2 < z1)
    return MinMax(z2, z1);  // z2,z1 neither is NaN
  if (std::isnan(z1))
    return MinMax(z2, z2);  // z2,z2 which could be NaN,NaN
  return MinMax(z1, z1);    // z1,z1 which cannot be NaN,NaN
}

MinMax minmax(const Cell& cell)
{
  auto tmp1 = minmax(cell.z1, cell.z2);
  auto tmp2 = minmax(cell.z3, cell.z4);
  auto tmp3 = minmax(tmp1.first, tmp2.first);
  auto tmp4 = minmax(tmp1.second, tmp2.second);
  return MinMax(tmp3.first, tmp4.second);
}

}  // namespace Trax
