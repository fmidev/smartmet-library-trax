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

// Return minmax values of a grid cell
MinMax minmax(const Cell& cell)
{
  const auto tmp1 = std::minmax(cell.z1, cell.z2);
  const auto tmp2 = std::minmax(cell.z3, cell.z4);
  return std::make_pair(std::min(tmp1.first, tmp2.first), std::max(tmp1.second, tmp2.second));
}

}  // namespace Trax
