
#include "Impl.h"
#include <macgyver/Exception.h>
#include <algorithm>  // std::minmax
#include <cmath>      // std::min and std::max
#include <utility>    // std::pair and std::make_pair

#define TRAX_VALIDATE 0

#if TRAX_VALIDATE
#include "Geos.h"
#include <geos/geom/GeometryFactory.h>
#include <geos/operation/valid/IsValidOp.h>
#endif

#if 1
#include <fmt/format.h>
#include <iostream>
#endif

namespace
{
#if TRAX_VALIDATE
std::string validate(const Trax::GeometryCollection& geom)
{
  const auto factory = geos::geom::GeometryFactory::create();

  auto g = Trax::to_geos_geom(geom, factory);
  geos::operation::valid::IsValidOp validator(g.get());
  if (validator.isValid())
    return {};

  return validator.getValidationError()->toString();
}
#endif

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
    throw Fmi::Exception(BCP, "Isoline values not valid");

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
    throw Fmi::Exception(BCP, "Isoband limits not valid");

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
  for (auto& builder : m_builders)
    builder.finish_isobands(m_strict);
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

  int n = m_isoline_values.size();

  m_min_index = std::max(m_min_index, 0);
  m_max_index = std::max(m_max_index, 0);

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
  double minvalue = minmax.first;
  double maxvalue = minmax.second;

  int n = m_isoband_limits.size();

  while (m_min_index > 0 && minvalue < m_isoband_limits[m_min_index - 1].hi())
    --m_min_index;
  while (m_min_index < n - 1 && minvalue >= m_isoband_limits[m_min_index].hi())
    ++m_min_index;

  m_max_index = std::max(m_max_index, m_min_index);

  while (m_max_index > 0 && maxvalue < m_isoband_limits[m_max_index].lo())
    --m_max_index;
  while (m_max_index < n - 1 && maxvalue >= m_isoband_limits[m_max_index + 1].lo())
    ++m_max_index;

  // Final validity check which may fail at index 0 or n-1
  bool ok = (m_isoband_limits[m_min_index].overlaps(minvalue, maxvalue));

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
  if (std::isnan(c.z1) || std::isnan(c.z2) || std::isnan(c.z3) || std::isnan(c.z4))
    return;

  if (!update_isolines_to_check(minmax(c)))
    return;

  // Process isolines/isobands min_index...max_index

  for (int index = m_min_index; index <= m_max_index; ++index)
  {
    double limit = m_isoline_values[index];
    auto& builder = m_builders[index];
    auto& merger = builder.merger();
    CellBuilder::isoline_linear(merger, c, limit);
  }
}

void Contour::Impl::isoband(const Cell& c)
{
  if (std::isnan(c.z1) || std::isnan(c.z2) || std::isnan(c.z3) || std::isnan(c.z4))
    return;

  if (!update_isobands_to_check(minmax(c)))
    return;

  // Process isolines/isobands min_index...max_index

  for (int index = m_min_index; index <= m_max_index; ++index)
    isoband(index, c);
}

void Contour::Impl::isoband(int index, const Cell& c)
{
  switch (m_itype)
  {
    case InterpolationType::Linear:
      isoband_linear(index, c);
      break;
    case InterpolationType::Midpoint:
      isoband_midpoint(index, c);
      break;
  }
}

// Calculate rectangle isobands using linear interpolation
void Contour::Impl::isoband_linear(int index, const Cell& c)
{
  const auto& range = m_isoband_limits[index];
  auto& builder = m_builders[index];
  auto& merger = builder.merger();

  CellBuilder::isoband_linear(merger, c, range);
}

// Calculate rectangle isobands using midpoint interpolation
void Contour::Impl::isoband_midpoint(int index, const Cell& c)
{
  const auto& range = m_isoband_limits[index];
  auto& builder = m_builders[index];
  auto& merger = builder.merger();

  CellBuilder::isoband_midpoint(merger, c, range);
}

void fill_buffers(const Grid& grid,
                  std::size_t j,
                  std::vector<double>& x,
                  std::vector<double>& y,
                  std::vector<double>& z)
{
  const auto nx = grid.width();
  for (auto i = 0UL; i < nx; i++)
  {
    x[i] = grid.x(i, j);
    y[i] = grid.y(i, j);
    z[i] = grid(i, j);
  }
}

