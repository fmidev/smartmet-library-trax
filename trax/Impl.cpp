#include "Impl.h"
#include "Geos.h"
#include "GridPoint.h"
#include <geos/geom/GeometryFactory.h>
#include <geos/operation/valid/IsValidOp.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/ThreadPool.h>
#include <algorithm>   // std::minmax
#include <cmath>       // std::min and std::max
#include <functional>  // std::function
#include <future>      // std::packaged_task / std::future
#include <limits>      // std::numeric_limits
#include <memory>      // std::make_shared
#include <mutex>       // std::mutex guarding the shared pool
#include <thread>      // std::thread::hardware_concurrency
#include <utility>     // std::pair and std::make_pair

#include <fmt/format.h>

#if 0
#include <fmt/format.h>
#include <iostream>
#endif

namespace Trax
{
namespace
{
// Process-wide worker pool that band tasks are dispatched to. It is null until
// set_worker_threads enables it; while null every band runs on the calling thread, so
// contouring is single-threaded. The pool is sized once at startup by the host application.
// PoolHolder's destructor shuts the pool down (joining workers) before it is destroyed.
struct PoolHolder
{
  std::unique_ptr<Fmi::ThreadPool::ThreadPool<>> pool;
  ~PoolHolder()
  {
    if (pool)
      pool->shutdown();
  }
};

std::mutex g_pool_mutex;
PoolHolder g_pool_holder;

// True once set_worker_threads has enabled the shared pool. When false, band-parallel
// contouring has no workers to dispatch to, so callers fall back to the single-threaded
// engine instead of paying the band-decomposition overhead for no parallel gain.
bool worker_pool_enabled()
{
  std::lock_guard<std::mutex> lock(g_pool_mutex);
  return g_pool_holder.pool != nullptr;
}

// Run a set of band tasks. Task 0 always runs on the calling thread; the rest are dispatched
// to the shared pool when one exists (otherwise they run inline, keeping behaviour identical
// to single-threaded). Blocks until all bands finish and rethrows the first band's exception.
void run_bands(std::vector<std::function<void()>>& tasks)
{
  Fmi::ThreadPool::ThreadPool<>* pool = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_pool_mutex);
    pool = g_pool_holder.pool.get();
  }

  if (pool == nullptr || tasks.size() <= 1)
  {
    for (auto& task : tasks)
      task();
    return;
  }

  std::vector<std::future<void>> futures;
  futures.reserve(tasks.size() - 1);
  for (std::size_t i = 1; i < tasks.size(); i++)
  {
    auto task = std::make_shared<std::packaged_task<void()>>(std::move(tasks[i]));
    futures.push_back(task->get_future());
    if (!pool->schedule([task]() { (*task)(); }))
      (*task)();  // scheduler full: run inline rather than drop the work
  }

  tasks[0]();  // the calling thread contributes one band

  for (auto& future : futures)
    future.get();  // wait for the dispatched bands and rethrow any exception
}

std::string validate_geom(const Trax::GeometryCollection& geom)
{
  const auto factory = geos::geom::GeometryFactory::create();

  auto g = Trax::to_geos_geom(geom, factory);
  geos::operation::valid::IsValidOp validator(g.get());
  if (validator.isValid())
    return {};

  return validator.getValidationError()->toString();
}

void desliver_geoms(std::vector<GeometryCollection>& geoms)
{
  for (auto& geom : geoms)
    geom.desliver();
}

std::list<std::string> validate_isobands(const GeometryCollections& geoms,
                                         const IsobandLimits& limits)
{
  std::list<std::string> details;
  for (auto i = 0UL; i < geoms.size(); i++)
  {
    const auto& tmp = geoms[i];
    std::string err;
    try
    {
      err = validate_geom(tmp);
    }
    catch (const std::exception& e)
    {
      err = e.what();
    }
    if (!err.empty())
    {
      details.emplace_back("Isoband error: " + err);
      details.emplace_back(fmt::format("Limits: {}...{}", limits[i].lo(), limits[i].hi()));
      details.emplace_back("WKT: " + tmp.wkt());
    }
  }
  return details;
}

