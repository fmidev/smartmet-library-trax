#pragma once

#include "BBox.h"
#include "Point.h"
#include <deque>
#include <initializer_list>
#include <list>
#include <sstream>
#include <string>

namespace Trax
{
struct Vertex;

class Polyline;
using Polylines = std::list<Polyline>;
using Holes = std::list<Polyline>;

class Polyline
{
 public:
  Polyline() = default;
  Polyline(std::initializer_list<double> init_list);

  bool empty() const { return m_points.empty(); }
  std::size_t size() const { return m_points.size(); }

  double xbegin() const { return m_points[0].x; }
  double ybegin() const { return m_points[0].y; }
  double xend() const { return m_points.back().x; }
  double yend() const { return m_points.back().y; }

  const BBox& bbox() const { return m_bbox; }
  bool bbox_contains(const Polyline& other) const;
  bool contains(const Polyline& other) const;

  // for testing if a hole is inside a polygon
  std::pair<double, double> inside_point() const;

  // for topology building
  double end_angle() const;
  double start_angle() const;

  // get all coordinates for building OGR/GDAL geometries
  std::vector<double> xcoordinates() const;
  std::vector<double> ycoordinates() const;

  Polyline& normalize();

  bool desliver();

  bool operator<(const Polyline& other) const;

  std::string wkt() const;
  std::string wkt_body() const;

  void wkb(std::ostringstream& out) const;
  void wkb_body(std::ostringstream& out) const;

  bool closed() const;
  bool stopped() const;
  bool clockwise() const;

  void append(const Polyline& other);

  void reverse();

  void append(const Vertex& vertex);

  void update_bbox();

  bool has_ghosts() const;
  void remove_ghosts(Polylines& new_polylines);

 private:
  Points m_points;
  BBox m_bbox;
};

}  // namespace Trax