// Contour full grid
GeometryCollections Contour::Impl::isobands(const Grid& grid, const IsobandLimits& limits)
{
  const auto nx = grid.width();
  const auto ny = grid.height();
  init(limits, nx, ny);

#if 0
  for (std::size_t j = 0; j < ny; j++)
  {
    for (std::size_t i = 0; i < nx; i++)
    {
      if (i > 0)
        std::cout << "\t";
      std::cout << grid(i, ny - j - 1);
    }
    std::cout << "\n";
  }

  for (std::size_t j = 0; j < ny; j++)
  {
    for (std::size_t i = 0; i < nx; i++)
    {
      if (i > 0)
        std::cout << " ";
      if (!grid.valid(i, 0))
        std::cout << "?";
      std::cout << fmt::format(
          "{},{}:{},{}\t", i, ny - j - 1, grid.x(i, ny - j - 1), grid.y(i, ny - j - 1));
    }
    std::cout << "\n";
  }
#endif

  // Accessing data through grid is sometimes slow, so we buffer the values into vectors
  // and just swap them after each row to avoid unnecessary copying.
  std::vector<double> x1(nx);
  std::vector<double> x2(nx);
  std::vector<double> y1(nx);
  std::vector<double> y2(nx);
  std::vector<double> z1(nx);
  std::vector<double> z2(nx);

  fill_buffers(grid, 0, x1, y1, z1);

  for (std::size_t j = 0; j < ny - 1; j++)
  {
    fill_buffers(grid, j + 1, x2, y2, z2);  // update the 2nd row

    for (std::size_t i = 0; i < nx - 1; i++)
    {
      // clang-format off
      if (grid.valid(i, j))
        isoband(Cell(x1[i], y1[i], z1[i], x2[i], y2[i], z2[i], x2[i+1], y2[i+1], z2[i+1], x1[i+1], y1[i+1], z1[i+1], i, j));
      // isoband(Cell(i, j, z1[i], i, j+1, z2[i], i+1, j+1, z2[i+1], i+1, j, z1[i+1], i, j));
      // clang-format on
    }
    finish_row();

    std::swap(x2, x1);  // roll down the coordinates to the bottom row
    std::swap(y2, y1);
    std::swap(z2, z1);
  }

  finish_isobands();

#if !TRAX_VALIDATE
  return result();
#else
  auto res = result();
  for (auto i = 0UL; i < res.size(); i++)
  {
    const auto& tmp = res[i];
    auto err = validate(tmp);
    if (!err.empty())
      std::cerr << "Isoband error: " << err << "\n"
                << "Limits: " << limits[i].lo() << "..." << limits[i].hi() << "\n"
                << "WKT:\n"
                << tmp.wkt() << "\n";
  }
  return res;
#endif
}  // namespace Trax

// Contour full grid for isolines
GeometryCollections Contour::Impl::isolines(const Grid& grid, const IsolineValues& limits)
{
  const auto nx = grid.width();
  const auto ny = grid.height();
  init(limits, nx, ny);

  // Accessing data trhough grid is sometimes slow, so we buffer the values into vectors
  // and just swap them after each row to avoid unnecessary copying.
  std::vector<double> x1(nx), x2(nx), y1(nx), y2(nx), z1(nx), z2(nx);
  fill_buffers(grid, 0, x1, y1, z1);

  for (std::size_t j = 0; j < ny - 1; j++)
  {
    fill_buffers(grid, j + 1, x2, y2, z2);  // update the 2nd row

    for (std::size_t i = 0; i < nx - 1; i++)
    {
      // clang-format off
      if (grid.valid(i, j))
        isoline(Cell(x1[i], y1[i], z1[i], x2[i], y2[i], z2[i], x2[i+1], y2[i+1], z2[i+1], x1[i+1], y1[i+1], z1[i+1], i, j));
      // clang-format on
    }
    finish_row();

    std::swap(x2, x1);  // roll down the coordinates to the bottom row
    std::swap(y2, y1);
    std::swap(z2, z1);
  }

  finish_isolines();
#if !TRAX_VALIDATE
  return result();
#else
  auto res = result();
  for (auto i = 0UL; i < res.size(); i++)
  {
    const auto& tmp = res[i];
    auto err = validate(tmp);
    if (!err.empty())
      std::cerr << "Isoline error: " << err << "\n"
                << "Isovalue: " << limits[i] << "\n"
                << "WKT:\n"
                << tmp.wkt() << "\n";
  }
  return res;
#endif
}

}  // namespace Trax
