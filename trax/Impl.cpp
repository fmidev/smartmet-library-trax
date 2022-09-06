#include "Impl.h"
#include "Geos.h"
#include "GridPoint.h"
#include <geos/geom/GeometryFactory.h>
#include <geos/operation/valid/IsValidOp.h>
#include <macgyver/Exception.h>
#include <algorithm>  // std::minmax
#include <cmath>      // std::min and std::max
#include <utility>    // std::pair and std::make_pair

#if 0
#include <fmt/format.h>
#include <iostream>
#endif

namespace
{
std::string validate_geom(const Trax::GeometryCollection& geom)
{
  const auto factory = geos::geom::GeometryFactory::create();

  auto g = Trax::to_geos_geom(geom, factory);
  geos::operation::valid::IsValidOp validator(g.get());
  if (validator.isValid())
    return {};

  return validator.getValidationError()->toString();
}

}  // namespace

namespace Trax
{
// Prepare for isoline calculations
void Contour::Impl::init(const IsolineValues& values, std::size_t width, std::size_t height)
{
  m_isoline_values = values;
  m_isoband_limits = IsobandLimits();
  m_isoline_values.sort();

  if (!m_isoline_values.valid())
    throw Fmi::Exception(BCP, "Isoline values not valid")
        .addParameter("Limits", m_isoline_values.dump());

  m_builders.clear();
  m_builders.reserve(m_isoline_values.size());
  for (auto i = 0UL; i < m_isoline_values.size(); i++)
    m_builders.emplace_back(width, height);

  // m_builders.resize(m_isoline_values.size(), Builder(width, height));
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
  // m_builders.resize(m_isoband_limits.size(), Builder(width, height));
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
    m_builders[i].finish_isobands(m_strict, m_isoband_limits[i].missing(), m_mincoord, m_maxcoord);
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
  double minvalue = minmax.first;
  double maxvalue = minmax.second;

  auto n = m_isoline_values.size();

  m_min_index = std::max(m_min_index, 0UL);
  m_max_index = std::max(m_max_index, 0UL);

  while (m_min_index > 0 && minvalue <= m_isoline_values[m_min_index - 1])
    --m_min_index;
  while (m_min_index < n - 1 && minvalue > m_isoline_values[m_min_index])
    ++m_min_index;

  m_max_index = std::max(m_max_index, m_min_index);

  while (m_max_index > 0 && maxvalue < m_isoline_values[m_max_index])
    --m_max_index;
  while (m_max_index < n - 1 && maxvalue >= m_isoline_values[m_max_index + 1])
    ++m_max_index;

  // The range may now be from 0 to 0, we must check the limits to finally accept the interval
  bool ok = (m_min_index <= m_max_index && minvalue <= m_isoline_values[m_min_index] &&
             maxvalue >= m_isoline_values[m_max_index]);

#if 0
  std::cout << "Range: " << minvalue << "..." << maxvalue << "\tlimits = " << m_min_index << ","
            << m_max_index << "\t= " << m_isoline_values[m_min_index] << "..."
            << m_isoline_values[m_max_index] << "\t";
  std::cout << (ok ? "ok" : "***") << "\n";
#endif
  return ok;
}

bool Contour::Impl::update_isobands_to_check(const MinMax& minmax)
{
  // Only contouring missing values? Exit now to make later logic easier
  if (m_contour_missing && m_isoband_limits.size() == 1)
    return false;

  double minvalue = minmax.first;
  double maxvalue = minmax.second;

  if (std::isnan(minvalue))
    return false;

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

#if 0
  std::cout << "Range: " << minvalue << "..." << maxvalue << "\tlimits = " << m_min_index << ","
            << m_max_index << "\t= " << m_isoband_limits[m_min_index].lo() << "..."
            << m_isoband_limits[m_min_index].hi() << " to " << m_isoband_limits[m_max_index].lo()
            << "..." << m_isoband_limits[m_max_index].hi() << "\t";
  std::cout << (ok ? "ok" : "***") << "\n";
#endif
  return ok;
}

void Contour::Impl::isoline(const Cell& c)
{
  if (std::isnan(c.p1.z) || std::isnan(c.p2.z) || std::isnan(c.p3.z) || std::isnan(c.p4.z))
    return;

  if (update_isolines_to_check(minmax(c)))
  {
    // Process isolines/isobands min_index...max_index

    for (auto index = m_min_index; index <= m_max_index; ++index)
    {
      double limit = m_isoline_values[index];
      auto& builder = m_builders[index];
      auto& merger = builder.merger();
      CellBuilder::isoline_linear(merger, c, limit);
    }
  }
}

void Contour::Impl::isoband(const Cell& c)
{
  // if (std::isnan(c.z1) || std::isnan(c.z2) || std::isnan(c.z3) || std::isnan(c.z4))
  // return;

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

  // Contour NaN...NaN as -inf...inf for later inversion from valid values to missing values
  if (range.missing())
  {
    auto inf = std::numeric_limits<double>::infinity();
    range = Range(-inf, inf);
  }

  switch (m_itype)
  {
    case InterpolationType::Linear:
      CellBuilder::isoband_linear(merger, c, range);
      break;
    case InterpolationType::Midpoint:
      CellBuilder::isoband_midpoint(merger, c, range);
      break;
  }
}

// Extract data from grid valid area
void fill_buffers(const Grid& grid,
                  const std::array<std::size_t, 4>& area,
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

// Contour full grid
GeometryCollections Contour::Impl::isobands(const Grid& grid, const IsobandLimits& limits)
{
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
  init(limits, nx, ny);

  // Determine whether we need to split rows into two parts due to shifted global data. Otherwise
  // we'd need special code to handle joining the first and last cells.

  const auto shift = grid.shift();
  const auto imid = shift - imin;
  const auto needs_two_passes = (shift != 0 && shift > imin && shift < imax);

  // Accessing data through grid is sometimes slow, so we buffer the values into vectors
  // and just swap them after each row to avoid unnecessary copying.
  std::vector<GridPoint> row1(nx);
  std::vector<GridPoint> row2(nx);

  fill_buffers(grid, bbox, jmin, row1);

  for (std::size_t j = jmin; j <= jmax; j++)
  {
    fill_buffers(grid, bbox, j + 1, row2);  // update the 2nd row

    if (!needs_two_passes)
    {
      for (std::size_t i = 0; i < nx - 1; i++)
      {
        if (grid.valid(imin + i, j))
          isoband(Cell(row1[i], row2[i], row2[i + 1], row1[i + 1], imin + i, j));
      }
    }
    else
    {
      for (std::size_t i = imid; i < nx - 1; i++)
      {
        if (grid.valid(imin + i, j))
          isoband(Cell(row1[i], row2[i], row2[i + 1], row1[i + 1], imin + i, j));
      }
      for (std::size_t i = 0; i < imid - 1; i++)
      {
        if (grid.valid(imin + i, j))
          isoband(Cell(row1[i], row2[i], row2[i + 1], row1[i + 1], imin + i, j));
      }
    }

    finish_row();

    std::swap(row1, row2);  // roll down the coordinates to the bottom row
  }

  finish_isobands();

  if (!m_validate)
    return result();

  auto res = result();
  for (auto i = 0UL; i < res.size(); i++)
  {
    const auto& tmp = res[i];
    auto err = validate_geom(tmp);
    if (!err.empty())
      std::cerr << "Isoband error: " << err << "\n"
                << "Limits: " << limits[i].lo() << "..." << limits[i].hi() << "\n"
                << "WKT:\n"
                << tmp.wkt() << "\n";
  }
  return res;

}  // namespace Trax

// Contour full grid for isolines
GeometryCollections Contour::Impl::isolines(const Grid& grid, const IsolineValues& limits)
{
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
  init(limits, nx, ny);

  // Determine whether we need to split rows into two parts due to shifted global data. Otherwise
  // we'd need special code to handle joining the first and last cells.

  const auto shift = grid.shift();
  const auto imid = shift - imin;
  const auto needs_two_passes = (shift != 0 && shift > imin && shift < imax);

  // Accessing data through grid is sometimes slow, so we buffer the values into vectors
  // and just swap them after each row to avoid unnecessary copying.
  std::vector<GridPoint> row1(nx);
  std::vector<GridPoint> row2(nx);
  fill_buffers(grid, bbox, jmin, row1);

  for (std::size_t j = jmin; j <= jmax; j++)
  {
    fill_buffers(grid, bbox, j + 1, row2);  // update the 2nd row

    if (!needs_two_passes)
    {
      for (std::size_t i = 0; i < nx - 1; i++)
      {
        if (grid.valid(imin + i, j))
          isoline(Cell(row1[i], row2[i], row2[i + 1], row1[i + 1], imin + i, j));
      }
    }
    else
    {
      for (std::size_t i = imid; i < nx - 1; i++)
      {
        if (grid.valid(imin + i, j))
          isoline(Cell(row1[i], row2[i], row2[i + 1], row1[i + 1], imin + i, j));
      }
      for (std::size_t i = 0; i < imid - 1; i++)
      {
        if (grid.valid(imin + i, j))
          isoline(Cell(row1[i], row2[i], row2[i + 1], row1[i + 1], imin + i, j));
      }
    }

    finish_row();

    std::swap(row1, row2);  // roll down the coordinates to the bottom row
  }

  finish_isolines();

  if (!m_validate)
    return result();

  auto res = result();
  for (auto i = 0UL; i < res.size(); i++)
  {
    const auto& tmp = res[i];
    auto err = validate_geom(tmp);
    if (!err.empty())
      std::cerr << "Isoline error: " << err << "\n"
                << "Isovalue: " << limits[i] << "\n"
                << "WKT:\n"
                << tmp.wkt() << "\n";
  }
  return res;
}

}  // namespace Trax
