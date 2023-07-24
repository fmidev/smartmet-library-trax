#include "GeometryCollection.h"
#include "Endian.h"
#include <algorithm>
#include <memory.h>

namespace Trax
{
namespace
{
std::string get_wkt(const std::vector<Polyline> &polylines)
{
  std::string wkt;
  if (!polylines.empty())
  {
    if (polylines.size() == 1)
      wkt = polylines[0].wkt();
    else
    {
      wkt = "MULTILINESTRING (";
      for (auto i = 0UL; i < polylines.size(); i++)
      {
        if (i > 0)
          wkt += ',';
        wkt += polylines[i].wkt_body();
      }
      wkt += ')';
    }
  }
  return wkt;
}

std::string get_wkt(const std::vector<Polygon> &polygons)
{
  std::string wkt;
  if (!polygons.empty())
  {
    if (polygons.size() == 1)
      wkt = polygons[0].wkt();
    else
    {
      wkt = "MULTIPOLYGON (";
      for (auto i = 0UL; i < polygons.size(); i++)
      {
        if (i > 0)
          wkt += ',';
        wkt += polygons[i].wkt_body();
      }
      wkt += ')';
    }
  }
  return wkt;
}

}  // namespace

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
  std::string linewkt = get_wkt(m_polylines);
  std::string polywkt = get_wkt(m_polygons);

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
      out.put(byteorder());
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
      out.put(byteorder());
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

template <typename T>
void desliver_container(T &container)
{
  bool emptied = false;
  for (auto &obj : container)
    emptied |= obj.desliver();

  // Should be very rare
  if (emptied)
  {
    T new_container;
    for (auto &obj : container)
      if (!obj.empty())
        new_container.emplace_back(obj);
    std::swap(container, new_container);
  }
}

// Remove slivers
void GeometryCollection::desliver()
{
  desliver_container(m_polygons);
  desliver_container(m_polylines);
}

}  // namespace Trax