std::list<std::string> validate_isolines(const GeometryCollections& geoms,
                                         const IsolineValues& limits)
{
  std::list<std::string> details;
  for (auto i = 0UL; i < geoms.size(); i++)
  {
    const auto& tmp = geoms[i];
    auto err = validate_geom(tmp);
    if (!err.empty())
    {
      details.emplace_back("Isoline error: " + err);
      details.emplace_back("Isovalue: " + fmt::format("{}", limits[i]));
      details.emplace_back("WKT: " + tmp.wkt());
    }
  }
  return details;
}

[[noreturn]] void throw_with_details(
    const Grid& grid, long imin, long imax, long jmin, long jmax, long nx, long ny)
{
  std::string details;
  if (nx < 10 && ny < 10)
  {
    details += "\nValues:\n";
    for (int j = jmax + 1; j >= jmin; --j)
    {
      for (int i = imin; i <= imax + 1; ++i)
        details += fmt::format("{}\t", grid(i, j));
      details += "\n";
    }
    details += "Coords:\n";
    for (int j = jmax + 1; j >= jmin; --j)
    {
      for (int i = imin; i <= imax + 1; ++i)
        details += fmt::format("{} {}\t", grid.x(i, j), grid.y(i, j));
      details += "\n";
    }
  }
  Fmi::Exception ex(BCP, "Contouring failed!", nullptr);
  ex.addParameter("nx", Fmi::to_string(nx));
  ex.addParameter("ny", Fmi::to_string(ny));
  if (!details.empty())
    ex.addParameter("details", details);
  throw ex;
}

}  // namespace

// Size (or disable) the process-wide band worker pool. Call once at startup.
void Contour::set_worker_threads(std::size_t n)
{
  std::lock_guard<std::mutex> lock(g_pool_mutex);
  if (g_pool_holder.pool)
  {
    g_pool_holder.pool->shutdown();  // join existing workers before replacing
    g_pool_holder.pool.reset();
  }
  if (n > 0)
  {
    // The "trax" name makes the pool workers show up as "trax-1" ... "trax-N"
    // in top/htop/gdb (Fmi::ThreadPool names its workers "<name>-<n>").
    g_pool_holder.pool = std::make_unique<Fmi::ThreadPool::ThreadPool<>>(
        n, std::numeric_limits<std::size_t>::max(), "trax");
    g_pool_holder.pool->start();
  }
}

// Prepare for isoline calculations
void Contour::Impl::init(const IsolineValues& values, std::size_t width, std::size_t height)
{
  m_isoline_values = values;
  m_isoband_limits = IsobandLimits();
  m_isoline_values.sort();

  if (!m_isoline_values.valid())
    throw Fmi::Exception(BCP, "Isoline values not valid")
        .addParameter("Limits", m_isoline_values.dump());

  // Flag whether we should contour all valid values for later inversion to missing values
  // Sorting forces this case to be the first one in the list of isolines.
  if (!m_isoline_values.empty())
    m_contour_missing = std::isnan(m_isoline_values[0]);

  m_builders.clear();
  m_builders.reserve(m_isoline_values.size());
  for (auto i = 0UL; i < m_isoline_values.size(); i++)
    m_builders.emplace_back(width, height);
}

// Prepare for isoband calculations
void Contour::Impl::init(const IsobandLimits& limits, std::size_t width, std::size_t height)
{
  m_isoband_limits = limits;
  m_isoline_values = IsolineValues();
  m_isoband_limits.sort(m_closed_range);

  if (!m_isoband_limits.valid())
    throw Fmi::Exception(BCP, "Isoband limits not valid")
        .addParameter("Limits", m_isoband_limits.dump());

  // Flag whether we should contour all valid values for later inversion to missing values
  // Range sorting forces this case to be the first one in the list of isobands.
  if (!m_isoband_limits.empty())
    m_contour_missing = m_isoband_limits[0].missing();

  m_builders.clear();
  m_builders.reserve(m_isoband_limits.size());
  for (auto i = 0UL; i < m_isoband_limits.size(); i++)
    m_builders.emplace_back(width, height);
}

