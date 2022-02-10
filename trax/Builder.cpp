#include "Builder.h"
#include "Cell.h"
#include "Place.h"
#include "Range.h"
#include "Topology.h"
#include <fmt/format.h>
#include <smartmet/macgyver/Exception.h>
#include <stdexcept>

#if 1
#include "JointPool.h"
#include <iostream>
#endif

namespace Trax
{
Builder::Builder(Builder&& other)
{
  m_geom = std::move(other.m_geom);
  m_merger = std::move(other.m_merger);
}

Builder::Builder(std::size_t width, std::size_t height)
{
  // we build joints per row and then merge them to enable
  // easy parallelization over rows
  // m_joints.resize(height);
}

void Builder::finish_isolines()
{
  finish_geometry(false);
}

void Builder::finish_geometry(bool isobands)
{
  Polygons polygons;
  Polylines holes;
  Polylines lines;
  build_rings(polygons, holes, m_merger.pool());

  assign_holes(polygons, holes);

  for (auto&& poly : polygons)
    m_geom.add(poly);

  if (!isobands)
  {
    for (auto&& line : lines)
      m_geom.add(std::move(line));

    // Process unassigned holes. They lose their meaning without exteriors
    // which have been cut into plain isolines and become new exteriors without
    // any holes in them.
    for (auto& polyline : holes)
    {
      polyline.reverse();
      m_geom.add(Polygon(std::move(polyline)));
    }
  }

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
