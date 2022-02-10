#pragma once

#include "MirrorMatrix.h"
#include "SavitzkyGolay2DCoefficients.h"
#include <cmath>
#include <stdexcept>

namespace Trax
{
namespace SavitzkyGolay2D
{
// ----------------------------------------------------------------------
/*!
 * \brief Smoothen a matrix using a mirror matrix for boundary conditions
 *
 * Length is limited to range 0..6, degree to 0...5
 */
// ----------------------------------------------------------------------

template <typename Grid>
void smooth(Grid& input, std::size_t length, std::size_t degree)
{
  if (length == 0 || degree == 0)
    return;

  if (length > 6)
    length = 6;
  if (degree > 5)
    degree = 5;

  int* factor = SavitzkyGolay2DCoefficients::coeffs[length - 1][degree - 1];
  if (factor == 0)
    return;

  // Smoothen back to input from a copy of the original

  auto grid = input;
  MirrorMatrix<Grid> mirror(grid);

  int n = 2 * length + 1;

  int denom = SavitzkyGolay2DCoefficients::denoms[length - 1][degree - 1];

  for (std::size_t jj = 0; jj < grid.height(); ++jj)
    for (std::size_t ii = 0; ii < grid.width(); ++ii)
    {
      double sum = 0;
      int k = 0;
      for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
          sum += (factor[k++] * mirror(ii + i - length, jj + j - length));
      if (!std::isnan(sum))
        input.set(ii, jj, sum / denom);
    }
}
}  // namespace SavitzkyGolay2D
}  // namespace Trax
