
#pragma once

#include "Polyline.h"
#include <list>
#include <sstream>
#include <string>
#include <vector>

namespace Trax
{
class Polygon;

using Polygons = std::list<Polygon>;

class Polygon
{
 public:
  Polygon() = delete;
  explicit Polygon(Polyline exterior);
  Polygon(std::initializer_list<std::initializer_list<double>> init_list);

  void hole(Polyline hole);
  bool bbox_contains(const Polyline& hole) const;
  bool contains(const Polyline& hole) const;

  const Polyline& exterior() const;
  const std::vector<Polyline>& holes() const;

  Polygon& normalize();
  bool operator<(const Polygon& other) const;

  std::string wkt() const;
  std::string wkt_body() const;

  void wkb(std::ostringstream& out) const;
  void wkb_body(std::ostringstream& out) const;

 private:
  Polyline m_exterior;
  std::vector<Polyline> m_holes;
};

}  // namespace Trax
