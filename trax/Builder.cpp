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
Builder::Builder(Builder &&other) noexcept
{
  m_geom = std::move(other.m_geom);
  m_merger = std::move(other.m_merger);
}

Builder::Builder(std::size_t /* width */, std::size_t /* height */) {}

void Builder::finish_isolines()
{
  finish_geometry(false);
}

void Builder::finish_geometry(bool isobands)
{
  Polygons polygons;
  Polylines holes;
  build_rings(polygons, holes, m_merger.pool());

  assign_holes(polygons, holes);

  for (auto &&poly : polygons)
    m_geom.add(std::move(poly));

  // Remove ghost vertices when calculating isolines
  if (!isobands)
    m_geom.remove_ghosts();
}

void Builder::finish_isobands()
{
  finish_geometry(true);
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
