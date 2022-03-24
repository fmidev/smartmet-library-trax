#include "CellBuilder.h"
#include "Cell.h"
#include "Joint.h"
#include "JointMerger.h"
#include "Place.h"
#include "Range.h"
#include "Vertex.h"
#include <limits>

#if 0
#include <fmt/format.h>
#include <iostream>
#endif

namespace Trax
{
namespace
{
struct point
{
  double x;
  double y;
  VertexType type;
  int di;
  int dj;
};

#if 0
bool print_it = false;
#endif

// Calculate intersection coordinates adjust to VertexType corner if necessary. di/dj are used only
// when the intersection is at a corner
point intersect(double x1,
                double y1,
                double z1,
                double x2,
                double y2,
                double z2,
                int di,
                int dj,
                VertexType type,
                double value)
{
#if 0  
  // These equality tests are necessary for handling value==lolimit cases without any rounding
  // errors! std::cout << fmt::format("{},{},{} - {},{},{} at {}\n", x1, y1, z1, x2, y2, z2, value);
  if (z1 == value)
    return {x1, y1, VertexType::Corner, 0, 0};
  if (z2 == value)
    return {x2, y2, VertexType::Corner, di, dj};
#endif
  if (x1 < x2 || (x1 == x2 && y1 < y2))  // lexicographic sorting to guarantee the same
  {                                      // result even if input x1,y1 and x2,y2 are swapped
    auto s = (value - z2) / (z1 - z2);
    auto x = x2 + s * (x1 - x2);
    auto y = y2 + s * (y1 - y2);
    if (x == x1 && y == y1)
      return {x, y, VertexType::Corner, 0, 0};
    if (x == x2 && y == y2)
      return {x, y, VertexType::Corner, di, dj};
    return {x, y, type, 0, 0};
  }

  auto s = (value - z1) / (z2 - z1);
  auto x = x1 + s * (x2 - x1);
  auto y = y1 + s * (y2 - y1);
  if (x == x1 && y == y1)
    return {x, y, VertexType::Corner, 0, 0};
  if (x == x2 && y == y2)
    return {x, y, VertexType::Corner, di, dj};
  return {x, y, type, 0, 0};
}

// Utilities to avoid highly likely typos in repetetive code. As luck would have it,
// I managed to mess up all four the first time...
point intersect_left(const Cell& c, VertexType type, const Range& range)
{
  const auto limit = (type == VertexType::Vertical_lo ? range.lo() : range.hi());
  return intersect(c.x1, c.y1, c.z1, c.x2, c.y2, c.z2, 0, 1, type, limit);
}

point intersect_top(const Cell& c, VertexType type, const Range& range)
{
  const auto limit = (type == VertexType::Horizontal_lo ? range.lo() : range.hi());
  return intersect(c.x2, c.y2, c.z2, c.x3, c.y3, c.z3, 1, 0, type, limit);
}

point intersect_right(const Cell& c, VertexType type, const Range& range)
{
  const auto limit = (type == VertexType::Vertical_lo ? range.lo() : range.hi());
  return intersect(c.x4, c.y4, c.z4, c.x3, c.y3, c.z3, 0, 1, type, limit);
}

point intersect_bottom(const Cell& c, VertexType type, const Range& range)
{
  const auto limit = (type == VertexType::Horizontal_lo ? range.lo() : range.hi());
  return intersect(c.x1, c.y1, c.z1, c.x4, c.y4, c.z4, 1, 0, type, limit);
}

Place center_place(const Cell& c, const Range& range)
{
  const auto z = (c.z1 + c.z2 + c.z3 + c.z4) / 4;
  return place(z, range);
}

/*
 * Private small builder class to connect the vertices properly in a single grid cell.
 */

class JointBuilder
{
 public:
  JointBuilder(JointMerger& joints, const Range& range) : m_range(range), m_joints(joints) {}

  void build_linear(const Cell& c);
  void build_midpoint(const Cell& c);
  void add(std::uint32_t column, std::uint32_t row, const point& p, double z);
  void add(std::uint32_t column, std::uint32_t row, VertexType vtype, double x, double y, double z);
  void close();
  void finish_cell();

