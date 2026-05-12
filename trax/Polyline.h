#pragma once

#include "BBox.h"
#include "Point.h"
#include <initializer_list>
#include <list>
#include <sstream>
#include <string>
#include <vector>

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
  explicit Polyline(const Vertex& vertex);

  bool empty() const;
  std::size_t size() const;

  // Highest grid row touched by any appended Vertex. Used as a close-order key
  // by Topology::build_polygons to assign holes to shells without a global
  // bbox/rtree scan. Returns -1 if no Vertex has ever been appended.
  int max_row() const { return m_max_row; }

  double xbegin() const { return m_segments[0][0].x; }
  double ybegin() const { return m_segments[0][0].y; }
  double xend() const { return m_segments.back().back().x; }
  double yend() const { return m_segments.back().back().y; }

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
  void append(Polyline&& other);

  void reverse();

  void append(const Vertex& vertex);

  void update_bbox();

  bool has_ghosts() const;
  void remove_ghosts(Polylines& new_polylines);

 private:
  // Segmented storage. Each segment is a contiguous run of points; segment[i+1]
  // shares its first point with segment[i]'s last point (the join vertex). When
  // iterating a polyline as a whole, the first vertex of every non-leading
  // segment is skipped to avoid emitting the duplicate. This lets append(Polyline&&)
  // move source segments into m_segments without touching point data.
  std::vector<Points> m_segments;
  BBox m_bbox;
  int m_max_row = -1;

  // Collapse all segments into a single segment in m_segments[0]. Used by methods
  // whose existing logic depends on contiguous indexing (desliver, normalize,
  // remove_ghosts).
  void flatten();
};

}  // namespace Trax
