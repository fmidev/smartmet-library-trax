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

namespace Trax
{
enum class SmoothMethod : std::uint8_t
{
  None,    // no smoothing (the default-constructed state)
  Box,     // repeated separable box blur via running sums (Gaussian approx)
  Pyramid  // factor-2 reduce/expand pyramid for very large radii
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

struct SmoothOptions
{
  SmoothMethod method = SmoothMethod::None;
  SmoothBoundary boundary = SmoothBoundary::Normalized;

  // Box method: half-width of each box pass, in grid cells. passes>=3
  // approximates a Gaussian (central limit). Effective stddev grows with
  // both radius and passes.
  int radius = 0;
  int passes = 3;

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
    if (method == SmoothMethod::Box)
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
