#include "Smoother.h"
#include "BufferGrid.h"
#include <macgyver/Exception.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <vector>

// Grid smoothers: Box (linear, low-pass) plus the extremum/edge-preserving
// Median and Morphology methods. All share the same NaN handling, boundary
// rules, periodic x-wrap and preserve_missing semantics; only the per-window
// operator differs (running sum vs. window median vs. running min/max).
//
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

// Map a logical line index k (possibly outside [0, n)) to the source index it
// reads from, or -1 when it contributes nothing (Normalized boundary halo).
// When period > 0 the line is globally periodic and wraps modulo period,
// overriding the boundary rule; n is then ignored. Shared by every smoother so
// they agree on boundaries, periodicity and the out-of-range "skip" sentinel.
long map_index(long k, long n, SmoothBoundary boundary, long period)
{
  if (period > 0)
  {
    long mm = k % period;
    if (mm < 0)
      mm += period;
    return mm;
  }
  if (k >= 0 && k < n)
    return k;
  if (boundary == SmoothBoundary::Replicate)
    return std::clamp(k, 0L, n - 1);
  if (boundary == SmoothBoundary::Reflect)
    return mirror_index(k, n);
  return -1;  // Normalized: out-of-range contributes zero weight / is skipped
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
    const long src = map_index(k, eff, boundary, period);
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

// ---------------------------------------------------------------------------
// Median and Morphology share a single value buffer (NaN where missing) rather
// than the box filter's numerator/denominator pair, since neither renormalizes.
// ---------------------------------------------------------------------------

// Copy the grid into a single value buffer, preserving NaN, and record the
// original missing-data footprint (when preserve_missing is requested).
void materialize_values(
    const Grid& grid, long w, long h, std::vector<double>& val, std::vector<char>& missing)
{
  for (long j = 0; j < h; ++j)
    for (long i = 0; i < w; ++i)
    {
      const auto idx = static_cast<std::size_t>(i) + static_cast<std::size_t>(w) * j;
      const float v = grid(i, j);
      val[idx] = std::isnan(v) ? kNaN : static_cast<double>(v);
      if (!missing.empty())
        missing[idx] = std::isnan(v) ? 1 : 0;
    }
}

// Convert a value buffer to floats, restoring the missing-data footprint.
std::vector<float> finalize_values(const std::vector<double>& val, const std::vector<char>& missing)
{
  std::vector<float> out(val.size());
  for (std::size_t idx = 0; idx < out.size(); ++idx)
  {
    if (!missing.empty() && missing[idx] != 0)
      out[idx] = kNaN;
    else
      out[idx] = std::isnan(val[idx]) ? kNaN : static_cast<float>(val[idx]);
  }
  return out;
}

// One separable running min/max pass of half-width r along a single line, the
// morphological counterpart of box_line. NaN samples never become candidates,
// so the window's extreme is taken over the real data it covers (NaN only where
// the whole window is missing). Boundary/periodicity follow map_index. A
// monotonic-deque sliding window makes this O(n), independent of r.
//
// scratch e must hold at least eff + 2r elements; dq is reused scratch.
void minmax_line(double* val,
                 long n,
                 long stride,
                 int r,
                 SmoothBoundary boundary,
                 long period,
                 bool take_max,
                 std::vector<double>& e,
                 std::deque<long>& dq)
{
  const long eff = (period > 0) ? period : n;  // distinct samples to convolve
  const long pad = r;
  const long m = eff + 2 * pad;

  for (long t = 0; t < m; ++t)
  {
    const long src = map_index(t - pad, eff, boundary, period);
    e[t] = (src < 0) ? kNaN : val[src * stride];
  }

  // Output i covers extended indices [i, i + 2r]; the deque front is its
  // extreme. Push non-NaN indices (popping dominated tail entries), evict the
  // front once it leaves the window.
  dq.clear();
  for (long t = 0; t < m; ++t)
  {
    const double cur = e[t];
    if (!std::isnan(cur))
    {
      if (take_max)
        while (!dq.empty() && e[dq.back()] <= cur)
          dq.pop_back();
      else
        while (!dq.empty() && e[dq.back()] >= cur)
          dq.pop_back();
      dq.push_back(t);
    }
    const long i = t - 2 * r;
    if (i >= 0)
    {
      while (!dq.empty() && dq.front() < i)
        dq.pop_front();
      val[i * stride] = dq.empty() ? kNaN : e[dq.front()];
    }
  }

  // Duplicate wrap column(s): copy from the matching distinct sample.
  for (long c = eff; c < n; ++c)
    val[c * stride] = val[(c - period) * stride];
}

// A full 2D erosion (take_max=false) or dilation (take_max=true) over a flat
// box element: separable, so min/max along x then along y is exact. The y axis
// never wraps, matching box.
void morph_op(std::vector<double>& val,
              long w,
              long h,
              int r,
              SmoothBoundary boundary,
              long period,
              bool take_max,
              std::vector<double>& e,
              std::deque<long>& dq)
{
  for (long j = 0; j < h; ++j)
    minmax_line(&val[static_cast<std::size_t>(w) * j], w, 1, r, boundary, period, take_max, e, dq);
  for (long i = 0; i < w; ++i)
    minmax_line(&val[i], h, w, r, boundary, 0, take_max, e, dq);
}

// Collect the non-NaN samples of the (2r+1)x(2r+1) window centred on (i,j)
// into `window`, honouring the boundary rule and periodic x-wrap (y never
// wraps, matching box).
void gather_window(const std::vector<double>& src,
                   long w,
                   long h,
                   long i,
                   long j,
                   int r,
                   SmoothBoundary boundary,
                   long period,
                   std::vector<double>& window)
{
  window.clear();
  for (long dj = -r; dj <= r; ++dj)
  {
    const long jj = map_index(j + dj, h, boundary, 0);
    if (jj < 0)
      continue;
    for (long di = -r; di <= r; ++di)
    {
      const long ii = map_index(i + di, w, boundary, period);
      if (ii < 0)
        continue;
      const double v = src[static_cast<std::size_t>(ii) + static_cast<std::size_t>(w) * jj];
      if (!std::isnan(v))
        window.push_back(v);
    }
  }
}

// Median of the gathered samples (mutates `window` via nth_element). Even-sized
// windows average the two central order statistics; empty windows yield NaN.
double window_median(std::vector<double>& window)
{
  if (window.empty())
    return kNaN;
  const auto k = window.size() / 2;
  std::nth_element(window.begin(), window.begin() + k, window.end());
  const double hi = window[k];
  if (window.size() % 2 != 0)
    return hi;
  const double lo = *std::max_element(window.begin(), window.begin() + k);
  return 0.5 * (lo + hi);
}

// One median pass over an exact 2D window (not a separable approximation),
// written from src into dst. The window is small in practice, so per-cell
// selection via nth_element is adequate.
void median_pass(const std::vector<double>& src,
                 std::vector<double>& dst,
                 long w,
                 long h,
                 int r,
                 SmoothBoundary boundary,
                 long period,
                 std::vector<double>& window)
{
  for (long j = 0; j < h; ++j)
    for (long i = 0; i < w; ++i)
    {
      gather_window(src, w, h, i, j, r, boundary, period, window);
      dst[static_cast<std::size_t>(i) + static_cast<std::size_t>(w) * j] = window_median(window);
    }
}

// ---------------------------------------------------------------------------
// Per-method drivers. Each materializes the source, runs opts.passes passes,
// and returns the finalized float buffer (missing footprint already restored).
// ---------------------------------------------------------------------------

std::vector<float> run_box(const Grid& src,
                           long w,
                           long h,
                           const SmoothOptions& opts,
                           long period,
                           std::vector<char>& missing)
{
  const int r = opts.radius;
  const auto n = static_cast<std::size_t>(w) * h;
  std::vector<double> num(n);
  std::vector<double> den(n);
  materialize(src, w, h, num, den, missing);

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
  return finalize(num, den, missing);
}

std::vector<float> run_morphology(const Grid& src,
                                  long w,
                                  long h,
                                  const SmoothOptions& opts,
                                  long period,
                                  std::vector<char>& missing)
{
  const int r = opts.radius;
  const auto n = static_cast<std::size_t>(w) * h;
  std::vector<double> val(n);
  materialize_values(src, w, h, val, missing);

  const long maxdim = std::max(w, h);
  std::vector<double> e(static_cast<std::size_t>(maxdim) + 2L * r);
  std::deque<long> dq;

  const bool kErode = false;
  const bool kDilate = true;
  for (int p = 0; p < opts.passes; ++p)
  {
    // Opening = erode then dilate; closing = dilate then erode. OpenClose runs
    // the opening, then the closing on its result.
    if (opts.morphology != MorphologyOp::Close)  // Open or OpenClose: opening first
    {
      morph_op(val, w, h, r, opts.boundary, period, kErode, e, dq);
      morph_op(val, w, h, r, opts.boundary, period, kDilate, e, dq);
    }
    if (opts.morphology != MorphologyOp::Open)  // Close or OpenClose: closing
    {
      morph_op(val, w, h, r, opts.boundary, period, kDilate, e, dq);
      morph_op(val, w, h, r, opts.boundary, period, kErode, e, dq);
    }
  }
  return finalize_values(val, missing);
}

std::vector<float> run_median(const Grid& src,
                              long w,
                              long h,
                              const SmoothOptions& opts,
                              long period,
                              std::vector<char>& missing)
{
  const int r = opts.radius;
  const auto n = static_cast<std::size_t>(w) * h;
  std::vector<double> a(n);
  std::vector<double> b(n);
  materialize_values(src, w, h, a, missing);

  std::vector<double> window;
  window.reserve(static_cast<std::size_t>(2 * r + 1) * (2 * r + 1));

  std::vector<double>* in = &a;
  std::vector<double>* out = &b;
  for (int p = 0; p < opts.passes; ++p)
  {
    median_pass(*in, *out, w, h, r, opts.boundary, period, window);
    std::swap(in, out);
  }
  return finalize_values(*in, missing);
}

}  // namespace

