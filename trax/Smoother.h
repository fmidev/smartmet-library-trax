#pragma once

#include "Grid.h"
#include "SmoothOptions.h"
#include <memory>

// Grid -> Grid smoothing transform.
//
// Smoothing is exposed independently of Contour so that any consumer can
// smooth a field and then contour it (or use it for anything else) without
// going through a higher-level engine. The Grid interface is the only
// contract: any Grid in, an owned Grid out.
//
// Implementation outline (for the eventual .cpp, here only so the interface
// intent is clear):
//   1. Copy the source values + a validity mask (1 where !isnan(value), else 0)
//      into contiguous buffers once, so the hot convolution loop never pays
//      virtual dispatch.
//   2. Run the chosen separable filter as normalized convolution: blur
//      value*mask and mask with the same kernel, then divide. Out-of-range and
//      NaN samples are both weight 0; the x axis wraps instead of using the
//      boundary rule when source.shift() != 0.
//   3. If preserve_missing, restore NaN wherever the input value was NaN.
//   4. Wrap the result in a BufferGrid sharing `source` for geometry.

namespace Trax
{
// Primary entry point. The source is retained (shared) by the result for its
// coordinates, so coordinates are never copied. If opts.active() is false the
// source is returned unchanged (no copy, no work).
std::shared_ptr<const Grid> smooth(std::shared_ptr<const Grid> source, const SmoothOptions& opts);

}  // namespace Trax
