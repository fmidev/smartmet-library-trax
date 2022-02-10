#include "GeometryCollection.h"
#include <algorithm>

namespace Trax
{
const std::vector<Polygon> &GeometryCollection::polygons() const
{
  return m_polygons;
}

const std::vector<Polyline> &GeometryCollection::polylines() const
{
  return m_polylines;
}

void GeometryCollection::add(Polyline &&polyline)
{
  m_polylines.emplace_back(polyline);
}

void GeometryCollection::add(Polygon &&upolygon)
{
  m_polygons.emplace_back(polygon);
}

std::string GeometryCollection::wkt() const
{
  // Process the linestring part first
  std::string linewkt;
  if (!m_polylines.empty())
  {
    if (m_polylines.size() == 1)
      linewkt = m_polylines[0].wkt();
    else
    {
      linewkt = "MULTILINESTRING (";
      for (auto i = 0UL; i < m_polylines.size(); i++)
      {
        if (i > 0)
          linewkt += ',';
        linewkt += m_polylines[i].wkt_body();
      }
      linewkt += ')';
    }
  }

  // Then the polygon part
  std::string polywkt;
  if (!m_polygons.empty())
  {
    if (m_polygons.size() == 1)
      polywkt = m_polygons[0].wkt();
    else
    {
      polywkt = "MULTIPOLYGON (";
      for (auto i = 0UL; i < m_polygons.size(); i++)
      {
        if (i > 0)
          polywkt += ',';
        polywkt += m_polygons[i].wkt_body();
      }
      polywkt += ')';
    }
  }

  // And merge the results
  if (linewkt.empty())
  {
    if (polywkt.empty())
      return "GEOMETRYCOLLECTION EMPTY";
    return polywkt;
  }

  if (polywkt.empty())
    return linewkt;

  return "GEOMETRYCOLLECTION (" + polywkt + ',' + linewkt + ')';
}

// Normalize to lexicographic order for testing purposes
GeometryCollection &GeometryCollection::normalize()
{
  for (auto &poly : m_polygons)
    poly.normalize();

  std::sort(m_polygons.begin(), m_polygons.end());
  std::sort(m_polylines.begin(), m_polylines.end());

  return *this;
}

void GeometryCollection::remove_ghosts()
{
  std::vector<Polygon> new_polygons;
  for (auto &poly : m_polygons)
    poly.remove_ghosts(new_polygons, m_polylines);
  m_polygons = std::move(new_polygons);
}

}  // namespace Trax
