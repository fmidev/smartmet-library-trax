#pragma once

#include "Point.h"
#include <deque>
#include <limits>
#include <string>
#include <vector>

namespace Trax
{
class BBox
{
 public:
  void init(const Points& points);
  bool contains(const BBox& other) const;
  std::string wkt() const;

  double xmin() const { return m_minx; }
  double ymin() const { return m_miny; }
  double xmax() const { return m_maxx; }
  double ymax() const { return m_maxy; }
  bool valid() const;

 private:
  double m_minx = std::numeric_limits<double>::quiet_NaN();
  double m_maxx = std::numeric_limits<double>::quiet_NaN();
  double m_miny = std::numeric_limits<double>::quiet_NaN();
  double m_maxy = std::numeric_limits<double>::quiet_NaN();
};

}  // namespace Trax