std::shared_ptr<const Grid> smooth(std::shared_ptr<const Grid> source, const SmoothOptions& opts)
{
  if (!source)
    throw Fmi::Exception(BCP, "Trax::smooth called with a null grid");

  if (!opts.active())
    return source;

  const long w = static_cast<long>(source->width());
  const long h = static_cast<long>(source->height());
  if (w <= 0 || h <= 0)
    return source;

  const auto n = static_cast<std::size_t>(w) * h;

  // Periodic x-wrap for global grids. A grid reporting a period outside (0, w]
  // is treated as non-periodic rather than trusted blindly.
  const auto xp = static_cast<long>(source->xperiod());
  const long period = (xp >= 1 && xp <= w) ? xp : 0;

  std::vector<char> missing(opts.preserve_missing ? n : 0);

  std::vector<float> values;
  switch (opts.method)
  {
    case SmoothMethod::Box:
      values = run_box(*source, w, h, opts, period, missing);
      break;
    case SmoothMethod::Median:
      values = run_median(*source, w, h, opts, period, missing);
      break;
    case SmoothMethod::Morphology:
      values = run_morphology(*source, w, h, opts, period, missing);
      break;
    case SmoothMethod::Pyramid:
      throw Fmi::Exception(BCP, "Trax::smooth: the Pyramid method is not implemented");
    case SmoothMethod::None:
      return source;  // unreachable: !active() handled above
  }

  return std::make_shared<BufferGrid>(std::move(source), std::move(values));
}

}  // namespace Trax