 private:
  Range m_range;
  JointMerger& m_joints;
  Vertices m_vertices;
};

// Connect from previous to next by default, close() will fix the necessary prev/next indices
// for the first and last vertices. Note that if we push the same coordinate twice in a row,
// we assume we have situations like a B-A-B corner where the "Above" corner actually has
// the isoband hi limit value. In such cases the horizontal and vertical locations cancel
// and only the corner remains. We assume that the vertices are processed in such an order
// so that close() will not have to worry about such details.
// An implementation detail is how consecutive horizontal/vertical and vertical/horizontal
// coordinates merge. Since we're upgrading to a corner, atleast one of row/column must increase
// and neither can decrease (both row&column are equal at the bottom left corner). Hence a simple
// std::max solution is enough to merge the two vertices.

inline void JointBuilder::add(std::uint32_t column, std::uint32_t row, const point& p, double z)
{
  add(column + p.di, row + p.dj, p.type, p.x, p.y, z);
}

void JointBuilder::add(
    std::uint32_t column, std::uint32_t row, VertexType vtype, double x, double y, double z)
{
  const auto n = m_vertices.size();
  const bool ghost = z != m_range.lo();
  Vertex vertex(column, row, vtype, x, y, ghost);
  if (n == 0)
    m_vertices.push_back(vertex);
  else if (m_vertices.back() == vertex)  // avoid consecutive duplicates
  {
  }
#if 0  
  else if (match(m_vertices.back(), vertex))  // same coordinates but different type?
  {
    if (m_vertices.back().type != VertexType::Corner)  // prefer corners over edges
      m_vertices.back() = vertex;
  }
  else if (n >= 2 && m_vertices[n - 2] == vertex)  // return back to same vertex?
  {
    m_vertices.pop_back();  // cancel the protruding line possibly caused
    m_vertices.pop_back();  // by rounding errors
  }
#endif
  else
    m_vertices.push_back(vertex);
}

// Close the ring.
void JointBuilder::close()
{
#if 0
  if (print_it)
  {
    std::cout << "Vertices closed:\n";
    for (auto i = 0UL; i < m_vertices.size(); i++)
    {
      const auto& v = m_vertices[i];
      std::cout << fmt::format(
          "\t{}:\t{},{}\t{},{}\t{}\n", i, v.x, v.y, v.column, v.row, to_string(v.type));
    }
  }
#endif

  // Special case where the isoband covers only one edge needs removal:

  if (m_vertices[0] == m_vertices.back())
    m_vertices.pop_back();

  // Discard empty rings
  if (m_vertices.size() >= 3)
    m_joints.merge_cell(m_vertices);

  m_vertices.clear();
}

void JointBuilder::finish_cell()
{
  m_joints.finish_cell();
}

void JointBuilder::build_linear(const Cell& c)
{
  const auto c1 = place(c.z1, m_range);
  const auto c2 = place(c.z2, m_range);
  const auto c3 = place(c.z3, m_range);
  const auto c4 = place(c.z4, m_range);

  // Note that clockwise orientation must be produced at all times to
  // make sure shell / hole winding rules will be obeyed in the final
  // result and can be utilized when extracting the correct rings from
  // available alternative paths passing through grid cell
  // corners. Always turning right extracts the a single shell and all
  // the holes touching it. Then turning left will extract the holes
  // and the exterior shell as rings.

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // Always produce the vertices at the left vertical edge first for fastest
  // possible detection of edge cancellation in JointMerger::merge_cell.
  // Note that this includes A-A edges on the left in case either value is
  // equal to range.hi()
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#if 0
  print_it = true;
  if (print_it)
  {
    std::cout << fmt::format(
        "{},{}\t{} {}\t\t{} {}\t{} {}\t{} {}\n\t{} {}\t\t{} {}\t{} {}\t{} {}\n",
        c.i,
        c.j,
        c.z2,
        c.z3,
        c.x2,
        c.y2,
        c.x3,
        c.y3,
        c2,
        c3,
        c.z1,
        c.z4,
        c.x1,
        c.y1,
        c.x4,
        c.y4,
        c1,
        c4);
  }
#endif

  switch (place_hash(c1, c2, c3, c4))
  {
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Below, Place::Below):
    case TRAX_PLACE_HASH(Place::Above, Place::Above, Place::Above, Place::Above):
      break;

    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);      // |****|
      close();                                                      // I----I
      break;
    }