// Finish building a row
void Contour::Impl::finish_row()
{
  for (auto& builder : m_builders)
    builder.finish_row();
}

// Finalize results
void Contour::Impl::finish_isolines()
{
  for (auto& builder : m_builders)
    builder.finish_isolines(m_strict);
}

// Finalize results
void Contour::Impl::finish_isobands()
{
  for (auto i = 0UL; i < m_builders.size(); i++)
  {
    try
    {
      m_builders[i].finish_isobands(m_strict);
    }
    catch (...)
    {
      Fmi::Exception ex(BCP, "Failed to finish isoband", nullptr);
      ex.addParameter("lolimit", fmt::format("{}", m_isoband_limits[i].lo()));
      ex.addParameter("hilimit", fmt::format("{}", m_isoband_limits[i].hi()));
      throw ex;
    }
  }
}

// Move the result for the caller
GeometryCollections Contour::Impl::result()
{
  GeometryCollections geoms;
  geoms.resize(m_builders.size());
  for (auto i = 0UL; i < m_builders.size(); i++)
  {
    auto pos = (!m_isoline_values.empty() ? m_isoline_values.original_position(i)
                                          : m_isoband_limits.original_position(i));
    geoms[pos] = m_builders[i].result();
  }
  return geoms;
}

bool Contour::Impl::update_isolines_to_check(const MinMax& minmax)
{
  // Only contouring missing values? Exit now to make later logic easier
  if (m_contour_missing && m_isoline_values.size() == 1)
    return false;

  float minvalue = minmax.first;
  float maxvalue = minmax.second;

  auto n = m_isoline_values.size();
  auto n0 = (m_contour_missing ? 1UL : 0UL);

  m_min_index = std::max(m_min_index, n0);
  m_max_index = std::max(m_max_index, n0);

  while (m_min_index > 0 && minvalue <= m_isoline_values[m_min_index - 1])
    --m_min_index;
  while (m_min_index < n - 1 && minvalue > m_isoline_values[m_min_index])
    ++m_min_index;

  m_max_index = std::max(m_max_index, m_min_index);

  while (m_max_index > n0 && maxvalue < m_isoline_values[m_max_index])
    --m_max_index;
  while (m_max_index < n - 1 && maxvalue >= m_isoline_values[m_max_index + 1])
    ++m_max_index;

  // The range may now be from 0 to 0, we must check the limits to finally accept the interval
  bool ok = (m_min_index <= m_max_index && minvalue <= m_isoline_values[m_min_index] &&
             maxvalue >= m_isoline_values[m_max_index]);

  return ok;
}

bool Contour::Impl::update_isobands_to_check(const MinMax& minmax)
{
  // Only contouring missing values? Exit now to make later logic easier
  if (m_contour_missing && m_isoband_limits.size() == 1)
    return false;

  float minvalue = minmax.first;
  float maxvalue = minmax.second;

#if 0  
  if (std::isnan(minvalue))
    return false;
#endif

  auto n = m_isoband_limits.size();

  // Ignore possible NaN...NaN range in the search, it will be isobanded separately if necessary
  auto n0 = (m_contour_missing ? 1UL : 0UL);
  m_min_index = std::max(m_min_index, n0);
  m_max_index = std::max(m_max_index, n0);

  while (m_min_index > n0 && minvalue < m_isoband_limits[m_min_index - 1].hi())
    --m_min_index;
  while (m_min_index + 1 < n && minvalue >= m_isoband_limits[m_min_index].hi())
    ++m_min_index;

  m_max_index = std::max(m_max_index, m_min_index);

  while (m_max_index > n0 && maxvalue < m_isoband_limits[m_max_index].lo())
    --m_max_index;
  while (m_max_index + 1 < n && maxvalue >= m_isoband_limits[m_max_index + 1].lo())
    ++m_max_index;

  // Final validity check which may fail at index n0 or n-1

  bool ok = (m_min_index < m_isoband_limits.size() &&
             m_isoband_limits[m_min_index].overlaps(minvalue, maxvalue));

  return ok;
}

