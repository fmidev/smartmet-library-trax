#include "Grid.h"
#include <limits>

namespace Trax
{
Grid::~Grid() = default;

std::size_t Grid::shift() const
{
  return 0UL;
}

double Grid::shell() const
{
  // No shell around the grid by default
  return std::numeric_limits<double>::quiet_NaN();
}

// For a 2x2 grid we can iterate only through cell at coordinate 0,0

std::array<long, 4> Grid::bbox() const
{
  return {0L, 0L, static_cast<long>(width()) - 2, static_cast<long>(height()) - 2};
}

}  // namespace Trax
