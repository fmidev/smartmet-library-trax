#include "Builder.h"
#include "Cell.h"
#include "Place.h"
#include "Range.h"
#include "Topology.h"
#include <fmt/format.h>
#include <smartmet/macgyver/Exception.h>
#include <stdexcept>

namespace Trax
{
namespace
{
void remove_ghosts(Polylines& rings, Polylines& lines)
{
  for (auto it = rings.begin(), end = rings.end(); it != end;)
  {
    if (!it->has_ghosts())
      ++it;
    else
    {
      it->remove_ghosts(lines);
      it = rings.erase(it);
    }
  }
}

Polyline make_invert_bbox(double mincoord, double maxcoord)
{
  auto xmin = mincoord;
  auto xmax = maxcoord;
  auto ymin = mincoord;
  auto ymax = maxcoord;
  return Polyline{xmin, ymin, xmin, ymax, xmax, ymax, xmax, ymin, xmin, ymin};
}

void invert(Polylines& shells, Holes& holes, double mincoord, double maxcoord)
{
  for (auto& shell : shells)
    shell.reverse();

  for (auto& hole : holes)
    hole.reverse();

  std::swap(shells, holes);

  shells.push_back(make_invert_bbox(mincoord, maxcoord));
}

}  // namespace

Builder::Builder(Builder&& other) noexcept
{
  m_geom = std::move(other.m_geom);
  m_merger = std::move(other.m_merger);
}

Builder::Builder(std::size_t /* width */, std::size_t /* height */) {}

void Builder::finish_isolines(bool strict)
{
  Polylines shells;
  Holes holes;
  build_rings(shells, holes, m_merger.pool(), strict);

  // We do not build multipolygons out of the shells and holes since
  // the algorithm does not guarantee there will not be nested shells
  // which in turn could break geometry algorithms.

  Polylines lines;
  remove_ghosts(shells, lines);
  remove_ghosts(holes, lines);

  for (auto&& shell : shells)
    m_geom.add(std::move(shell));
  for (auto&& hole : holes)
    m_geom.add(std::move(hole));
  for (auto&& line : lines)
    m_geom.add(std::move(line));
}

void Builder::finish_isobands(bool strict, bool missing, double mincoord, double maxcoord)
{
  Polylines shells;
  Holes holes;
  build_rings(shells, holes, m_merger.pool(), strict);

#if 0
  std::cout << "Before:\n";
  for (const auto& shell : shells)
    std::cout << "\tshell: " << shell.wkt() << "\n";
  for (const auto& hole : holes)
    std::cout << "\thole: " << hole.wkt() << "\n";
#endif

  // Invert when contouring missing values
  if (missing)
  {
#if 0
    std::cout << "Inverting\n";
#endif
    invert(shells, holes, mincoord, maxcoord);
#if 0
    std::cout << "After:\n";
    for (const auto& shell : shells)
      std::cout << "\tshell: " << shell.wkt() << "\n";
    for (const auto& hole : holes)
      std::cout << "\thole: " << hole.wkt() << "\n";
#endif
  }

  Polygons polygons;
  build_polygons(polygons, shells, holes);

  for (auto&& poly : polygons)
    m_geom.add(std::move(poly));
}

void Builder::finish_row()
{
  m_merger.merge_row();
}

GeometryCollection Builder::result()
{
  return std::move(m_geom);
}

}  // namespace Trax
