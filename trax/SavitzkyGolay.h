#pragma once

#include "Grid.h"
#include <vector>

// Legacy 2D Savitzky-Golay smoother.
//
// A verbatim port of the contour engine's SavitzkyGolay2D filter, kept in Trax
// for backward compatibility. The kernels are fixed integer least-squares
// coefficients for square windows of half-width `size` (1..6, i.e. 3x3..13x13)
// and polynomial `degree` (1..5). Boundaries use an odd-reflection (mirror)
// extrapolation that preserves the local trend (and may overshoot the data
// range); any window touching a NaN keeps the original centre value, and NaN
// centres stay NaN. Because of this the method ignores SmoothOptions::boundary,
// passes and the periodic x-wrap, and always preserves the missing footprint.

namespace Trax
{
// Return the smoothed values (row-major, w*h) for the grid. `size` and `degree`
// are clamped to the supported ranges; a combination with no kernel (e.g.
// size 1 with degree > 2) returns the input values unchanged.
std::vector<float> savitzky_golay(const Grid& grid, long w, long h, int size, int degree);

}  // namespace Trax
