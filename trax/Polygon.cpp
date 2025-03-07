#include "Polygon.h"
#include "Endian.h"
#include <smartmet/macgyver/Exception.h>
#include <algorithm>

namespace Trax
{
Polygon::Polygon(Polyline exterior) : m_exterior(std::move(exterior))
{
  if (m_exterior.size() < 4)
    throw Fmi::Exception(BCP, "Polygon shells must have at least 4 points")
        .addParameter("WKT", m_exterior.wkt());
}

Polygon::Polygon(std::initializer_list<std::initializer_list<double>> init_list)
    : m_exterior(*init_list.begin())
{
  const auto* iter = init_list.begin();
  for (++iter; iter != init_list.end(); ++iter)
    hole(Polyline(*iter));
}

const Polyline& Polygon::exterior() const
{
  return m_exterior;
}

const std::vector<Polyline>& Polygon::holes() const
{
  return m_holes;
}

void Polygon::hole(Polyline hole)
{
  m_holes.emplace_back(std::move(hole));
}

bool Polygon::bbox_contains(const Polyline& hole) const
{
  return m_exterior.bbox().contains(hole.bbox());
}

bool Polygon::contains(const Polyline& hole) const
{
  return m_exterior.contains(hole);
}

std::string Polygon::wkt() const
{
  return "POLYGON " + wkt_body();
}

std::string Polygon::wkt_body() const
{
  std::string ret = "(";
  ret += m_exterior.wkt_body();

  for (const auto& h : m_holes)
  {
    ret += ',';
    ret += h.wkt_body();
  }
  ret += ')';
  return ret;
}

void Polygon::wkb(std::ostringstream& out) const
{
  out.put(byteorder());
  uint type = 3;  // polygon
  out.write((const char*)&type, sizeof(type));
  wkb_body(out);
}

void Polygon::wkb_body(std::ostringstream& out) const
{
  uint ringCount = m_holes.size() + 1;
  out.write((const char*)&ringCount, sizeof(ringCount));

  m_exterior.wkb_body(out);

  for (const auto& h : m_holes)
  {
    h.wkb_body(out);
  }
}

// Normalize for testing purposes
Polygon& Polygon::normalize()
{
  m_exterior.normalize();
  for (auto& h : m_holes)
    h.normalize();

  std::sort(m_holes.begin(), m_holes.end());

  return *this;
}

// For normalizing collections of polygons
bool Polygon::operator<(const Polygon& other) const
{
  if (this == &other)
    return false;

  // The exterior cannot be the same in contouring
  return m_exterior < other.m_exterior;
}

// Remove slivers
bool Polygon::desliver()
{
  // If exterior disappears, the polygon vanishes
  if (m_exterior.desliver())
  {
    m_holes.clear();
    return true;
  }

  bool emptied = false;
  for (auto& h : m_holes)
    emptied |= h.desliver();

  // Avoid unnecessary work if no holes disappeared
  if (emptied)
  {
    std::vector<Polyline> new_holes;
    for (auto& h : m_holes)
      if (!h.empty())
        new_holes.emplace_back(h);
    std::swap(m_holes, new_holes);
  }

  return false;  // since the exterior remains
}

}  // namespace Trax
