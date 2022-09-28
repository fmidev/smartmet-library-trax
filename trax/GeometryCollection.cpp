#include "GeometryCollection.h"
#include <arpa/inet.h>
#include <algorithm>
#include <memory.h>

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

void GeometryCollection::add(Polygon &&polygon)
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

void GeometryCollection::wkb(std::ostringstream &out) const
{
  uint polyLineCount = m_polylines.size();
  uint polygonCount = m_polygons.size();

  unsigned char byteOrder = 1;
  int n = 1;
  if (*(char *)&n == 0)
    byteOrder = 0;

  // Process the linestring part first

  if (polyLineCount > 0)
  {
    if (polyLineCount == 1)
    {
      m_polylines[0].wkb(out);
    }
    else
    {
      uint type = 5;  // multi line string
      out.write((const char *)&byteOrder, sizeof(byteOrder));
      out.write((const char *)&type, sizeof(type));
      out.write((const char *)&polyLineCount, sizeof(polyLineCount));

      for (uint i = 0; i < polyLineCount; i++)
      {
        m_polylines[i].wkb(out);
      }
    }
  }

  // Then the polygon part
  if (polygonCount)
  {
    if (polygonCount == 1)
    {
      m_polygons[0].wkb(out);
    }
    else
    {
      uint type = 6;  // multi polygon
      out.write((const char *)&byteOrder, sizeof(byteOrder));
      out.write((const char *)&type, sizeof(type));
      out.write((const char *)&polygonCount, sizeof(polygonCount));
      for (uint i = 0; i < polygonCount; i++)
      {
        m_polygons[i].wkb(out);
      }
    }
  }
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

}  // namespace Trax
