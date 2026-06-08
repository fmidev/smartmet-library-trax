#include "Smoother.h"
#include "BufferGrid.h"
#include <macgyver/Exception.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// Box normalized-convolution smoother.
//
// We carry two fields through the separable, multi-pass box filter: a
// numerator (value where valid, 0 where missing) and a denominator/weight
// (1 where valid, 0 where missing). The same linear box operator is applied
// to both, and the smoothed estimate is num/den at the end. Because both
// fields are scaled by the same kernel-weight sum, plain windowed *sums* (no
// per-window averaging) suffice and the normalization cancels in the ratio.
// This makes the Normalized boundary trivial: samples outside the grid (and
// NaN samples) simply contribute 0 to both sums, i.e. the kernel renormalizes
// over whatever real data the window covers.
//
// Boundary handling is applied at the grid edges, except in x for globally
// periodic grids: when source->xperiod() > 0 the x passes wrap with that period
// (the number of distinct columns) instead of using the boundary rule. A grid
// with a duplicated wrap column (width() == period + 1) is convolved over the
// distinct columns only, and the duplicate column is set equal to its wrapped
// counterpart afterwards so no value is double-counted. Grids that do not
// override xperiod() report 0 and keep the edge behaviour. The y axis is never
// wrapped.
//
// Periodicity assumption: operator()(i,j) must return columns in rotational
// longitude order, so that wrapping by modulo period equals physical
// adjacency, and any duplicate wrap column is the last one. This holds for the
// global grids in this stack (e.g. ShiftedGrid reads matrix[i % NX]), and is
// independent of the Atlantic/Pacific view, since the view rotation is applied
// to coordinates and to the contouring iteration, not to the value ordering.