void Contour::Impl::isoline(const Cell& c)
{
  if (update_isolines_to_check(minmax(c)))
  {
    // Process isolines/isobands min_index...max_index

    for (auto index = m_min_index; index <= m_max_index; ++index)
      isoline(index, c);
  }

  // Contour valid values if not handled above for later inversion to missing values
  if (m_contour_missing)
    isoline(0, c);
}

void Contour::Impl::isoband(const Cell& c)
{
  if (update_isobands_to_check(minmax(c)))
  {
    // Process isobands min_index...max_index
    for (auto index = m_min_index; index <= m_max_index; ++index)
      isoband(index, c);
  }

  // Contour valid values if not handled above for later inversion to missing values
  if (m_contour_missing)
    isoband(0, c);
}

void Contour::Impl::isoband(int index, const Cell& c)
{
  auto& builder = m_builders[index];
  auto& merger = builder.merger();
  auto range = m_isoband_limits[index];

  switch (m_itype)
  {
    case InterpolationType::Linear:
      CellBuilder::isoband_linear(merger, c, range, m_subdivide);
      break;
    case InterpolationType::Midpoint:
      CellBuilder::isoband_midpoint(merger, c, range, m_shell);
      break;
    case InterpolationType::Logarithmic:
      CellBuilder::isoband_logarithmic(merger, c, range, m_subdivide);
      break;
  }
}

void Contour::Impl::isoline(int index, const Cell& c)
{
  auto& builder = m_builders[index];
  auto& merger = builder.merger();
  auto limit = m_isoline_values[index];

  switch (m_itype)
  {
    case InterpolationType::Linear:
      CellBuilder::isoline_linear(merger, c, limit, m_subdivide);
      break;
    case InterpolationType::Logarithmic:
      CellBuilder::isoline_logarithmic(merger, c, limit, m_subdivide);
      break;
    case InterpolationType::Midpoint:
      break;
  }
}

namespace
{
// Extract data from grid valid area
void fill_buffers(const Grid& grid,
                  const std::array<long, 4>& area,
                  std::size_t j,
                  std::vector<GridPoint>& points)
{
  auto imin = area[0];
  for (auto i = 0UL; i < points.size(); i++)
  {
    auto ii = i + imin;
    points[i] = GridPoint(grid.x(ii, j), grid.y(ii, j), grid(ii, j));
  }
}
}  // namespace

// Single-threaded grid loop for isobands; caller handles desliver/validate
// Contour the cell-rows [bbox.jmin..bbox.jmax] into m_builders. This is the shared grid
// loop used both by the single-threaded engine and by each parallel band; it stops short
// of finalizing so that bands can be stitched together before rings are built.
void Contour::Impl::contour_rows(const Grid& grid,
                                 const std::array<long, 4>& bbox,
                                 long nx,
                                 bool isobands)
{
  const auto imin = bbox[0];
  const auto jmin = bbox[1];
  const auto imax = bbox[2];
  const auto jmax = bbox[3];

  long shift = grid.shift();
  const auto imid = shift - imin;
  const auto needs_two_passes = (shift != 0 && shift > imin && shift < imax);

  // Accessing data through grid is sometimes slow, so we buffer the values into vectors
  // and just swap them after each row to avoid unnecessary copying.
  std::vector<GridPoint> row1(nx);
  std::vector<GridPoint> row2(nx);
  fill_buffers(grid, bbox, jmin, row1);

  std::vector<long> loop_limits;
  if (!needs_two_passes)
    loop_limits = {0, nx - 1};
  else
    loop_limits = {imid, nx - 1, 0, imid - 1};

  for (auto j = jmin; j <= jmax; j++)
  {
    fill_buffers(grid, bbox, j + 1, row2);  // update the 2nd row

    // Keep Cell i,j coordinates continuous by applying a shift in the second loop
    auto ishift = 0;
    for (auto k = 0UL; k < loop_limits.size(); k += 2)
    {
      for (auto i = loop_limits[k]; i < loop_limits[k + 1]; i++)
        if (grid.valid(imin + i, j))
        {
          Cell cell(row1[i], row2[i], row2[i + 1], row1[i + 1], imin + i + ishift, j);
          if (isobands)
            isoband(cell);
          else
            isoline(cell);
        }
      ishift = loop_limits[k + 1];
    }

    finish_row();
    std::swap(row1, row2);  // roll down the coordinates to the bottom row
  }
}

