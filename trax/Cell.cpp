#include "Cell.h"
#include <algorithm>  // std::minmax
#include <cmath>

namespace Trax
{
// A grid cell looks like a saddle point for some value z if that value would intersect
// all the edges. Hence if the intersection of all the intervals from the edges is not empty,
// there is a saddle point.

bool is_saddle(const Cell& cell)
{
  auto lo = std::min(cell.p1.z, cell.p2.z);
  auto hi = std::max(cell.p1.z, cell.p2.z);
  lo = std::max(lo, std::min(cell.p2.z, cell.p3.z));
  hi = std::min(hi, std::max(cell.p2.z, cell.p3.z));
  if (lo >= hi)
    return false;
  lo = std::max(lo, std::min(cell.p3.z, cell.p4.z));
  hi = std::min(hi, std::max(cell.p3.z, cell.p4.z));
  if (lo >= hi)
    return false;
  lo = std::max(lo, std::min(cell.p4.z, cell.p1.z));
  hi = std::min(hi, std::max(cell.p4.z, cell.p1.z));
  return (hi > lo);
}

// Test which diagonal has larger values
bool first_diagonal_larger(const Cell& cell)
{
  return (cell.p1.z + cell.p3.z > cell.p2.z + cell.p4.z);
}

// Return minmax values of a grid cell. Returns NaN if all the elements are NaN.
// Note that if a comparison returns true, neither of the arguments can be NaN.
// This can be utilized to avoid std::isnan if none of the numbers is NaN.

inline MinMax minmax(float z1, float z2)
{
  if (z1 <= z2)
    return {z1, z2};  // z1,z2 neither is NaN
  // codechecker_false_positive [knownConditionTrueFalse]
  if (z2 < z1)
    return {z2, z1};  // z2,z1 neither is NaN
  if (std::isnan(z1))
    return {z2, z2};  // z2,z2 which could be NaN,NaN
  return {z1, z1};    // z1,z1 which cannot be NaN,NaN
}

MinMax minmax(const Cell& cell)
{
  auto tmp1 = minmax(cell.p1.z, cell.p2.z);
  auto tmp2 = minmax(cell.p3.z, cell.p4.z);
  auto tmp3 = minmax(tmp1.first, tmp2.first);
  auto tmp4 = minmax(tmp1.second, tmp2.second);
  return {tmp3.first, tmp4.second};
}

}  // namespace Trax