namespace Trax
{
namespace
{
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// Map an out-of-range index to its reflect-101 source index (no edge repeat),
// folding repeatedly so it works for any radius relative to the line length.
long mirror_index(long k, long n)
{
  if (n == 1)
    return 0;
  const long period = 2 * (n - 1);
  long m = k % period;
  if (m < 0)
    m += period;
  if (m >= n)
    m = period - m;
  return m;
}

// One separable box pass of half-width r along a single line of n samples with
// the given stride, operating in place on the numerator and denominator.
//
// When period == 0 the line is treated as non-periodic: the halo is filled per
// boundary mode. When period > 0 the line is periodic with that many distinct
// samples; the halo wraps modulo period, only the distinct samples are
// convolved, and any trailing duplicate samples (n > period) are set equal to
// their wrapped counterparts so they are not double-counted.
//
// The line is first copied into the extended scratch buffers (en/ed), then a
// sliding window sum of width 2r+1 is written back. The scratch buffers must
// hold at least eff + 2r elements, where eff = (period > 0 ? period : n).
void box_line(double* num,
              double* den,
              long n,
              long stride,
              int r,
              SmoothBoundary boundary,
              long period,
              std::vector<double>& en,
              std::vector<double>& ed)
{
  const long eff = (period > 0) ? period : n;  // distinct samples to convolve
  const long pad = r;
  const long m = eff + 2 * pad;

  for (long t = 0; t < m; ++t)
  {
    const long k = t - pad;  // logical index in [-pad, eff-1+pad]
    long src = -1;
    if (period > 0)
    {
      long mm = k % period;  // wrap into [0, period)
      if (mm < 0)
        mm += period;
      src = mm;
    }
    else if (k >= 0 && k < eff)
      src = k;
    else if (boundary == SmoothBoundary::Replicate)
      src = std::clamp(k, 0L, eff - 1);
    else if (boundary == SmoothBoundary::Reflect)
      src = mirror_index(k, eff);
    // Normalized: src stays -1 -> contributes zero weight

    if (src < 0)
    {
      en[t] = 0.0;
      ed[t] = 0.0;
    }
    else
    {
      en[t] = num[src * stride];
      ed[t] = den[src * stride];
    }
  }

  // Sliding window sum: output i covers extended indices [i, i + 2r].
  double sn = 0.0;
  double sd = 0.0;
  for (long t = 0; t <= 2 * r; ++t)
  {
    sn += en[t];
    sd += ed[t];
  }
  num[0] = sn;
  den[0] = sd;
  for (long i = 1; i < eff; ++i)
  {
    sn += en[i + 2 * r] - en[i - 1];
    sd += ed[i + 2 * r] - ed[i - 1];
    num[i * stride] = sn;
    den[i * stride] = sd;
  }

  // Duplicate wrap column(s): copy from the matching distinct sample.
  for (long c = eff; c < n; ++c)
  {
    num[c * stride] = num[(c - period) * stride];
    den[c * stride] = den[(c - period) * stride];
  }
}

// Copy the grid into the numerator (value, or 0 if missing) and denominator
// (1, or 0 if missing) fields, recording the original missing-data footprint.
void materialize(const Grid& grid,
                 long w,
                 long h,
                 std::vector<double>& num,
                 std::vector<double>& den,
                 std::vector<char>& missing)
{
  for (long j = 0; j < h; ++j)
    for (long i = 0; i < w; ++i)
    {
      const auto idx = static_cast<std::size_t>(i) + static_cast<std::size_t>(w) * j;
      const float v = grid(i, j);
      const bool nan = std::isnan(v);
      num[idx] = nan ? 0.0 : v;
      den[idx] = nan ? 0.0 : 1.0;
      if (!missing.empty())
        missing[idx] = nan ? 1 : 0;
    }
}

// Form the smoothed estimate num/den, restoring the missing-data footprint.
std::vector<float> finalize(const std::vector<double>& num,
                            const std::vector<double>& den,
                            const std::vector<char>& missing)
{
  std::vector<float> out(num.size());
  for (std::size_t idx = 0; idx < out.size(); ++idx)
  {
    if (!missing.empty() && missing[idx] != 0)
      out[idx] = kNaN;
    else
      out[idx] = (den[idx] > 0.0) ? static_cast<float>(num[idx] / den[idx]) : kNaN;
  }
  return out;
}

}  // namespace

std::shared_ptr<const Grid> smooth(std::shared_ptr<const Grid> source, const SmoothOptions& opts)
{
  if (!source)
    throw Fmi::Exception(BCP, "Trax::smooth called with a null grid");

  if (!opts.active())
    return source;

  if (opts.method != SmoothMethod::Box)
    throw Fmi::Exception(BCP, "Trax::smooth: only the Box method is implemented");

  const long w = static_cast<long>(source->width());
  const long h = static_cast<long>(source->height());
  if (w <= 0 || h <= 0)
    return source;

  const int r = opts.radius;
  const auto n = static_cast<std::size_t>(w) * h;

  // Periodic x-wrap for global grids. A grid reporting a period outside (0, w]
  // is treated as non-periodic rather than trusted blindly.
  const auto xp = static_cast<long>(source->xperiod());
  const long period = (xp >= 1 && xp <= w) ? xp : 0;

  std::vector<double> num(n);
  std::vector<double> den(n);
  std::vector<char> missing(opts.preserve_missing ? n : 0);
  materialize(*source, w, h, num, den, missing);

  // Scratch reused across all lines and passes.
  const long maxdim = std::max(w, h);
  std::vector<double> en(static_cast<std::size_t>(maxdim) + 2L * r);
  std::vector<double> ed(static_cast<std::size_t>(maxdim) + 2L * r);

  for (int p = 0; p < opts.passes; ++p)
  {
    for (long j = 0; j < h; ++j)
      box_line(&num[static_cast<std::size_t>(w) * j],
               &den[static_cast<std::size_t>(w) * j],
               w,
               1,
               r,
               opts.boundary,
               period,  // x wraps for global grids
               en,
               ed);
    for (long i = 0; i < w; ++i)
      box_line(&num[i], &den[i], h, w, r, opts.boundary, 0, en, ed);  // y never wraps
  }

  return std::make_shared<BufferGrid>(std::move(source), finalize(num, den, missing));
}

}  // namespace Trax