      // Corner triangles
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                          // B----B
      add(c.i + 1, c.j, p2, m_range.lo());                      // |    |
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // |   /|
      close();                                                  // |  /*|
      break;                                                    // B----I
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j + 1, p2, m_range.lo());                          // B----I
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |  \*|
      add(c.i + 1, c.j, p1, m_range.lo());                          // |   \|
      close();                                                      // |    |
      break;                                                        // B----B
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                          // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |*/  |
      add(c.i, c.j + 1, p2, m_range.lo());                      // |/   |
      close();                                                  // |    |
      break;                                                    // B----B
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // B----B
      add(c.i, c.j, p1, m_range.lo());                      // |    |
      add(c.i, c.j, p2, m_range.lo());                      // |\   |
      close();                                              // |*\  |
      break;                                                // I----B
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Above, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // A----A
      add(c.i, c.j, p1, m_range.hi());                      // |    |
      add(c.i, c.j, p2, m_range.hi());                      // |\   |
      close();                                              // |*\  |
      break;                                                // I----A
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Inside, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                          // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |*/  |
      add(c.i, c.j + 1, p2, m_range.hi());                      // |/   |
      close();                                                  // |    |
      break;                                                    // A----A
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Above, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j + 1, p1, m_range.hi());                          // A----I
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |  \*|
      add(c.i + 1, c.j, p2, m_range.hi());                          // |   \|
      close();                                                      // |    |
      break;                                                        // A----A
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Above, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                          // A----A
      add(c.i + 1, c.j, p2, m_range.hi());                      // |    |
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // |   /|
      close();                                                  // |  /*|
      break;                                                    // A----I
    }

      // Side rectangles

    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                              // B----I
      add(c.i, c.j + 1, p2, m_range.lo());                          // |  |*|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |  |*|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);      // |  |*|
      close();                                                      // B----I
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                              // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |----|
      add(c.i + 1, c.j, p2, m_range.lo());                          // |    |
      close();                                                      // B----B

      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);      // B----B
      add(c.i, c.j, p1, m_range.lo());                          // |    |
      add(c.i + 1, c.j, p2, m_range.lo());                      // |----|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // |****|
      close();                                                  // I----I
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Below, Place::Below):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);      // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |*|  |
      add(c.i, c.j + 1, p1, m_range.lo());                      // |*|  |
      add(c.i, c.j, p2, m_range.lo());                          // |*|  |
      close();                                                  // I----B
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Above, Place::Above):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);      // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |*|  |
      add(c.i, c.j + 1, p1, m_range.hi());                      // |*|  |
      add(c.i, c.j, p2, m_range.hi());                          // |*|  |
      close();                                                  // I----A
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Above, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);      // A----A
      add(c.i, c.j, p1, m_range.hi());                          // |    |
      add(c.i + 1, c.j, p2, m_range.hi());                      // |----|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // |****|
      close();                                                  // I----I
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Inside, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                              // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |----|
      add(c.i + 1, c.j, p2, m_range.hi());                          // |    |
      close();                                                      // A----A
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Above, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                              // A----I
      add(c.i, c.j + 1, p2, m_range.hi());                          // |  |*|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |  |*|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);      // |  |*|
      close();                                                      // A----I
      break;
    }

      // Side stripes
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Below, Place::Above):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());      // B----B
      add(c.i, c.j, p2, m_range.lo());      // |    |
      add(c.i + 1, c.j, p3, m_range.lo());  // |   /|
      add(c.i + 1, c.j, p4, m_range.hi());  // |  //|
      close();                              // B----A
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Above, Place::Below):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i + 1, c.j, p1, m_range.lo());  // B----A
      add(c.i, c.j + 1, p2, m_range.lo());  // |  \\|
      add(c.i, c.j + 1, p3, m_range.hi());  // |   \|
      add(c.i + 1, c.j, p4, m_range.hi());  // |    |
      close();                              // B----B
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Above, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());      // A----B
      add(c.i, c.j, p2, m_range.hi());      // |//  |
      add(c.i, c.j + 1, p3, m_range.hi());  // |/   |
      add(c.i, c.j + 1, p4, m_range.lo());  // |    |
      close();                              // B----B
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Above, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());  // A----A
      add(c.i, c.j, p2, m_range.hi());  // |    |
      add(c.i, c.j, p3, m_range.hi());  // |\   |
      add(c.i, c.j, p4, m_range.lo());  // |\\  |
      close();                          // B----A
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Below, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());  // B----B
      add(c.i, c.j, p2, m_range.lo());  // |    |
      add(c.i, c.j, p3, m_range.lo());  // |\   |
      add(c.i, c.j, p4, m_range.hi());  // |\\  |
      close();                          // A----B
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Below, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());      // B----A
      add(c.i, c.j, p2, m_range.lo());      // |//  |
      add(c.i, c.j + 1, p3, m_range.lo());  // |/   |
      add(c.i, c.j + 1, p4, m_range.hi());  // |    |
      close();                              // A----A
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Above, Place::Below, Place::Above):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i + 1, c.j, p1, m_range.hi());  // A----B
      add(c.i, c.j + 1, p2, m_range.hi());  // |  \\|
      add(c.i, c.j + 1, p3, m_range.lo());  // |   \|
      add(c.i + 1, c.j, p4, m_range.lo());  // |    |
      close();                              // A----A
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Above, Place::Above, Place::Below):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.hi());      // A----A
      add(c.i + 1, c.j, p2, m_range.hi());  // |    |
      add(c.i + 1, c.j, p3, m_range.lo());  // |   /|
      add(c.i, c.j, p4, m_range.lo());      // |  //|
      close();                              // A----B  A may be H!
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Above, Place::Above, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());      // A----A
      add(c.i, c.j, p2, m_range.hi());      // |    |
      add(c.i + 1, c.j, p3, m_range.hi());  // |====|
      add(c.i + 1, c.j, p4, m_range.lo());  // |    |
      close();                              // B----B
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Below, Place::Below, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());      // B----B
      add(c.i, c.j, p2, m_range.lo());      // |    |
      add(c.i + 1, c.j, p3, m_range.lo());  // |====|
      add(c.i + 1, c.j, p4, m_range.hi());  // |    |
      close();                              // A----A
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Above, Place::Below, Place::Below):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.hi());      // A----B
      add(c.i, c.j + 1, p2, m_range.hi());  // | || |
      add(c.i, c.j + 1, p3, m_range.lo());  // | || |
      add(c.i, c.j, p4, m_range.lo());      // | || |
      close();                              // A----B
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Above, Place::Above):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());      // B----A
      add(c.i, c.j, p2, m_range.lo());      // | || |
      add(c.i, c.j + 1, p3, m_range.lo());  // | || |
      add(c.i, c.j + 1, p4, m_range.hi());  // | || |
      close();                              // B----A
      break;
    }

      // Pentagons
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // B----I
      add(c.i + 1, c.j, p1, m_range.hi());                          // | |**|
      add(c.i, c.j, p2, m_range.hi());                              // | |**|
      add(c.i, c.j, p3, m_range.lo());                              // | |*/|
      add(c.i, c.j + 1, p4, m_range.lo());                          // B----A
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Above, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                          // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |***\|
      add(c.i, c.j + 1, p2, m_range.hi());                      // |****|
      add(c.i + 1, c.j, p3, m_range.hi());                      // |----|
      add(c.i + 1, c.j, p4, m_range.lo());                      // B----B
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                          // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |**| |
      add(c.i, c.j + 1, p2, m_range.hi());                      // |**| |
      add(c.i, c.j, p3, m_range.hi());                          // |\*| |
      add(c.i, c.j, p4, m_range.lo());                          // B----A
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Above, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                              // A----I
      add(c.i, c.j, p2, m_range.hi());                              // |/***|
      add(c.i, c.j + 1, p3, m_range.hi());                          // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |----|
      add(c.i + 1, c.j, p4, m_range.lo());                          // B----B
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // B----A
      add(c.i, c.j, p1, m_range.lo());                          // | |\ |
      add(c.i, c.j + 1, p2, m_range.lo());                      // | |*\|
      add(c.i, c.j + 1, p3, m_range.hi());                      // | |**|
      add(c.i + 1, c.j, p4, m_range.hi());                      // B----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                              // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |\***|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);      // | \**|
      add(c.i, c.j, p2, m_range.lo());                              // B----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Above, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                          // A----A
      add(c.i, c.j, p2, m_range.hi());                          // |    |
      add(c.i + 1, c.j, p3, m_range.hi());                      // |----|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // |\***|
      add(c.i, c.j, p4, m_range.lo());                          // B----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Below, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // B----B
      add(c.i, c.j, p1, m_range.lo());                      // |    |
      add(c.i + 1, c.j, p2, m_range.lo());                  // |----|
      add(c.i + 1, c.j, p3, m_range.hi());                  // |***/|
      add(c.i, c.j, p4, m_range.hi());                      // I----A
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // B----I
      add(c.i, c.j, p1, m_range.lo());                              // | /**|
      add(c.i, c.j + 1, p2, m_range.lo());                          // |/***|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);      // I----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // B----A
      add(c.i, c.j, p1, m_range.lo());                      // |/*| |
      add(c.i, c.j + 1, p2, m_range.lo());                  // |**| |
      add(c.i, c.j + 1, p3, m_range.hi());                  // |**| |
      add(c.i, c.j, p4, m_range.hi());                      // I----A
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);      // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |**\ |
      add(c.i, c.j + 1, p1, m_range.lo());                      // |***\|
      add(c.i + 1, c.j, p2, m_range.lo());                      // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // I----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |***/|
      add(c.i + 1, c.j, p1, m_range.lo());                          // |**/ |
      add(c.i, c.j, p2, m_range.lo());                              // I----B
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |***/|
      add(c.i + 1, c.j, p1, m_range.hi());                          // |**/ |
      add(c.i, c.j, p2, m_range.hi());                              // I----A
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);      // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |**\ |
      add(c.i, c.j + 1, p1, m_range.hi());                      // |***\|
      add(c.i + 1, c.j, p2, m_range.hi());                      // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // I----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Above, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // A----B
      add(c.i, c.j, p1, m_range.hi());                      // |/*| |
      add(c.i, c.j + 1, p2, m_range.hi());                  // |**| |
      add(c.i, c.j + 1, p3, m_range.lo());                  // |**| |
      add(c.i, c.j, p4, m_range.lo());                      // I----B
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Above, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // A----I
      add(c.i, c.j, p1, m_range.hi());                              // |/***|
      add(c.i, c.j + 1, p2, m_range.hi());                          // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);      // I----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Above, Place::Above, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // A----A
      add(c.i, c.j, p1, m_range.hi());                      // |----|
      add(c.i + 1, c.j, p2, m_range.hi());                  // |****|
      add(c.i + 1, c.j, p3, m_range.lo());                  // |***/|
      add(c.i, c.j, p4, m_range.lo());                      // I----B
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Below, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                          // B----B
      add(c.i, c.j, p2, m_range.lo());                          // |    |
      add(c.i + 1, c.j, p3, m_range.lo());                      // |----|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // |\***|
      add(c.i, c.j, p4, m_range.hi());                          // A----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Below, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                              // B----I
      add(c.i, c.j, p2, m_range.lo());                              // |/***|
      add(c.i, c.j + 1, p3, m_range.lo());                          // |----|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |    |
      add(c.i + 1, c.j, p4, m_range.hi());                          // A----A
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Inside, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                          // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |**| |
      add(c.i, c.j + 1, p2, m_range.lo());                      // |**| |
      add(c.i, c.j, p3, m_range.lo());                          // |\*| |
      add(c.i, c.j, p4, m_range.hi());                          // A----B
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Inside, Place::Below, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                          // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |***\|
      add(c.i, c.j + 1, p2, m_range.lo());                      // |****|
      add(c.i + 1, c.j, p3, m_range.lo());                      // |----|
      add(c.i + 1, c.j, p4, m_range.hi());                      // A----A
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Inside, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                              // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);      // |\***|
      add(c.i, c.j, p2, m_range.hi());                              // A----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Above, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // A----B
      add(c.i, c.j, p1, m_range.hi());                          // | |*\|
      add(c.i, c.j + 1, p2, m_range.hi());                      // | |**|
      add(c.i, c.j + 1, p3, m_range.lo());                      // | |**|
      add(c.i + 1, c.j, p4, m_range.lo());                      // A----I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Above, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.hi());                              // A----I
      add(c.i, c.j + 1, p2, m_range.hi());                          // | |**|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // | |**|
      add(c.i + 1, c.j, p3, m_range.lo());                          // | |*/|
      add(c.i, c.j, p4, m_range.lo());                              // A----B
      close();
      break;
    }

      // Hexagons
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Below, Place::Above):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);      // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |***\|
      add(c.i, c.j + 1, p1, m_range.lo());                      // |****|
      add(c.i + 1, c.j, p2, m_range.lo());                      // |***/|
      add(c.i + 1, c.j, p3, m_range.hi());                      // I----A
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Above, Place::Below):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);      // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |***\|
      add(c.i, c.j + 1, p1, m_range.hi());                      // |****|
      add(c.i + 1, c.j, p2, m_range.hi());                      // |***/|
      add(c.i + 1, c.j, p3, m_range.lo());                      // I----B
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Above, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                              // A----I
      add(c.i, c.j, p2, m_range.hi());                              // |/***|
      add(c.i, c.j + 1, p3, m_range.hi());                          // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |\***|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);      // B----I
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Below, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                              // B----I
      add(c.i, c.j, p2, m_range.lo());                              // |/***|
      add(c.i, c.j + 1, p3, m_range.lo());                          // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |\***|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);      // A----I
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                              // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |\***|
      add(c.i + 1, c.j, p2, m_range.hi());                          // | \*/|
      add(c.i, c.j, p3, m_range.hi());                              // B----A
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Inside, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                              // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |\***|
      add(c.i + 1, c.j, p2, m_range.lo());                          // | \*/|
      add(c.i, c.j, p3, m_range.lo());                              // A----B
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // B----A
      add(c.i, c.j, p1, m_range.lo());                      // | /*\|
      add(c.i, c.j + 1, p2, m_range.lo());                  // |/**\|
      add(c.i, c.j + 1, p3, m_range.hi());                  // |****|
      add(c.i + 1, c.j, p4, m_range.hi());                  // I----I
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Above, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // A----B
      add(c.i, c.j, p1, m_range.hi());                      // | /*\|
      add(c.i, c.j + 1, p2, m_range.hi());                  // |/**\|
      add(c.i, c.j + 1, p3, m_range.lo());                  // |****|
      add(c.i + 1, c.j, p4, m_range.lo());                  // I----I
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      close();
      break;
    }

    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                          // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |**\ |
      add(c.i, c.j + 1, p2, m_range.hi());                      // |\**\|
      add(c.i + 1, c.j, p3, m_range.hi());                      // | \**|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // B----I
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Inside, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                          // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |***\|
      add(c.i, c.j + 1, p2, m_range.lo());                      // |****|
      add(c.i + 1, c.j, p3, m_range.lo());                      // |\***|
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);  // A----I
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Above, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.hi());                              // A----I
      add(c.i, c.j + 1, p2, m_range.hi());                          // |/***|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |****|
      add(c.i + 1, c.j, p3, m_range.lo());                          // |***/|
      add(c.i, c.j, p4, m_range.lo());                              // I----B
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // B----I
      add(c.i, c.j, p1, m_range.lo());                              // |/***|
      add(c.i, c.j + 1, p2, m_range.lo());                          // |***/|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |**/ |
      add(c.i + 1, c.j, p3, m_range.hi());                          // I----A
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }

      // Saddle point cases need to be resolved by the value at the center of the
      // grid cell.

    case TRAX_PLACE_HASH(Place::Above, Place::Inside, Place::Above, Place::Inside):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                          // I----A   I----A
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |***\|   |*/  |
      add(c.i, c.j + 1, p2, m_range.hi());                      // |**I*|   |/ X/|
      if (cc != Place::Inside)                                  // |\***|   |  /*|
        close();                                                // A----I   A----I
      add(c.i + 1, c.j, p3, m_range.hi());
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Below, Place::Inside):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                          // I----B   I----B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |***\|   |*/  |
      add(c.i, c.j + 1, p2, m_range.lo());                      // |**I*|   |/ B/|
      if (cc != Place::Inside)                                  // |\***|   |  /*|
        close();                                                // B----I   B----I
      add(c.i + 1, c.j, p3, m_range.lo());
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }

    case TRAX_PLACE_HASH(Place::Inside, Place::Above, Place::Inside, Place::Above):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // A----I
        add(c.i, c.j, p1, m_range.hi());                              // |/***|
        add(c.i, c.j + 1, p2, m_range.hi());                          // |**I*|
        add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |***/|
        add(c.i + 1, c.j, p3, m_range.hi());                          // I----A
        add(c.i, c.j, p4, m_range.hi());
        close();
      }
      else
      {
        add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // A----I  top A could be H!
        add(c.i, c.j, p1, m_range.hi());                      // |  \*|
        add(c.i, c.j, p4, m_range.hi());                      // |\ A\|
        close();                                              // |*\  |
        // must be this order, possible corner touch!         // I----A
        add(c.i, c.j + 1, p2, m_range.hi());
        add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);
        add(c.i + 1, c.j, p3, m_range.hi());
        close();
      }
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Inside, Place::Below):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // B----I
        add(c.i, c.j, p1, m_range.lo());                              // | /**|
        add(c.i, c.j + 1, p2, m_range.lo());                          // |/*I/|
        add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |**/ |
        add(c.i + 1, c.j, p3, m_range.lo());                          // I----B
        add(c.i, c.j, p4, m_range.lo());
        close();
      }
      else
      {
        add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // B----I
        add(c.i, c.j, p1, m_range.lo());                              // |  \*|
        add(c.i, c.j, p4, m_range.lo());                              // |\ B\|
        close();                                                      // |*\  |
        add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // I----B
        add(c.i + 1, c.j, p3, m_range.lo());
        add(c.i, c.j + 1, p2, m_range.lo());
        close();
      }
      break;
    }

    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Below, Place::Above):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());                          // I----B    I----B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |***\|    |*/  |
      add(c.i, c.j + 1, p2, m_range.lo());                      // |\*I*|    |/ X/|  X = A or B
      if (cc != Place::Below)                                   // | \*/|    |  //|
        close();                                                // B----A    B----A
      add(c.i + 1, c.j, p3, m_range.lo());
      add(c.i + 1, c.j, p4, m_range.hi());
      add(c.i, c.j, p5, m_range.hi());
      add(c.i, c.j, p6, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Inside, Place::Above, Place::Below):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());                          // I----A   I----A
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);  // |***\|   |*/  |
      add(c.i, c.j + 1, p2, m_range.hi());                      // |**I*|   |/ X/| X = A or B
      if (cc != Place::Inside)                                  // |\**/|   |  //|
        close();                                                // A----B   A----B
      add(c.i + 1, c.j, p3, m_range.hi());
      add(c.i + 1, c.j, p4, m_range.lo());
      add(c.i, c.j, p5, m_range.lo());
      add(c.i, c.j, p6, m_range.hi());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Above, Place::Below, Place::Inside):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p5 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      add(c.i, c.j, p1, m_range.lo());      // A----B    A----B
      add(c.i, c.j, p2, m_range.hi());      // |/**\|    |//  |
      add(c.i, c.j + 1, p3, m_range.hi());  // |**I*|    |/ X/| X = A or B
      add(c.i, c.j + 1, p4, m_range.lo());  // |\***|    |  /*|
      if (cc != Place::Inside)              // B----I    B----I
        close();
      add(c.i + 1, c.j, p5, m_range.lo());
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      add(c.i, c.j, p6, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Below, Place::Above, Place::Inside):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p5 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      add(c.i, c.j, p1, m_range.hi());      // B----A    B----A
      add(c.i, c.j, p2, m_range.lo());      // |/**\|    |//  |
      add(c.i, c.j + 1, p3, m_range.lo());  // |**I*|    |/ X/| X = A or B
      add(c.i, c.j + 1, p4, m_range.hi());  // |\***|    |  /*|
      if (cc != Place::Inside)              // A----I    A----I
        close();
      add(c.i + 1, c.j, p5, m_range.hi());
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      add(c.i, c.j, p6, m_range.hi());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Above, Place::Below):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p5 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // B----A
        add(c.i, c.j, p1, m_range.lo());                      // |/*\ |
        add(c.i, c.j + 1, p2, m_range.lo());                  // |**I\|
        add(c.i, c.j + 1, p3, m_range.hi());                  // |***/|
        add(c.i + 1, c.j, p4, m_range.hi());                  // I----B
        add(c.i + 1, c.j, p5, m_range.lo());
        add(c.i, c.j, p6, m_range.lo());
        close();
      }
      else
      {
        add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // B----A
        add(c.i, c.j, p1, m_range.lo());                      // |  \\|
        add(c.i, c.j, p6, m_range.lo());                      // |\ X\| X = A or B
        close();                                              // |*\  |
        add(c.i, c.j + 1, p2, m_range.lo());                  // I----B
        add(c.i, c.j + 1, p3, m_range.hi());
        add(c.i + 1, c.j, p4, m_range.hi());
        add(c.i + 1, c.j, p5, m_range.lo());
        close();
      }
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Above, Place::Below, Place::Above):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p5 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // A----B
        add(c.i, c.j, p1, m_range.hi());                      // |/**\|
        add(c.i, c.j + 1, p2, m_range.hi());                  // |**I*|
        add(c.i, c.j + 1, p3, m_range.lo());                  // |***/|
        add(c.i + 1, c.j, p4, m_range.lo());                  // I----A
        add(c.i + 1, c.j, p5, m_range.hi());
        add(c.i, c.j, p6, m_range.hi());
        close();
      }
      else
      {
        add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);  // A----B
        add(c.i, c.j, p1, m_range.hi());                      // |  \\|
        add(c.i, c.j, p6, m_range.hi());                      // |\ X\| X = A or B
        close();                                              // |*\  |
        add(c.i, c.j + 1, p2, m_range.hi());                  // I----A
        add(c.i, c.j + 1, p3, m_range.lo());
        add(c.i + 1, c.j, p4, m_range.lo());
        add(c.i + 1, c.j, p5, m_range.hi());
        close();
      }
      break;
    }
    case TRAX_PLACE_HASH(Place::Above, Place::Below, Place::Inside, Place::Below):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, p1, m_range.hi());                              // B----I
        add(c.i, c.j, p2, m_range.lo());                              // |/***|
        add(c.i, c.j + 1, p3, m_range.lo());                          // |**I*|
        add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |\**/|
        add(c.i + 1, c.j, p4, m_range.lo());                          // A----B
        add(c.i, c.j, p5, m_range.lo());
        add(c.i, c.j, p6, m_range.hi());
        close();
      }
      else
      {
        add(c.i, c.j, p1, m_range.hi());  // B----I
        add(c.i, c.j, p2, m_range.lo());  // |  \*|
        add(c.i, c.j, p5, m_range.lo());  // |\ X\| X = A or B
        add(c.i, c.j, p6, m_range.hi());  // |\\  |
        close();                          // A----B
        add(c.i, c.j + 1, p3, m_range.lo());
        add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);
        add(c.i + 1, c.j, p4, m_range.lo());
        close();
      }
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Above, Place::Inside, Place::Above):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, p1, m_range.lo());                              // A----I
        add(c.i, c.j, p2, m_range.hi());                              // |/***|
        add(c.i, c.j + 1, p3, m_range.hi());                          // |**I*|
        add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // |\**/|
        add(c.i + 1, c.j, p4, m_range.hi());                          // B----A
        add(c.i, c.j, p5, m_range.hi());
        add(c.i, c.j, p6, m_range.lo());
        close();
      }
      else
      {
        add(c.i, c.j, p1, m_range.lo());  // A----I
        add(c.i, c.j, p2, m_range.hi());  // |  \*|
        add(c.i, c.j, p5, m_range.hi());  // |\ X\| X = A or B
        add(c.i, c.j, p6, m_range.lo());  // |\\  |
        close();                          // B----A
        add(c.i, c.j + 1, p3, m_range.hi());
        add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);
        add(c.i + 1, c.j, p4, m_range.hi());
        close();
      }
      break;
    }

    case TRAX_PLACE_HASH(Place::Below, Place::Above, Place::Below, Place::Above):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p5 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p6 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p7 = intersect_right(c, VertexType::Vertical_lo, m_range);
      const auto p8 = intersect_right(c, VertexType::Vertical_hi, m_range);
      if (cc == Place::Inside || (c.z2 == m_range.hi() && c.z4 == m_range.hi()))
      {
        add(c.i, c.j, p1, m_range.lo());      // A----B
        add(c.i, c.j, p2, m_range.hi());      // |***\|
        add(c.i, c.j + 1, p5, m_range.hi());  // |\*I\| or A=H
        add(c.i, c.j + 1, p6, m_range.lo());  // | \**|
        add(c.i + 1, c.j, p7, m_range.hi());  // B----A
        add(c.i + 1, c.j, p8, m_range.lo());
        add(c.i, c.j, p3, m_range.lo());
        add(c.i, c.j, p4, m_range.hi());
        close();
      }
      else if (cc == Place::Below)
      {
        add(c.i, c.j, p1, m_range.lo());      // A----B
        add(c.i, c.j, p2, m_range.hi());      // |//  |
        add(c.i, c.j + 1, p5, m_range.hi());  // |/ B/|
        add(c.i, c.j + 1, p6, m_range.lo());  // |  //|
        close();                              // B----A
        add(c.i + 1, c.j, p7, m_range.lo());
        add(c.i + 1, c.j, p8, m_range.hi());
        add(c.i, c.j, p3, m_range.hi());
        add(c.i, c.j, p4, m_range.lo());
        close();
      }
      else
      {
        add(c.i, c.j, p1, m_range.lo());  // A----B
        add(c.i, c.j, p2, m_range.hi());  // |  \\|
        add(c.i, c.j, p3, m_range.hi());  // |\ A\|
        add(c.i, c.j, p4, m_range.lo());  // |\\  |
        close();                          // B----A
        add(c.i, c.j + 1, p5, m_range.hi());
        add(c.i, c.j + 1, p6, m_range.lo());
        add(c.i + 1, c.j, p7, m_range.lo());
        add(c.i + 1, c.j, p8, m_range.hi());
        close();
      }
      break;
    }

    case TRAX_PLACE_HASH(Place::Above, Place::Below, Place::Above, Place::Below):
    {
      const auto cc = center_place(c, m_range);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi, m_range);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo, m_range);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo, m_range);
      const auto p4 = intersect_top(c, VertexType::Horizontal_hi, m_range);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_lo, m_range);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi, m_range);
      const auto p7 = intersect_right(c, VertexType::Vertical_hi, m_range);
      const auto p8 = intersect_right(c, VertexType::Vertical_lo, m_range);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, p1, m_range.hi());      // B----A
        add(c.i, c.j, p2, m_range.lo());      // |***\|
        add(c.i, c.j + 1, p3, m_range.lo());  // |\*I\|
        add(c.i, c.j + 1, p4, m_range.hi());  // | \**|
        add(c.i + 1, c.j, p7, m_range.hi());  // A----B
        add(c.i + 1, c.j, p8, m_range.lo());
        add(c.i, c.j, p5, m_range.lo());
        add(c.i, c.j, p6, m_range.hi());
        close();
      }
      else if (cc == Place::Below)
      {
        add(c.i, c.j, p1, m_range.hi());  // B----A
        add(c.i, c.j, p2, m_range.lo());  // |  \\|
        add(c.i, c.j, p5, m_range.lo());  // |\ B\|
        add(c.i, c.j, p6, m_range.hi());  // |\\  |
        close();                          // A----B
        add(c.i, c.j + 1, p3, m_range.lo());
        add(c.i, c.j + 1, p4, m_range.hi());
        add(c.i + 1, c.j, p7, m_range.hi());
        add(c.i + 1, c.j, p8, m_range.lo());
        close();
      }
      else
      {
        add(c.i, c.j, p1, m_range.hi());                   // B----A
        add(c.i, c.j, p2, m_range.lo());                   // |//  |
        add(c.i, c.j + 1, p3, m_range.lo());               // |/ A/|
        add(c.i, c.j + 1, p4, m_range.hi());               // |  //|
        if (c.z1 != m_range.hi() || c.z3 != m_range.hi())  // A----B
          close();
        add(c.i + 1, c.j, p7, m_range.hi());
        add(c.i + 1, c.j, p8, m_range.lo());
        add(c.i, c.j, p5, m_range.lo());
        add(c.i, c.j, p6, m_range.hi());
        close();
      }
      break;
    }
  }
  finish_cell();
}  // namespace

