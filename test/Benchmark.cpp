// Large-grid benchmark for comparing contouring performance across branches.
//
// Models a convective precipitation scene: a 2000x2000 km grid (1 km cells)
// of zeros, splashed with several hundred small Gaussian "cells" of radius
// 5-20 km and varying intensity. Each Gaussian produces concentric isoband
// rings, so lower-intensity bands enclose dozens to hundreds of holes (the
// inner higher-intensity bands of every nearby cell). This is the topology
// pattern where O(holes x shells) hole-to-shell assignment dominates.
//
// Usage: ./Benchmark [nx] [ny] [reps] [threads] [num_cells]
//
// Defaults: 2000 2000 5 1 300
//
// Compile with:
//   make Benchmark
// Run on a branch, then `git checkout no-rtree && make && make -C test Benchmark`
// and compare the printed timings. The WKT-length fingerprint should match
// across branches; if it does not, the two branches produce different topology.

#include "Contour.h"
#include "IsobandLimits.h"
#include "TestGrid.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

double seconds_since(Clock::time_point t0)
{
  return std::chrono::duration<double>(Clock::now() - t0).count();
}

struct Gaussian
{
  double cx;     // center x in grid units (cells)
  double cy;     // center y in grid units (cells)
  double sigma;  // stddev in grid units (cells)
  double peak;   // peak intensity (mm/h-ish)
};

// Build a deterministic set of Gaussian "convective cells". Domain is
// nx x ny grid cells; cell spacing is assumed 1 unit (interpret as km).
// sigma ranges 5..20 km, peak 2..40 mm/h.
std::vector<Gaussian> make_gaussians(long nx, long ny, int count, unsigned seed = 42)
{
  std::mt19937 rng(seed);
  std::uniform_real_distribution<double> ux(0.0, static_cast<double>(nx - 1));
  std::uniform_real_distribution<double> uy(0.0, static_cast<double>(ny - 1));
  std::uniform_real_distribution<double> usig(5.0, 20.0);
  std::uniform_real_distribution<double> upeak(2.0, 40.0);

  std::vector<Gaussian> out;
  out.reserve(count);
  for (int k = 0; k < count; ++k)
    out.push_back({ux(rng), uy(rng), usig(rng), upeak(rng)});
  return out;
}

// Splash Gaussians onto a zeroed grid. Only touches cells within 4*sigma of
// each center, so cost is O(num_gaussians * sigma^2) — trivial vs 4M cells.
void fill_grid(Trax::TestGrid& grid,
               long nx,
               long ny,
               const std::vector<Gaussian>& gs)
{
  std::vector<float> raw(static_cast<size_t>(nx) * ny, 0.0f);

  for (const auto& g : gs)
  {
    const double r = 4.0 * g.sigma;
    const long i0 = std::max<long>(0, static_cast<long>(std::floor(g.cx - r)));
    const long i1 = std::min<long>(nx - 1, static_cast<long>(std::ceil(g.cx + r)));
    const long j0 = std::max<long>(0, static_cast<long>(std::floor(g.cy - r)));
    const long j1 = std::min<long>(ny - 1, static_cast<long>(std::ceil(g.cy + r)));
    const double inv2s2 = 1.0 / (2.0 * g.sigma * g.sigma);

    for (long j = j0; j <= j1; ++j)
    {
      const double dy = j - g.cy;
      const double dy2 = dy * dy;
      for (long i = i0; i <= i1; ++i)
      {
        const double dx = i - g.cx;
        const double d2 = dx * dx + dy2;
        raw[i + nx * j] += static_cast<float>(g.peak * std::exp(-d2 * inv2s2));
      }
    }
  }

  // Quantize to 0.1 mm/h precision (matches real precipitation product
  // precision and triggers many equal-corner cell configurations).
  for (long j = 0; j < ny; ++j)
    for (long i = 0; i < nx; ++i)
    {
      const float v = std::round(raw[i + nx * j] * 10.0f) / 10.0f;
      grid.set(i, j, v);
    }
}

Trax::IsobandLimits make_limits()
{
  // Precipitation-style breakpoints. Each band covers a [lo, hi] interval.
  // Includes a wide outer band so the "everything above trace" shell exists.
  const float edges[] = {0.1f, 0.2f, 0.5f, 1.0f, 2.0f,  5.0f,  10.0f,
                         15.0f, 20.0f, 30.0f, 50.0f, 100.0f};
  Trax::IsobandLimits limits;
  for (size_t k = 0; k + 1 < sizeof(edges) / sizeof(edges[0]); ++k)
    limits.add(edges[k], edges[k + 1]);
  return limits;
}

std::size_t polygon_fingerprint(const std::vector<Trax::GeometryCollection>& results)
{
  std::size_t sum = 0;
  for (const auto& gc : results)
    sum += gc.wkt().size();
  return sum;
}
}  // namespace

int main(int argc, char* argv[])
{
  long nx = 2000;
  long ny = 2000;
  int reps = 5;
  int threads = 1;
  int num_cells = 300;

  if (argc > 1) nx = std::atol(argv[1]);
  if (argc > 2) ny = std::atol(argv[2]);
  if (argc > 3) reps = std::atoi(argv[3]);
  if (argc > 4) threads = std::atoi(argv[4]);
  if (argc > 5) num_cells = std::atoi(argv[5]);

  std::cout << "Trax convective-precip benchmark\n"
            << "  grid:    " << nx << " x " << ny << " (" << (nx * ny) << " cells)\n"
            << "  reps:    " << reps << "\n"
            << "  threads: " << threads << "\n"
            << "  storms:  " << num_cells << " Gaussians, sigma 5-20, peak 2-40\n";

  const auto t_build = Clock::now();
  Trax::TestGrid grid(nx, ny, 0.0, 0.0, static_cast<double>(nx), static_cast<double>(ny));
  const auto gaussians = make_gaussians(nx, ny, num_cells);
  fill_grid(grid, nx, ny, gaussians);
  std::cout << "  build:   " << seconds_since(t_build) << " s\n";

  const auto limits = make_limits();
  std::cout << "  levels:  11 precip isobands in [0.1, 100] mm/h\n\n";

  Trax::Contour contourer;
  contourer.threads(threads);

  std::vector<double> times;
  times.reserve(reps);
  std::size_t fingerprint = 0;

  for (int r = 0; r < reps; ++r)
  {
    const auto t0 = Clock::now();
    auto results = contourer.isobands(grid, limits);
    const double elapsed = seconds_since(t0);

    const std::size_t fp = polygon_fingerprint(results);
    if (r == 0)
      fingerprint = fp;
    else if (fp != fingerprint)
      std::cout << "  WARN: fingerprint changed across reps (" << fingerprint << " -> " << fp
                << ")\n";

    times.push_back(elapsed);
    std::cout << "  run " << (r + 1) << ":   " << elapsed << " s\n";
  }

  std::sort(times.begin(), times.end());
  const double tmin = times.front();
  const double tmed = times[times.size() / 2];

  std::cout << "\n  min:     " << tmin << " s\n"
            << "  median:  " << tmed << " s\n"
            << "  fp:      " << fingerprint << " (sum of WKT lengths)\n";

  return 0;
}