GeometryCollections Contour::Impl::isobands_impl(const Grid& grid,
                                                  const std::array<long, 4>& bbox,
                                                  long nx,
                                                  long /* ny */)
{
  contour_rows(grid, bbox, nx, true);
  finish_isobands();
  return result();
}

// Number of contiguous row-bands for parallel contouring. Auto (0) uses hardware
// concurrency; otherwise the requested thread count. Never more bands than cell-rows.
int Contour::Impl::band_count(long n_rows) const
{
  if (n_rows < 2)
    return 1;
  unsigned hw = std::thread::hardware_concurrency();
  unsigned requested = (m_threads <= 0) ? std::max(1U, hw) : static_cast<unsigned>(m_threads);
  long bands = std::min<long>(requested, n_rows);
  return (bands < 1) ? 1 : static_cast<int>(bands);
}

// Parallel isoband contouring by horizontal row-bands.
//
// The grid's cell-rows are split into contiguous bands, each contoured independently into
// its own sub-Impl (one JointMerger/JointPool per level). Adjacent bands are then stitched
// along their single shared boundary row (join_boundary), after which each level's rings are
// assembled across all band pools. This traverses the grid once and scales with grid height
// rather than the number of levels. (Band tasks run serially here; see set_worker_threads for
// pooled parallel dispatch.)
GeometryCollections Contour::Impl::isobands_parallel(const Grid& grid,
                                                      const std::array<long, 4>& bbox,
                                                      long nx,
                                                      long ny)
{
  const auto n = m_isoband_limits.size();
  const auto jmin = bbox[1];
  const auto jmax = bbox[3];
  const long n_rows = jmax - jmin + 1;

  // Fall back to the single-threaded engine when there is nothing to gain: only one band, or no
  // worker pool has been enabled (so bands would just run serially with extra overhead).
  const int b = band_count(n_rows);
  if (b <= 1 || !worker_pool_enabled())
    return isobands_impl(grid, bbox, nx, ny);

  // Full level set in sorted order, so each band's m_builders[L] maps to sorted level L and
  // the parent's original_position(L) maps L back to the caller's order.
  IsobandLimits sub_limits;
  for (auto i = 0UL; i < n; i++)
    sub_limits.add(m_isoband_limits[i].lo(), m_isoband_limits[i].hi());

  // Split cell-rows into b contiguous bands, each with its own sub-Impl.
  std::vector<std::shared_ptr<Impl>> bands;
  std::vector<std::function<void()>> tasks;
  bands.reserve(b);
  tasks.reserve(b);
  const long chunk = (n_rows + b - 1) / b;  // ceiling division
  for (int k = 0; k < b; k++)
  {
    const long band_jmin = jmin + static_cast<long>(k) * chunk;
    if (band_jmin > jmax)
      break;
    const long band_jmax = std::min(band_jmin + chunk - 1, jmax);

    auto sub = std::make_shared<Impl>();
    sub->m_itype = m_itype;
    sub->m_strict = m_strict;
    sub->m_closed_range = m_closed_range;
    sub->m_shell = m_shell;
    sub->m_subdivide = m_subdivide;
    sub->init(sub_limits, nx, ny);

    const std::array<long, 4> band_bbox{bbox[0], band_jmin, bbox[2], band_jmax};
    auto* raw = sub.get();
    tasks.emplace_back([raw, &grid, band_bbox, nx]() { raw->contour_rows(grid, band_bbox, nx, true); });
    bands.push_back(std::move(sub));
  }

  // Contour the bands (band 0 on this thread, the rest on the shared pool if any).
  run_bands(tasks);

  // Stitch adjacent bands along their shared boundary row, per level.
  for (auto L = 0UL; L < n; L++)
    for (std::size_t k = 0; k + 1 < bands.size(); k++)
      join_boundary(bands[k]->m_builders[L].merger(), bands[k + 1]->m_builders[L].merger());

  // Assemble each level's rings across all band pools (rings may span several pools). Levels are
  // independent -- level L touches only the per-band pools for L -- so the assembly runs on the
  // same worker pool, one task per level (the calling thread takes level 0).
  GeometryCollections final_result(n);
  std::vector<std::function<void()>> finalize_tasks;
  finalize_tasks.reserve(n);
  for (auto L = 0UL; L < n; L++)
  {
    finalize_tasks.emplace_back(
        [this, L, &bands, &final_result]()
        {
          std::vector<JointPool*> pools;
          pools.reserve(bands.size());
          for (auto& band : bands)
            pools.push_back(&band->m_builders[L].merger().pool());

          try
          {
            bands[0]->m_builders[L].finish_isobands(pools, m_strict);
          }
          catch (...)
          {
            Fmi::Exception ex(BCP, "Failed to finish isoband", nullptr);
            ex.addParameter("lolimit", fmt::format("{}", m_isoband_limits[L].lo()));
            ex.addParameter("hilimit", fmt::format("{}", m_isoband_limits[L].hi()));
            throw ex;
          }
          final_result[m_isoband_limits.original_position(L)] = bands[0]->m_builders[L].result();
        });
  }
  run_bands(finalize_tasks);
  return final_result;
}