void JointBuilder::build_midpoint(const Cell& c)
{
  const auto c1 = discrete_place(c.z1, m_range);
  const auto c2 = discrete_place(c.z2, m_range);
  const auto c3 = discrete_place(c.z3, m_range);
  const auto c4 = discrete_place(c.z4, m_range);

  const auto xa = (c.x4 + c.x1) / 2;  // bottom center
  const auto ya = (c.y4 + c.y1) / 2;
  const auto xb = (c.x1 + c.x2) / 2;  // left center
  const auto yb = (c.y1 + c.y2) / 2;
  const auto xc = (c.x2 + c.x3) / 2;  // top center
  const auto yc = (c.y2 + c.y3) / 2;
  const auto xd = (c.x3 + c.x4) / 2;  // right center
  const auto yd = (c.y3 + c.y4) / 2;

  switch (place_hash(c1, c2, c3, c4))
  {
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Below, Place::Below):
    {
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Below, Place::Inside):
    {
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);           // B--B
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());    // | /|
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());  // B--I
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Inside, Place::Below):
    {
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);         // B--I
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());    // | \|
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // B--B
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Below, Place::Inside, Place::Inside):
    {
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);     // B--I
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);         // | ||
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());  // B--I
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Below, Place::Below):
    {
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());        // I--B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);             // |/ |
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // B--B
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Below, Place::Inside):
    {
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());        // I--B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);             // |//|
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // B--I
      close();
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Inside, Place::Below):
    {
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());  // I--I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);       // |--|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);   // B--B
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Below, Place::Inside, Place::Inside, Place::Inside):
    {
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());  // I--I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);       // |\ |
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);   // B--I
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Below, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);             // B--B
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());    // |\ |
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());  // I--B
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Below, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);               // B--B
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());      // |--|
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());  // I--I
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Inside, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);             // B--I
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());    // |\\|
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());  // I--B
      close();
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Below, Place::Inside, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);                 // B--I
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());        // |/ |
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // I--I
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Below, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);                 // I--B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);             // || |
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // I--B
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Below, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);                 // I--B
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);             // | \|
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // I--I
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // I--I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // | /|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // I--B
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());
      close();
      break;
    }
    case TRAX_PLACE_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.x1, c.y1, c.z1);          // I--I
      add(c.i, c.j + 1, VertexType::Corner, c.x2, c.y2, c.z2);      // |  |
      add(c.i + 1, c.j + 1, VertexType::Corner, c.x3, c.y3, c.z3);  // I--I
      add(c.i + 1, c.j, VertexType::Corner, c.x4, c.y4, c.z4);
      close();
      break;
    }
  }
  finish_cell();
}

}  // namespace

// ----------------------------------------------------------------------

namespace CellBuilder
{
void isoband_linear(JointMerger& joints, const Cell& c, const Range& range)
{
  JointBuilder b(joints, range);
  b.build_linear(c);
}

void isoline_linear(JointMerger& joints, const Cell& c, double limit)
{
  Range range(limit, std::numeric_limits<double>::infinity());
  JointBuilder b(joints, range);
  b.build_linear(c);
}

void isoband_midpoint(JointMerger& joints, const Cell& c, const Range& range)
{
  JointBuilder b(joints, range);
  b.build_midpoint(c);
}

}  // namespace CellBuilder

}  // namespace Trax
