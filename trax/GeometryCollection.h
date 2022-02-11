#pragma once

#include "Polygon.h"
#include "Polyline.h"
#include <vector>

namespace Trax
{
class GeometryCollection
{
 public:
  bool empty() const { return m_polylines.empty() && m_polygons.empty(); }
  void add(Polygon&& polygon);
  void add(Polyline&& polyline);
  std::string wkt() const;

  const std::vector<Polygon>& polygons() const;
  const std::vector<Polyline>& polylines() const;

  GeometryCollection& normalize();
  void remove_ghosts();

 private:
  std::vector<Polygon> m_polygons;
  std::vector<Polyline> m_polylines;
};

using GeometryCollections = std::vector<GeometryCollection>;

}  // namespace Trax
