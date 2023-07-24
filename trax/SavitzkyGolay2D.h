#pragma once

#include "MirrorMatrix.h"
#include "SavitzkyGolay2DCoefficients.h"
#include <cmath>
#include <stdexcept>
#include <vector>

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

  // Reflect data at the borders for better results
  MirrorMatrix<Grid> mirror(input);

  int n = 2 * length + 1;
  const auto nx = input.width();
  const auto ny = input.height();

  // Holder for temporary results
  std::vector<float> sums;
  sums.reserve(nx * ny);

  int denom = SavitzkyGolay2DCoefficients::denoms[length - 1][degree - 1];

  // Calculate the smoothened values
  for (std::size_t jj = 0; jj < ny; ++jj)
    for (std::size_t ii = 0; ii < nx; ++ii)
    {
      float sum = 0;
      int k = 0;
      for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
          sum += (factor[k++] * mirror(ii + i - length, jj + j - length));
      sums.push_back(sum);
    }

  // Replace old values with new ones
  std::size_t pos = 0;
  for (std::size_t jj = 0; jj < ny; ++jj)
    for (std::size_t ii = 0; ii < nx; ++ii, ++pos)
      if (!std::isnan(sums[pos]))
        input.set(ii, jj, sums[pos] / denom);
}
}  // namespace SavitzkyGolay2D
}  // namespace Trax