// Contour full grid
GeometryCollections Contour::Impl::isobands(const Grid& grid, const IsobandLimits& limits)
{
  if (limits.empty())
    return {};

  shell(grid.shell());

  const auto bbox = grid.bbox();
  const auto imin = bbox[0];
  const auto jmin = bbox[1];
  const auto imax = bbox[2];
  const auto jmax = bbox[3];

  if (imax < imin || jmax < jmin)
  {
    init(limits, 0, 0);
    return result();
  }

  const auto nx = imax - imin + 2;
  const auto ny = jmax - jmin + 2;

  try
  {
    init(limits, nx, ny);

    auto res = (m_threads == 1) ? isobands_impl(grid, bbox, nx, ny)
                                 : isobands_parallel(grid, bbox, nx, ny);

    if (m_desliver)
      desliver_geoms(res);

    if (!m_validate)
      return res;

    auto details = validate_isobands(res, limits);

    if (!details.empty())
    {
      if (m_strict)
        throw Fmi::Exception(BCP, "Isoband validation failed").addDetails(details);

      for (const auto& row : details)
        std::cerr << row << '\n';
      std::cerr << std::flush;
    }
    return res;
  }
  catch (...)
  {
    // Try to provide some info into the logs
    throw_with_details(grid, imin, imax, jmin, jmax, nx, ny);
  }
}

// Single-threaded grid loop for isolines; caller handles desliver/validate
GeometryCollections Contour::Impl::isolines_impl(const Grid& grid,
                                                  const std::array<long, 4>& bbox,
                                                  long nx,
                                                  long /* ny */)
{
  contour_rows(grid, bbox, nx, false);
  finish_isolines();
  return result();
}

