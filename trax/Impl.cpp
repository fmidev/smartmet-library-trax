#include "Impl.h"
#include "Geos.h"
#include "GridPoint.h"
#include <geos/geom/GeometryFactory.h>
#include <geos/operation/valid/IsValidOp.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <algorithm>  // std::minmax
#include <cmath>      // std::min and std::max
#include <utility>    // std::pair and std::make_pair

#include <fmt/format.h>

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
      CellBuilder::isoband_linear(merger, c, range);
      break;
    case InterpolationType::Midpoint:
      CellBuilder::isoband_midpoint(merger, c, range, m_shell);
      break;
    case InterpolationType::Logarithmic:
      CellBuilder::isoband_logarithmic(merger, c, range);
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
      CellBuilder::isoline_linear(merger, c, limit);
      break;
    case InterpolationType::Logarithmic:
      CellBuilder::isoline_logarithmic(merger, c, limit);
      break;
    case InterpolationType::Midpoint:
      break;
  }
}

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

    // Determine whether we need to split rows into two parts due to shifted global data. Otherwise
    // we'd need special code to handle joining the first and last cells.

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
            isoband(Cell(row1[i], row2[i], row2[i + 1], row1[i + 1], imin + i + ishift, j));
        ishift = loop_limits[k + 1];
      }

      finish_row();
      std::swap(row1, row2);  // roll down the coordinates to the bottom row
    }

    finish_isobands();

    if (!m_validate)
      return result();

    auto res = result();

    std::list<std::string> details;
    for (auto i = 0UL; i < res.size(); i++)
    {
      const auto& tmp = res[i];
      auto err = validate_geom(tmp);
      if (!err.empty())
      {
        details.emplace_back("Isoband error: " + err);
        details.emplace_back(fmt::format("Limits: {}...{}", limits[i].lo(), limits[i].hi()));
        details.emplace_back("WKT: " + tmp.wkt());
      }
    }
    if (!details.empty())
    {
      if (m_strict)
        throw Fmi::Exception(BCP, "Isoband validation failed").addDetails(details);
      else
      {
        for (const auto& row : details)
          std::cerr << row << '\n';
        std::cerr << std::flush;
      }
    }
    return res;
  }
  catch (...)
  {
    // Try to provide some info into the logs
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
          details += fmt::format("{},{}\t", grid.x(i, j), grid.y(i, j));
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

    // Determine whether we need to split rows into two parts due to shifted global data. Otherwise
    // we'd need special code to handle joining the first and last cells.

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
            isoline(Cell(row1[i], row2[i], row2[i + 1], row1[i + 1], imin + i + ishift, j));
        ishift = loop_limits[k + 1];
      }
      finish_row();
      std::swap(row1, row2);  // roll down the coordinates to the bottom row
    }

    finish_isolines();

    if (!m_validate)
      return result();

    auto res = result();

    std::list<std::string> details;
    for (auto i = 0UL; i < res.size(); i++)
    {
      const auto& tmp = res[i];
      auto err = validate_geom(tmp);
      if (!err.empty())
      {
        details.emplace_back("Isoline error: " + err);
        details.emplace_back("Isovalue: " + fmt::format("{}", limits[i]));
        details.emplace_back("WKT: " + tmp.wkt());
      }
    }
    if (!details.empty())
    {
      if (m_strict)
        throw Fmi::Exception(BCP, "Isoband validation failed").addDetails(details);
      else
      {
        for (const auto& row : details)
          std::cerr << row << '\n';
        std::cerr << std::flush;
      }
    }
    return res;
  }
  catch (...)
  {
    // Try to provide some info into the logs
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
          details += fmt::format("{},{}\t", grid.x(i, j), grid.y(i, j));
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
}

}  // namespace Trax
