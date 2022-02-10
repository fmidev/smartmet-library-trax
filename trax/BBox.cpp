#include "BBox.h"
#include <fmt/format.h>

#if 0
#include <iostream>
#endif

namespace Trax
{
// Initialize bbox from a polyline
void BBox::init(const Points& points)
{
  auto minx = points[0].x;
  auto miny = points[0].y;
  auto maxx = minx;
  auto maxy = miny;

  const auto n = points.size();
  for (auto i = 1UL; i < n; i++)
  {
    auto value = points[i].x;

    if (value < minx)
      minx = value;
    else if (value > maxx)
      maxx = value;

    value = points[i].y;
    if (value < miny)
      miny = value;
    else if (value > maxy)
      maxy = value;
  }

  m_minx = minx;
  m_maxx = maxx;
  m_miny = miny;
  m_maxy = maxy;
}

// Another bbox is contained using <= and >= comparisons since a hole may touch the exterior shell
bool BBox::contains(const BBox& other) const
{
  auto ret = (m_minx <= other.m_minx && m_maxx >= other.m_maxx && m_miny <= other.m_miny && m_maxy >= other.m_maxy);
#if 0
  std::cout << fmt::format("{} contains {} result = {}\n", wkt(), other.wkt(), ret);
#endif
  return ret;
}

std::string BBox::wkt() const
{
  return fmt::format("BBOX({},{} {},{})", m_minx, m_miny, m_maxx, m_maxy);
}

}  // namespace Trax
