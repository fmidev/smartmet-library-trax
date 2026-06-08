#pragma once

#include <cstdint>
#include <string>

// Options controlling grid smoothing performed before contouring.
//
// Smoothing happens in grid *index* space (cells), not in projected
// coordinate units, since both supported methods are resampling/averaging
// operators tied to the grid lattice. If a consumer wants a physical
// smoothing distance it maps that distance to a cell count using its own
// knowledge of the grid spacing before filling these options.
//
// The default method is a separable, NaN- and boundary-aware box blur
// ("normalized convolution"): cheap (O(N) per pass, independent of radius
// via running sums), separable, introduces no spurious extrema, and treats
// out-of-grid and missing (NaN) samples identically as zero-weight. Three
// passes approximate a Gaussian. The Pyramid method is offered for very
// large effective radii where the geometric work reduction matters.
//
// Box (and Pyramid) are linear low-pass filters: they never create new
// extrema but they *attenuate* existing ones (a sharp peak is flattened
// toward its neighbourhood mean). When the magnitude of real peaks and pits
// must survive, use one of the rank/morphological methods instead:
//
//   Median     - per-window median. Removes spikes/salt-and-pepper of either
//                sign, keeps step edges sharp (output is always an existing
//                input value, so no overshoot and no new value is invented),
//                and preserves the value of any feature broader than the
//                window. The 2D window is exact (not a separable approxi-
//                mation), so cost grows with radius.
//   Morphology - grayscale opening/closing with a flat box structuring
//                element, built from separable running min/max (van-Herk-
//                style monotonic window): O(N) per pass, radius-independent,
//                exactly separable. Opening removes bright features smaller
//                than the element while preserving the value of larger ones;
//                closing does the same for dark features. OpenClose applies
//                both for a symmetric edge-preserving smoothing.
//
// All methods are NaN- and boundary-aware and honour preserve_missing and the
// periodic x-wrap exactly like Box.

namespace Trax
{
enum class SmoothMethod : std::uint8_t
{
  None,        // no smoothing (the default-constructed state)
  Box,         // repeated separable box blur via running sums (Gaussian approx)
  Median,      // per-window median: extremum/edge-preserving spike removal
  Morphology,  // grayscale opening/closing via separable running min/max
  Pyramid      // factor-2 reduce/expand pyramid for very large radii
};

// Which morphological operation Morphology performs. A flat box structuring
// element of half-width `radius` is used in every case.
enum class MorphologyOp : std::uint8_t
{
  Open,      // erode then dilate: removes bright peaks smaller than the element
  Close,     // dilate then erode: fills dark pits smaller than the element
  OpenClose  // open then close: symmetric removal of small bright and dark features
};

// How out-of-grid (and NaN) samples are treated by the kernel. Periodicity
// in the x direction is handled automatically when the source grid reports a
// non-zero shift() (global wrap-around data) and overrides this setting on
// the x axis.
enum class SmoothBoundary : std::uint8_t
{
  Normalized,  // renormalize over in-range, non-NaN samples only (recommended)
  Replicate,   // clamp to nearest edge value (Neumann, zero-flux)
  Reflect      // mirror edge values (reflect-101). NB: this is the stable even mirror,
               // not MirrorGrid's gradient-extrapolating odd reflection, which is
               // intentionally not offered because it can overshoot the data range.
};

SmoothMethod to_smooth_method(const std::string& str);
SmoothBoundary to_smooth_boundary(const std::string& str);
MorphologyOp to_morphology_op(const std::string& str);

struct SmoothOptions
{
  SmoothMethod method = SmoothMethod::None;
  SmoothBoundary boundary = SmoothBoundary::Normalized;

  // Box/Median/Morphology: half-width of the window (box pass, median window,
  // or structuring element), in grid cells. For Box, passes>=3 approximates a
  // Gaussian (central limit) and the effective stddev grows with both radius
  // and passes. For Median and Morphology, passes is the number of times the
  // whole operation is repeated (idempotent enough that passes==1 is usual).
  int radius = 0;
  int passes = 3;

  // Morphology method: which operation to apply (see MorphologyOp).
  MorphologyOp morphology = MorphologyOp::OpenClose;

  // Pyramid method: number of reduce/expand levels. Effective smoothing
  // radius roughly doubles per level, so levels = ceil(log2(radius)).
  int levels = 0;

  // Preserve the original missing-data footprint: cells that were NaN in the
  // input stay NaN in the output even if the kernel could have filled them
  // from valid neighbours. Recommended so smoothing does not bleed data
  // across coastlines / radar edges / land-sea masks.
  bool preserve_missing = true;

  // True when these options would actually change the data.
  bool active() const
  {
    if (method == SmoothMethod::Box || method == SmoothMethod::Median ||
        method == SmoothMethod::Morphology)
      return radius > 0 && passes > 0;
    if (method == SmoothMethod::Pyramid)
      return levels > 0;
    return false;
  }

  // Hash of the options, to be combined by the caller with its own grid-data
  // hash to key an external cache. Smoothing depends only on values + kernel,
  // never on coordinates, so this deliberately excludes anything geometric.
  std::size_t hash() const;
};

}  // namespace Trax