// Parallel isoline contouring by horizontal row-bands (see isobands_parallel).
GeometryCollections Contour::Impl::isolines_parallel(const Grid& grid,
                                                      const std::array<long, 4>& bbox,
                                                      long nx,
                                                      long ny)
{
  const auto n = m_isoline_values.size();
  const auto jmin = bbox[1];
  const auto jmax = bbox[3];
  const long n_rows = jmax - jmin + 1;

  // Fall back to the single-threaded engine when there is nothing to gain: only one band, or no
  // worker pool has been enabled (so bands would just run serially with extra overhead).
  const int b = band_count(n_rows);
  if (b <= 1 || !worker_pool_enabled())
    return isolines_impl(grid, bbox, nx, ny);

  // Full level set in sorted order so each band's m_builders[L] maps to sorted level L.
  IsolineValues sub_values;
  for (auto i = 0UL; i < n; i++)
    sub_values.add(m_isoline_values[i]);

  // Split cell-rows into b contiguous bands, each with its own sub-Impl.
  std::vector<std::shared_ptr<Impl>> bands;
  std::vector<std::function<void()>> tasks;
  bands.reserve(b);
  tasks.reserve(b);
  const long chunk = (n_rows + b - 1) / b;  // ceiling division
  for (int k = 0; k < b; k++)
  {
    const long band_jmin = jmin + static_cast<long>(k) * chunk;
    if (band_jmin > jmax)
      break;
    const long band_jmax = std::min(band_jmin + chunk - 1, jmax);

    auto sub = std::make_shared<Impl>();
    sub->m_itype = m_itype;
    sub->m_strict = m_strict;
    sub->m_shell = m_shell;
    sub->m_subdivide = m_subdivide;
    sub->init(sub_values, nx, ny);

    const std::array<long, 4> band_bbox{bbox[0], band_jmin, bbox[2], band_jmax};
    auto* raw = sub.get();
    tasks.emplace_back([raw, &grid, band_bbox, nx]() { raw->contour_rows(grid, band_bbox, nx, false); });
    bands.push_back(std::move(sub));
  }

  // Contour the bands (band 0 on this thread, the rest on the shared pool if any).
  run_bands(tasks);

  // Stitch adjacent bands along their shared boundary row, per level.
  for (auto L = 0UL; L < n; L++)
    for (std::size_t k = 0; k + 1 < bands.size(); k++)
      join_boundary(bands[k]->m_builders[L].merger(), bands[k + 1]->m_builders[L].merger());

  // Assemble each level's lines across all band pools (chains may span several pools). Levels are
  // independent, so assembly runs on the same worker pool, one task per level.
  GeometryCollections final_result(n);
  std::vector<std::function<void()>> finalize_tasks;
  finalize_tasks.reserve(n);
  for (auto L = 0UL; L < n; L++)
  {
    finalize_tasks.emplace_back(
        [this, L, &bands, &final_result]()
        {
          std::vector<JointPool*> pools;
          pools.reserve(bands.size());
          for (auto& band : bands)
            pools.push_back(&band->m_builders[L].merger().pool());

          bands[0]->m_builders[L].finish_isolines(pools, m_strict);
          final_result[m_isoline_values.original_position(L)] = bands[0]->m_builders[L].result();
        });
  }
  run_bands(finalize_tasks);
  return final_result;
}

// Contour full grid for isolines
GeometryCollections Contour::Impl::isolines(const Grid& grid, const IsolineValues& limits)
{
  if (limits.empty())
    return {};

  shell(grid.shell());

  auto bbox = grid.bbox();
  auto imin = bbox[0];
  auto jmin = bbox[1];
  auto imax = bbox[2];
  auto jmax = bbox[3];

  if (imax < imin || jmax < jmin)
  {
    init(limits, 0, 0);
    return result();
  }

  auto nx = imax - imin + 2;
  auto ny = jmax - jmin + 2;

  try
  {
    init(limits, nx, ny);

    auto res = (m_threads == 1) ? isolines_impl(grid, bbox, nx, ny)
                                 : isolines_parallel(grid, bbox, nx, ny);

    if (m_desliver)
      desliver_geoms(res);

    if (!m_validate)
      return res;

    auto details = validate_isolines(res, limits);

    if (!details.empty())
    {
      if (m_strict)
        throw Fmi::Exception(BCP, "Isoband validation failed").addDetails(details);

      for (const auto& row : details)
        std::cerr << row << '\n';
      std::cerr << std::flush;
    }
    return res;
  }
  catch (...)
  {
    // Try to provide some info into the logs
    throw_with_details(grid, imin, imax, jmin, jmax, nx, ny);
  }
}

}  // namespace Trax
