#include "CellBuilder.h"
#include "Cell.h"
#include "Joint.h"
#include "JointMerger.h"
#include "Place.h"
#include "Range.h"
#include "Vertex.h"
#include <fmt/format.h>
#include <limits>

#if 0
#include <fmt/format.h>
#include <iostream>
bool print_it = true;
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

/*
 * Private small builder class to connect the vertices properly in a single grid cell.
 */

class JointBuilder
{
 public:
  JointBuilder(JointMerger& joints, const Range& range) : m_range(range), m_joints(joints) {}

  void build_linear(const Cell& c);
  void build_midpoint(const Cell& c, double shell);
  void build_missing(const Cell& c);  // with linear interpolation

  void set_logarithmic_mode() { m_logarithmic = true; }
  void set_isoline_mode() { m_isoline = true; }

 private:
  void build_edge(VertexType type,
                  int di,
                  int dj,
                  Place c1,
                  float z1,
                  int i1,
                  int j1,
                  const GridPoint& g1,
                  Place c2,
                  float z2,
                  int i2,
                  int j2,
                  const GridPoint& g2);
  void add(std::int32_t column, std::int32_t row, const point& p, float z);
  void add(std::int32_t column, std::int32_t row, VertexType vtype, const GridPoint& p);
  void add(std::int32_t column, std::int32_t row, VertexType vtype, const GridPoint& p, bool ghost);
  void add(std::int32_t column, std::int32_t row, VertexType vtype, double x, double y, float z);
  void add(std::int32_t column,
           std::int32_t row,
           VertexType vtype,
           double x,
           double y,
           float z,
           bool ghost);
  void close();
  void finish_cell();

  point intersect(const GridPoint& p1,
                  const GridPoint& p2,
                  int di,
                  int dj,
                  VertexType type,
                  double value) const;
  point intersect_left(const Cell& c, VertexType type) const;
  point intersect_top(const Cell& c, VertexType type) const;
  point intersect_right(const Cell& c, VertexType type) const;
  point intersect_bottom(const Cell& c, VertexType type) const;
  Place center_place(const Cell& c) const;

  Range m_range;
  JointMerger& m_joints;
  Vertices m_vertices;
  bool m_logarithmic = false;
  bool m_isoline = false;
};

point JointBuilder::intersect(
    const GridPoint& p1, const GridPoint& p2, int di, int dj, VertexType type, double value) const
{
#ifdef HANDLE_ROUNDING_ERRORS
  // These equality tests are necessary for handling value==lolimit cases without any rounding
  // errors! std::cout << fmt::format("{},{},{} - {},{},{} at {}\n", x1, y1, z1, x2, y2, z2, value);
  if (p1.z == value)
    return {p1.x, p1.y, VertexType::Corner, 0, 0};
  if (p2.z == value)
    return {p2.x, p2.y, VertexType::Corner, di, dj};
#endif
  if (p1.x < p2.x || (p1.x == p2.x && p1.y < p2.y))  // lexicographic sorting to guarantee the same
  {  // result even if input x1,y1 and x2,y2 are swapped
    auto s = (!m_logarithmic
                  ? (value - p2.z) / (p1.z - p2.z)
                  : (std::log1p(value) - std::log1p(p2.z)) / (std::log1p(p1.z) - std::log1p(p2.z)));
    auto x = p2.x + s * (p1.x - p2.x);
    auto y = p2.y + s * (p1.y - p2.y);
    if (x == p1.x && y == p1.y)
      return {x, y, VertexType::Corner, 0, 0};
    if (x == p2.x && y == p2.y)
      return {x, y, VertexType::Corner, di, dj};
    return {x, y, type, 0, 0};
  }

  auto s = (!m_logarithmic
                ? (value - p1.z) / (p2.z - p1.z)
                : (std::log1p(value) - std::log1p(p1.z)) / (std::log1p(p2.z) - std::log1p(p1.z)));

  auto x = p1.x + s * (p2.x - p1.x);
  auto y = p1.y + s * (p2.y - p1.y);
  if (x == p1.x && y == p1.y)
    return {x, y, VertexType::Corner, 0, 0};
  if (x == p2.x && y == p2.y)
    return {x, y, VertexType::Corner, di, dj};
  return {x, y, type, 0, 0};
}

// Utilities to avoid highly likely typos in repetetive code. As luck would have it,
// I managed to mess up all four the first time...
point JointBuilder::intersect_left(const Cell& c, VertexType type) const
{
  const auto limit = (type == VertexType::Vertical_lo ? m_range.lo() : m_range.hi());
  return intersect(c.p1, c.p2, 0, 1, type, limit);
}

point JointBuilder::intersect_top(const Cell& c, VertexType type) const
{
  const auto limit = (type == VertexType::Horizontal_lo ? m_range.lo() : m_range.hi());
  return intersect(c.p2, c.p3, 1, 0, type, limit);
}

point JointBuilder::intersect_right(const Cell& c, VertexType type) const
{
  const auto limit = (type == VertexType::Vertical_lo ? m_range.lo() : m_range.hi());
  return intersect(c.p4, c.p3, 0, 1, type, limit);
}

point JointBuilder::intersect_bottom(const Cell& c, VertexType type) const
{
  const auto limit = (type == VertexType::Horizontal_lo ? m_range.lo() : m_range.hi());
  return intersect(c.p1, c.p4, 1, 0, type, limit);
}

Place JointBuilder::center_place(const Cell& c) const
{
  if (!m_logarithmic)
  {
    const auto z = (c.p1.z + c.p2.z + c.p3.z + c.p4.z) / 4;
    return place(z, m_range);
  }

  const auto z = std::expm1(
      (std::log1p(c.p1.z) + std::log1p(c.p2.z) + std::log1p(c.p3.z) + std::log1p(c.p4.z)) / 4);
  return place(z, m_range);
}

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

inline void JointBuilder::add(std::int32_t column, std::int32_t row, const point& p, float z)
{
  add(column + p.di, row + p.dj, p.type, p.x, p.y, z);
}

void JointBuilder::add(std::int32_t column, std::int32_t row, VertexType vtype, const GridPoint& p)
{
  add(column, row, vtype, p.x, p.y, p.z);
}

void JointBuilder::add(
    std::int32_t column, std::int32_t row, VertexType vtype, const GridPoint& p, bool ghost)
{
  add(column, row, vtype, p.x, p.y, p.z, ghost);
}

void JointBuilder::add(
    std::int32_t column, std::int32_t row, VertexType vtype, double x, double y, float z)
{
  // Note that NaN is always marked a ghost as needed for midpoint algorithm
  const bool ghost = z != m_range.lo();
  add(column, row, vtype, x, y, z, ghost);
}

void JointBuilder::add(std::int32_t column,
                       std::int32_t row,
                       VertexType vtype,
                       double x,
                       double y,
                       float z,
                       bool ghost)
{
  Vertex vertex(column, row, vtype, x, y, ghost);

  const auto n = m_vertices.size();
  if (n == 0)
    m_vertices.push_back(vertex);        // NOLINT(bugprone-branch-clone)
  else if (m_vertices.back() == vertex)  // avoid consecutive duplicates
  {
  }
#ifdef HANDLE_REDUNDANCIES
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
      std::cout << fmt::format("\t{}:\t{},{}\t{},{}\t{}\t{}\n",
                               i,
                               v.x,
                               v.y,
                               v.column,
                               v.row,
                               v.ghost ? "G" : "-",
                               to_string(v.type));
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
  if (m_range.missing())
  {
    build_missing(c);
    return;
  }

  const auto c1 = place(c.p1.z, m_range);
  const auto c2 = place(c.p2.z, m_range);
  const auto c3 = place(c.p3.z, m_range);
  const auto c4 = place(c.p4.z, m_range);

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
        "\n{},{}\t{} {}\t\t{},{}\t{},{}\t{} {}\n\t{} {}\t\t{},{}\t{},{}\t{} {}\t\trange={}...{}\n",
        c.i,
        c.j,
        c.p2.z,
        c.p3.z,
        c.p2.x,
        c.p2.y,
        c.p3.x,
        c.p3.y,
        to_string(c2),
        to_string(c3),
        c.p1.z,
        c.p4.z,
        c.p1.x,
        c.p1.y,
        c.p4.x,
        c.p4.y,
        to_string(c1),
        to_string(c4),
        m_range.lo(),
        m_range.hi());
  }
#endif

  switch (place_hash(c1, c2, c3, c4))
  {
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Below, Place::Below):
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Above, Place::Above):
      break;

    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.p1);          // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);      // |****|
      close();                                          // I----I
      break;
    }

    // Corner triangles
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i, c.j, p1, m_range.lo());              // B----B
      add(c.i + 1, c.j, p2, m_range.lo());          // |    |
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // |   /|
      close();                                      // |  /*|
      break;                                        // B----I
    }
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      add(c.i, c.j + 1, p2, m_range.lo());              // B----I
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |  \*|
      add(c.i + 1, c.j, p1, m_range.lo());              // |   \|
      close();                                          // |    |
      break;                                            // B----B
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());              // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |*/  |
      add(c.i, c.j + 1, p2, m_range.lo());          // |/   |
      close();                                      // |    |
      break;                                        // B----B
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);  // B----B
      add(c.i, c.j, p1, m_range.lo());          // |    |
      add(c.i, c.j, p2, m_range.lo());          // |\   |
      close();                                  // |*\  |
      break;                                    // I----B
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);  // A----A
      add(c.i, c.j, p1, m_range.hi());          // |    |
      add(c.i, c.j, p2, m_range.hi());          // |\   |
      close();                                  // |*\  |
      break;                                    // I----A
    }
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());              // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |*/  |
      add(c.i, c.j + 1, p2, m_range.hi());          // |/   |
      close();                                      // |    |
      break;                                        // A----A
    }
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j + 1, p1, m_range.hi());              // A----I
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |  \*|
      add(c.i + 1, c.j, p2, m_range.hi());              // |   \|
      close();                                          // |    |
      break;                                            // A----A
    }
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j, p1, m_range.hi());              // A----A
      add(c.i + 1, c.j, p2, m_range.hi());          // |    |
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // |   /|
      close();                                      // |  /*|
      break;                                        // A----I
    }

    // Side rectangles
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());                  // B----I
      add(c.i, c.j + 1, p2, m_range.lo());              // |  |*|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |  |*|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);      // |  |*|
      close();                                          // B----I
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i, c.j, p1, m_range.lo());                  // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |----|
      add(c.i + 1, c.j, p2, m_range.lo());              // |    |
      close();                                          // B----B

      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);      // B----B
      add(c.i, c.j, p1, m_range.lo());              // |    |
      add(c.i + 1, c.j, p2, m_range.lo());          // |----|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // |****|
      close();                                      // I----I
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Below, Place::Below):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);      // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |*|  |
      add(c.i, c.j + 1, p1, m_range.lo());          // |*|  |
      add(c.i, c.j, p2, m_range.lo());              // |*|  |
      close();                                      // I----B
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Above, Place::Above):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);      // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |*|  |
      add(c.i, c.j + 1, p1, m_range.hi());          // |*|  |
      add(c.i, c.j, p2, m_range.hi());              // |*|  |
      close();                                      // I----A
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);      // A----A
      add(c.i, c.j, p1, m_range.hi());              // |    |
      add(c.i + 1, c.j, p2, m_range.hi());          // |----|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // |****|
      close();                                      // I----I
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j, p1, m_range.hi());                  // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |----|
      add(c.i + 1, c.j, p2, m_range.hi());              // |    |
      close();                                          // A----A
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());                  // A----I
      add(c.i, c.j + 1, p2, m_range.hi());              // |  |*|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |  |*|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);      // |  |*|
      close();                                          // A----I
      break;
    }

      // Side stripes
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Below, Place::Above):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j, p1, m_range.hi());      // B----B
      add(c.i, c.j, p2, m_range.lo());      // |    |
      add(c.i + 1, c.j, p3, m_range.lo());  // |   /|
      add(c.i + 1, c.j, p4, m_range.hi());  // |  //|
      close();                              // B----A
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Above, Place::Below):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i + 1, c.j, p1, m_range.lo());  // B----A
      add(c.i, c.j + 1, p2, m_range.lo());  // |  \\|
      add(c.i, c.j + 1, p3, m_range.hi());  // |   \|
      add(c.i + 1, c.j, p4, m_range.hi());  // |    |
      close();                              // B----B
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_top(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());      // A----B
      add(c.i, c.j, p2, m_range.hi());      // |//  |
      add(c.i, c.j + 1, p3, m_range.hi());  // |/   |
      add(c.i, c.j + 1, p4, m_range.lo());  // |    |
      close();                              // B----B
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());  // A----A
      add(c.i, c.j, p2, m_range.hi());  // |    |
      add(c.i, c.j, p3, m_range.hi());  // |\   |
      add(c.i, c.j, p4, m_range.lo());  // |\\  |
      close();                          // B----A
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());  // B----B
      add(c.i, c.j, p2, m_range.lo());  // |    |
      add(c.i, c.j, p3, m_range.lo());  // |\   |
      add(c.i, c.j, p4, m_range.hi());  // |\\  |
      close();                          // A----B
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_top(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());      // B----A
      add(c.i, c.j, p2, m_range.lo());      // |//  |
      add(c.i, c.j + 1, p3, m_range.lo());  // |/   |
      add(c.i, c.j + 1, p4, m_range.hi());  // |    |
      close();                              // A----A
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Below, Place::Above):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i + 1, c.j, p1, m_range.hi());  // A----B
      add(c.i, c.j + 1, p2, m_range.hi());  // |  \\|
      add(c.i, c.j + 1, p3, m_range.lo());  // |   \|
      add(c.i + 1, c.j, p4, m_range.lo());  // |    |
      close();                              // A----A
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Above, Place::Below):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.hi());      // A----A
      add(c.i + 1, c.j, p2, m_range.hi());  // |    |
      add(c.i + 1, c.j, p3, m_range.lo());  // |   /|
      add(c.i, c.j, p4, m_range.lo());      // |  //|
      close();                              // A----B  A may be H!
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Above, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i, c.j, p1, m_range.lo());      // A----A
      add(c.i, c.j, p2, m_range.hi());      // |    |
      add(c.i + 1, c.j, p3, m_range.hi());  // |====|
      add(c.i + 1, c.j, p4, m_range.lo());  // |    |
      close();                              // B----B
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Below, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j, p1, m_range.hi());      // B----B
      add(c.i, c.j, p2, m_range.lo());      // |    |
      add(c.i + 1, c.j, p3, m_range.lo());  // |====|
      add(c.i + 1, c.j, p4, m_range.hi());  // |    |
      close();                              // A----A
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Below, Place::Below):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.hi());      // A----B
      add(c.i, c.j + 1, p2, m_range.hi());  // | || |
      add(c.i, c.j + 1, p3, m_range.lo());  // | || |
      add(c.i, c.j, p4, m_range.lo());      // | || |
      close();                              // A----B
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Above, Place::Above):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_top(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());      // B----A
      add(c.i, c.j, p2, m_range.lo());      // | || |
      add(c.i, c.j + 1, p3, m_range.lo());  // | || |
      add(c.i, c.j + 1, p4, m_range.hi());  // | || |
      close();                              // B----A
      break;
    }

    // Pentagons
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_hi);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_top(c, VertexType::Horizontal_lo);
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // B----I
      add(c.i + 1, c.j, p1, m_range.hi());              // | |**|
      add(c.i, c.j, p2, m_range.hi());                  // | |**|
      add(c.i, c.j, p3, m_range.lo());                  // | |*/|
      add(c.i, c.j + 1, p4, m_range.lo());              // B----A
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Above, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i, c.j, p1, m_range.lo());              // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |***\|
      add(c.i, c.j + 1, p2, m_range.hi());          // |****|
      add(c.i + 1, c.j, p3, m_range.hi());          // |----|
      add(c.i + 1, c.j, p4, m_range.lo());          // B----B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());              // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |**| |
      add(c.i, c.j + 1, p2, m_range.hi());          // |**| |
      add(c.i, c.j, p3, m_range.hi());              // |\*| |
      add(c.i, c.j, p4, m_range.lo());              // B----A
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i, c.j, p1, m_range.lo());                  // A----I
      add(c.i, c.j, p2, m_range.hi());                  // |/***|
      add(c.i, c.j + 1, p3, m_range.hi());              // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |----|
      add(c.i + 1, c.j, p4, m_range.lo());              // B----B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // B----A
      add(c.i, c.j, p1, m_range.lo());              // | |\ |
      add(c.i, c.j + 1, p2, m_range.lo());          // | |*\|
      add(c.i, c.j + 1, p3, m_range.hi());          // | |**|
      add(c.i + 1, c.j, p4, m_range.hi());          // B----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());                  // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |\***|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);      // | \**|
      add(c.i, c.j, p2, m_range.lo());                  // B----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());              // A----A
      add(c.i, c.j, p2, m_range.hi());              // |    |
      add(c.i + 1, c.j, p3, m_range.hi());          // |----|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // |\***|
      add(c.i, c.j, p4, m_range.lo());              // B----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Below, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);  // B----B
      add(c.i, c.j, p1, m_range.lo());          // |    |
      add(c.i + 1, c.j, p2, m_range.lo());      // |----|
      add(c.i + 1, c.j, p3, m_range.hi());      // |***/|
      add(c.i, c.j, p4, m_range.hi());          // I----A
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);          // B----I
      add(c.i, c.j, p1, m_range.lo());                  // | /**|
      add(c.i, c.j + 1, p2, m_range.lo());              // |/***|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);      // I----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Above, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);  // B----A
      add(c.i, c.j, p1, m_range.lo());          // |/*| |
      add(c.i, c.j + 1, p2, m_range.lo());      // |**| |
      add(c.i, c.j + 1, p3, m_range.hi());      // |**| |
      add(c.i, c.j, p4, m_range.hi());          // I----A
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);      // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |**\ |
      add(c.i, c.j + 1, p1, m_range.lo());          // |***\|
      add(c.i + 1, c.j, p2, m_range.lo());          // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // I----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_lo);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);          // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |***/|
      add(c.i + 1, c.j, p1, m_range.lo());              // |**/ |
      add(c.i, c.j, p2, m_range.lo());                  // I----B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_right(c, VertexType::Vertical_hi);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);          // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |***/|
      add(c.i + 1, c.j, p1, m_range.hi());              // |**/ |
      add(c.i, c.j, p2, m_range.hi());                  // I----A
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);      // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |**\ |
      add(c.i, c.j + 1, p1, m_range.hi());          // |***\|
      add(c.i + 1, c.j, p2, m_range.hi());          // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // I----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);  // A----B
      add(c.i, c.j, p1, m_range.hi());          // |/*| |
      add(c.i, c.j + 1, p2, m_range.hi());      // |**| |
      add(c.i, c.j + 1, p3, m_range.lo());      // |**| |
      add(c.i, c.j, p4, m_range.lo());          // I----B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);          // A----I
      add(c.i, c.j, p1, m_range.hi());                  // |/***|
      add(c.i, c.j + 1, p2, m_range.hi());              // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);      // I----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Above, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);  // A----A
      add(c.i, c.j, p1, m_range.hi());          // |----|
      add(c.i + 1, c.j, p2, m_range.hi());      // |****|
      add(c.i + 1, c.j, p3, m_range.lo());      // |***/|
      add(c.i, c.j, p4, m_range.lo());          // I----B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());              // B----B
      add(c.i, c.j, p2, m_range.lo());              // |    |
      add(c.i + 1, c.j, p3, m_range.lo());          // |----|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // |\***|
      add(c.i, c.j, p4, m_range.hi());              // A----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j, p1, m_range.hi());                  // B----I
      add(c.i, c.j, p2, m_range.lo());                  // |/***|
      add(c.i, c.j + 1, p3, m_range.lo());              // |----|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |    |
      add(c.i + 1, c.j, p4, m_range.hi());              // A----A
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Below, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());              // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |**| |
      add(c.i, c.j + 1, p2, m_range.lo());          // |**| |
      add(c.i, c.j, p3, m_range.lo());              // |\*| |
      add(c.i, c.j, p4, m_range.hi());              // A----B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Below, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j, p1, m_range.hi());              // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |***\|
      add(c.i, c.j + 1, p2, m_range.lo());          // |****|
      add(c.i + 1, c.j, p3, m_range.lo());          // |----|
      add(c.i + 1, c.j, p4, m_range.hi());          // A----A
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());                  // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |****|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);      // |\***|
      add(c.i, c.j, p2, m_range.hi());                  // A----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // A----B
      add(c.i, c.j, p1, m_range.hi());              // | |*\|
      add(c.i, c.j + 1, p2, m_range.hi());          // | |**|
      add(c.i, c.j + 1, p3, m_range.lo());          // | |**|
      add(c.i + 1, c.j, p4, m_range.lo());          // A----I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.hi());                  // A----I
      add(c.i, c.j + 1, p2, m_range.hi());              // | |**|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // | |**|
      add(c.i + 1, c.j, p3, m_range.lo());              // | |*/|
      add(c.i, c.j, p4, m_range.lo());                  // A----B
      close();
      break;
    }

    // Hexagons
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Below, Place::Above):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);      // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |***\|
      add(c.i, c.j + 1, p1, m_range.lo());          // |****|
      add(c.i + 1, c.j, p2, m_range.lo());          // |***/|
      add(c.i + 1, c.j, p3, m_range.hi());          // I----A
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Above, Place::Below):
    {
      const auto p1 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);      // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |***\|
      add(c.i, c.j + 1, p1, m_range.hi());          // |****|
      add(c.i + 1, c.j, p2, m_range.hi());          // |***/|
      add(c.i + 1, c.j, p3, m_range.lo());          // I----B
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());                  // A----I
      add(c.i, c.j, p2, m_range.hi());                  // |/***|
      add(c.i, c.j + 1, p3, m_range.hi());              // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |\***|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);      // B----I
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Inside, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());                  // B----I
      add(c.i, c.j, p2, m_range.lo());                  // |/***|
      add(c.i, c.j + 1, p3, m_range.lo());              // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |\***|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);      // A----I
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_right(c, VertexType::Vertical_hi);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());                  // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |\***|
      add(c.i + 1, c.j, p2, m_range.hi());              // | \*/|
      add(c.i, c.j, p3, m_range.hi());                  // B----A
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_right(c, VertexType::Vertical_lo);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());                  // I----I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |****|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |\***|
      add(c.i + 1, c.j, p2, m_range.lo());              // | \*/|
      add(c.i, c.j, p3, m_range.lo());                  // A----B
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);  // B----A
      add(c.i, c.j, p1, m_range.lo());          // | /*\|
      add(c.i, c.j + 1, p2, m_range.lo());      // |/**\|
      add(c.i, c.j + 1, p3, m_range.hi());      // |****|
      add(c.i + 1, c.j, p4, m_range.hi());      // I----I
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo);
      add(c.i, c.j, VertexType::Corner, c.p1);  // A----B
      add(c.i, c.j, p1, m_range.hi());          // | /*\|
      add(c.i, c.j + 1, p2, m_range.hi());      // |/**\|
      add(c.i, c.j + 1, p3, m_range.lo());      // |****|
      add(c.i + 1, c.j, p4, m_range.lo());      // I----I
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      close();
      break;
    }

    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Above, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());              // I----A
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |**\ |
      add(c.i, c.j + 1, p2, m_range.hi());          // |\**\|
      add(c.i + 1, c.j, p3, m_range.hi());          // | \**|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // B----I
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Below, Place::Inside):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());              // I----B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |***\|
      add(c.i, c.j + 1, p2, m_range.lo());          // |****|
      add(c.i + 1, c.j, p3, m_range.lo());          // |\***|
      add(c.i + 1, c.j, VertexType::Corner, c.p4);  // A----I
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Inside, Place::Below):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.hi());                  // A----I
      add(c.i, c.j + 1, p2, m_range.hi());              // |/***|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |****|
      add(c.i + 1, c.j, p3, m_range.lo());              // |***/|
      add(c.i, c.j, p4, m_range.lo());                  // I----B
      add(c.i, c.j, VertexType::Corner, c.p1);
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Inside, Place::Above):
    {
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, VertexType::Corner, c.p1);          // B----I
      add(c.i, c.j, p1, m_range.lo());                  // |/***|
      add(c.i, c.j + 1, p2, m_range.lo());              // |***/|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |**/ |
      add(c.i + 1, c.j, p3, m_range.hi());              // I----A
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }

    // Saddle point cases need to be resolved by the value at the center of the
    // grid cell.
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Above, Place::Inside):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());              // I----A   I----A
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |***\|   |*/  |
      add(c.i, c.j + 1, p2, m_range.hi());          // |**I*|   |/ X/|
      if (cc != Place::Inside)                      // |\***|   |  /*|
        close();                                    // A----I   A----I
      add(c.i + 1, c.j, p3, m_range.hi());
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      add(c.i, c.j, p4, m_range.hi());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Below, Place::Inside):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());              // I----B   I----B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |***\|   |*/  |
      add(c.i, c.j + 1, p2, m_range.lo());          // |**I*|   |/ B/|
      if (cc != Place::Inside)                      // |\***|   |  /*|
        close();                                    // B----I   B----I
      add(c.i + 1, c.j, p3, m_range.lo());
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      add(c.i, c.j, p4, m_range.lo());
      close();
      break;
    }

    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Inside, Place::Above):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_hi);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, VertexType::Corner, c.p1);          // A----I
        add(c.i, c.j, p1, m_range.hi());                  // |/***|
        add(c.i, c.j + 1, p2, m_range.hi());              // |**I*|
        add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |***/|
        add(c.i + 1, c.j, p3, m_range.hi());              // I----A
        add(c.i, c.j, p4, m_range.hi());
        close();
      }
      else
      {
        add(c.i, c.j, VertexType::Corner, c.p1);  // A----I  top A could be H!
        add(c.i, c.j, p1, m_range.hi());          // |  \*|
        add(c.i, c.j, p4, m_range.hi());          // |\ A\|
        close();                                  // |*\  |
        // must be this order, possible corner touch!         // I----A
        add(c.i, c.j + 1, p2, m_range.hi());
        add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);
        add(c.i + 1, c.j, p3, m_range.hi());
        close();
      }
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Inside, Place::Below):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, VertexType::Corner, c.p1);          // B----I
        add(c.i, c.j, p1, m_range.lo());                  // | /**|
        add(c.i, c.j + 1, p2, m_range.lo());              // |/*I/|
        add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |**/ |
        add(c.i + 1, c.j, p3, m_range.lo());              // I----B
        add(c.i, c.j, p4, m_range.lo());
        close();
      }
      else
      {
        add(c.i, c.j, VertexType::Corner, c.p1);          // B----I
        add(c.i, c.j, p1, m_range.lo());                  // |  \*|
        add(c.i, c.j, p4, m_range.lo());                  // |\ B\|
        close();                                          // |*\  |
        add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // I----B
        add(c.i + 1, c.j, p3, m_range.lo());
        add(c.i, c.j + 1, p2, m_range.lo());
        close();
      }
      break;
    }

    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Below, Place::Above):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_right(c, VertexType::Vertical_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());              // I----B    I----B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |***\|    |*/  |
      add(c.i, c.j + 1, p2, m_range.lo());          // |\*I*|    |/ X/|  X = A or B
      if (cc != Place::Inside)                      // | \*/|    |  //|
        close();                                    // B----A    B----A
      add(c.i + 1, c.j, p3, m_range.lo());
      add(c.i + 1, c.j, p4, m_range.hi());
      add(c.i, c.j, p5, m_range.hi());
      add(c.i, c.j, p6, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Above, Place::Below):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_right(c, VertexType::Vertical_hi);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());              // I----A   I----A
      add(c.i, c.j + 1, VertexType::Corner, c.p2);  // |***\|   |*/  |
      add(c.i, c.j + 1, p2, m_range.hi());          // |**I*|   |/ X/| X = A or B
      if (cc != Place::Inside)                      // |\**/|   |  //|
        close();                                    // A----B   A----B
      add(c.i + 1, c.j, p3, m_range.hi());
      add(c.i + 1, c.j, p4, m_range.lo());
      add(c.i, c.j, p5, m_range.lo());
      add(c.i, c.j, p6, m_range.hi());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Below, Place::Inside):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p5 = intersect_right(c, VertexType::Vertical_lo);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_lo);
      add(c.i, c.j, p1, m_range.lo());      // A----B    A----B
      add(c.i, c.j, p2, m_range.hi());      // |/**\|    |//  |
      add(c.i, c.j + 1, p3, m_range.hi());  // |**I*|    |/ X/| X = A or B
      add(c.i, c.j + 1, p4, m_range.lo());  // |\***|    |  /*|
      if (cc != Place::Inside)              // B----I    B----I
        close();
      add(c.i + 1, c.j, p5, m_range.lo());
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      add(c.i, c.j, p6, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Above, Place::Inside):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p5 = intersect_right(c, VertexType::Vertical_hi);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi);
      add(c.i, c.j, p1, m_range.hi());      // B----A    B----A
      add(c.i, c.j, p2, m_range.lo());      // |/**\|    |//  |
      add(c.i, c.j + 1, p3, m_range.lo());  // |**I*|    |/ X/| X = A or B
      add(c.i, c.j + 1, p4, m_range.hi());  // |\***|    |  /*|
      if (cc != Place::Inside)              // A----I    A----I
        close();
      add(c.i + 1, c.j, p5, m_range.hi());
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      add(c.i, c.j, p6, m_range.hi());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Above, Place::Below):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      const auto p5 = intersect_right(c, VertexType::Vertical_lo);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_lo);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, VertexType::Corner, c.p1);  // B----A
        add(c.i, c.j, p1, m_range.lo());          // |/*\ |
        add(c.i, c.j + 1, p2, m_range.lo());      // |**I\|
        add(c.i, c.j + 1, p3, m_range.hi());      // |***/|
        add(c.i + 1, c.j, p4, m_range.hi());      // I----B
        add(c.i + 1, c.j, p5, m_range.lo());
        add(c.i, c.j, p6, m_range.lo());
        close();
      }
      else
      {
        add(c.i, c.j, VertexType::Corner, c.p1);  // B----A
        add(c.i, c.j, p1, m_range.lo());          // |  \\|
        add(c.i, c.j, p6, m_range.lo());          // |\ X\| X = A or B
        close();                                  // |*\  |
        add(c.i, c.j + 1, p2, m_range.lo());      // I----B
        add(c.i, c.j + 1, p3, m_range.hi());
        add(c.i + 1, c.j, p4, m_range.hi());
        add(c.i + 1, c.j, p5, m_range.lo());
        close();
      }
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Below, Place::Above):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo);
      const auto p5 = intersect_right(c, VertexType::Vertical_hi);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, VertexType::Corner, c.p1);  // A----B
        add(c.i, c.j, p1, m_range.hi());          // |/**\|
        add(c.i, c.j + 1, p2, m_range.hi());      // |**I*|
        add(c.i, c.j + 1, p3, m_range.lo());      // |***/|
        add(c.i + 1, c.j, p4, m_range.lo());      // I----A
        add(c.i + 1, c.j, p5, m_range.hi());
        add(c.i, c.j, p6, m_range.hi());
        close();
      }
      else
      {
        add(c.i, c.j, VertexType::Corner, c.p1);  // A----B
        add(c.i, c.j, p1, m_range.hi());          // |  \\|
        add(c.i, c.j, p6, m_range.hi());          // |\ X\| X = A or B
        close();                                  // |*\  |
        add(c.i, c.j + 1, p2, m_range.hi());      // I----A
        add(c.i, c.j + 1, p3, m_range.lo());
        add(c.i + 1, c.j, p4, m_range.lo());
        add(c.i + 1, c.j, p5, m_range.hi());
        close();
      }
      break;
    }
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Inside, Place::Below):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_right(c, VertexType::Vertical_lo);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, p1, m_range.hi());                  // B----I
        add(c.i, c.j, p2, m_range.lo());                  // |/***|
        add(c.i, c.j + 1, p3, m_range.lo());              // |**I*|
        add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |\**/|
        add(c.i + 1, c.j, p4, m_range.lo());              // A----B
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
        add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);
        add(c.i + 1, c.j, p4, m_range.lo());
        close();
      }
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Inside, Place::Above):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi);
      const auto p3 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_right(c, VertexType::Vertical_hi);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_lo);
      if (cc == Place::Inside)
      {
        add(c.i, c.j, p1, m_range.lo());                  // A----I
        add(c.i, c.j, p2, m_range.hi());                  // |/***|
        add(c.i, c.j + 1, p3, m_range.hi());              // |**I*|
        add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // |\**/|
        add(c.i + 1, c.j, p4, m_range.hi());              // B----A
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
        add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);
        add(c.i + 1, c.j, p4, m_range.hi());
        close();
      }
      break;
    }

    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Below, Place::Above):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_lo);
      const auto p2 = intersect_left(c, VertexType::Vertical_hi);
      const auto p3 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p4 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p5 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p6 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p7 = intersect_right(c, VertexType::Vertical_lo);
      const auto p8 = intersect_right(c, VertexType::Vertical_hi);
      if (cc == Place::Inside || (c.p2.z == m_range.hi() && c.p4.z == m_range.hi()))
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

    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Above, Place::Below):
    {
      const auto cc = center_place(c);
      const auto p1 = intersect_left(c, VertexType::Vertical_hi);
      const auto p2 = intersect_left(c, VertexType::Vertical_lo);
      const auto p3 = intersect_top(c, VertexType::Horizontal_lo);
      const auto p4 = intersect_top(c, VertexType::Horizontal_hi);
      const auto p5 = intersect_bottom(c, VertexType::Horizontal_lo);
      const auto p6 = intersect_bottom(c, VertexType::Horizontal_hi);
      const auto p7 = intersect_right(c, VertexType::Vertical_hi);
      const auto p8 = intersect_right(c, VertexType::Vertical_lo);
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
        add(c.i, c.j, p1, m_range.hi());                       // B----A
        add(c.i, c.j, p2, m_range.lo());                       // |//  |
        add(c.i, c.j + 1, p3, m_range.lo());                   // |/ A/|
        add(c.i, c.j + 1, p4, m_range.hi());                   // |  //|
        if (c.p1.z != m_range.hi() || c.p3.z != m_range.hi())  // A----B
          close();
        add(c.i + 1, c.j, p7, m_range.hi());
        add(c.i + 1, c.j, p8, m_range.lo());
        add(c.i, c.j, p5, m_range.lo());
        add(c.i, c.j, p6, m_range.hi());
        close();
      }
      break;
    }

    // Cases with exactly one corner missing
    case TRAX_RECT_HASH(Place::Invalid, Place::Above, Place::Above, Place::Above):
    case TRAX_RECT_HASH(Place::Invalid, Place::Below, Place::Below, Place::Below):
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Above, Place::Invalid):
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Below, Place::Invalid):
    case TRAX_RECT_HASH(Place::Above, Place::Invalid, Place::Above, Place::Above):
    case TRAX_RECT_HASH(Place::Below, Place::Invalid, Place::Below, Place::Below):
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Invalid, Place::Above):
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Invalid, Place::Below):
      break;

    case TRAX_RECT_HASH(Place::Invalid, Place::Above, Place::Above, Place::Below):
    case TRAX_RECT_HASH(Place::Invalid, Place::Above, Place::Above, Place::Inside):
    case TRAX_RECT_HASH(Place::Invalid, Place::Above, Place::Below, Place::Above):
    case TRAX_RECT_HASH(Place::Invalid, Place::Above, Place::Below, Place::Below):
    case TRAX_RECT_HASH(Place::Invalid, Place::Above, Place::Below, Place::Inside):
    case TRAX_RECT_HASH(Place::Invalid, Place::Above, Place::Inside, Place::Above):
    case TRAX_RECT_HASH(Place::Invalid, Place::Above, Place::Inside, Place::Below):
    case TRAX_RECT_HASH(Place::Invalid, Place::Above, Place::Inside, Place::Inside):
    case TRAX_RECT_HASH(Place::Invalid, Place::Below, Place::Above, Place::Above):
    case TRAX_RECT_HASH(Place::Invalid, Place::Below, Place::Above, Place::Below):
    case TRAX_RECT_HASH(Place::Invalid, Place::Below, Place::Above, Place::Inside):
    case TRAX_RECT_HASH(Place::Invalid, Place::Below, Place::Below, Place::Above):
    case TRAX_RECT_HASH(Place::Invalid, Place::Below, Place::Below, Place::Inside):
    case TRAX_RECT_HASH(Place::Invalid, Place::Below, Place::Inside, Place::Above):
    case TRAX_RECT_HASH(Place::Invalid, Place::Below, Place::Inside, Place::Below):
    case TRAX_RECT_HASH(Place::Invalid, Place::Below, Place::Inside, Place::Inside):
    case TRAX_RECT_HASH(Place::Invalid, Place::Inside, Place::Above, Place::Above):
    case TRAX_RECT_HASH(Place::Invalid, Place::Inside, Place::Above, Place::Below):
    case TRAX_RECT_HASH(Place::Invalid, Place::Inside, Place::Above, Place::Inside):
    case TRAX_RECT_HASH(Place::Invalid, Place::Inside, Place::Below, Place::Above):
    case TRAX_RECT_HASH(Place::Invalid, Place::Inside, Place::Below, Place::Below):
    case TRAX_RECT_HASH(Place::Invalid, Place::Inside, Place::Below, Place::Inside):
    case TRAX_RECT_HASH(Place::Invalid, Place::Inside, Place::Inside, Place::Above):
    case TRAX_RECT_HASH(Place::Invalid, Place::Inside, Place::Inside, Place::Below):
    case TRAX_RECT_HASH(Place::Invalid, Place::Inside, Place::Inside, Place::Inside):
    {
      // clang-format off
      build_edge(VertexType::Horizontal_lo, 1, 0,
                 c2, c.p2.z, c.i, c.j + 1, c.p2,
                 c3, c.p3.z, c.i + 1, c.j + 1, c.p3);
      build_edge(VertexType::Vertical_lo, 0, -1,
                 c3, c.p3.z, c.i + 1, c.j + 1, c.p3,
                 c4, c.p4.z, c.i + 1, c.j, c.p4);
      build_edge(VertexType::Diagonal_lo, -1, 0,
                 c4, c.p4.z, c.i + 1, c.j, c.p4,
                 c2, c.p2.z, c.i, c.j + 1, c.p2);
      // clang-format on
      close();
      break;
    }

    case TRAX_RECT_HASH(Place::Above, Place::Invalid, Place::Above, Place::Below):
    case TRAX_RECT_HASH(Place::Above, Place::Invalid, Place::Above, Place::Inside):
    case TRAX_RECT_HASH(Place::Above, Place::Invalid, Place::Below, Place::Above):
    case TRAX_RECT_HASH(Place::Above, Place::Invalid, Place::Below, Place::Below):
    case TRAX_RECT_HASH(Place::Above, Place::Invalid, Place::Below, Place::Inside):
    case TRAX_RECT_HASH(Place::Above, Place::Invalid, Place::Inside, Place::Above):
    case TRAX_RECT_HASH(Place::Above, Place::Invalid, Place::Inside, Place::Below):
    case TRAX_RECT_HASH(Place::Above, Place::Invalid, Place::Inside, Place::Inside):
    case TRAX_RECT_HASH(Place::Below, Place::Invalid, Place::Above, Place::Above):
    case TRAX_RECT_HASH(Place::Below, Place::Invalid, Place::Above, Place::Below):
    case TRAX_RECT_HASH(Place::Below, Place::Invalid, Place::Above, Place::Inside):
    case TRAX_RECT_HASH(Place::Below, Place::Invalid, Place::Below, Place::Above):
    case TRAX_RECT_HASH(Place::Below, Place::Invalid, Place::Below, Place::Inside):
    case TRAX_RECT_HASH(Place::Below, Place::Invalid, Place::Inside, Place::Above):
    case TRAX_RECT_HASH(Place::Below, Place::Invalid, Place::Inside, Place::Below):
    case TRAX_RECT_HASH(Place::Below, Place::Invalid, Place::Inside, Place::Inside):
    case TRAX_RECT_HASH(Place::Inside, Place::Invalid, Place::Above, Place::Above):
    case TRAX_RECT_HASH(Place::Inside, Place::Invalid, Place::Above, Place::Below):
    case TRAX_RECT_HASH(Place::Inside, Place::Invalid, Place::Above, Place::Inside):
    case TRAX_RECT_HASH(Place::Inside, Place::Invalid, Place::Below, Place::Above):
    case TRAX_RECT_HASH(Place::Inside, Place::Invalid, Place::Below, Place::Below):
    case TRAX_RECT_HASH(Place::Inside, Place::Invalid, Place::Below, Place::Inside):
    case TRAX_RECT_HASH(Place::Inside, Place::Invalid, Place::Inside, Place::Above):
    case TRAX_RECT_HASH(Place::Inside, Place::Invalid, Place::Inside, Place::Below):
    case TRAX_RECT_HASH(Place::Inside, Place::Invalid, Place::Inside, Place::Inside):
    {
      // clang-format off
      build_edge(VertexType::Diagonal_lo, 1, 1,
                 c1, c.p1.z, c.i, c.j, c.p1,
                 c3, c.p3.z, c.i + 1, c.j + 1, c.p3);
      build_edge(VertexType::Vertical_lo, 0, -1,
                 c3, c.p3.z, c.i + 1, c.j + 1, c.p3,
                 c4, c.p4.z, c.i + 1, c.j, c.p4);
      build_edge(VertexType::Horizontal_lo, -1, 0,
                 c4, c.p4.z, c.i + 1, c.j, c.p4,
                 c1, c.p1.z, c.i, c.j, c.p1);
      // clang-format on
      close();
      break;
    }

    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Invalid, Place::Below):
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Invalid, Place::Inside):
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Invalid, Place::Above):
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Invalid, Place::Below):
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Invalid, Place::Inside):
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Invalid, Place::Above):
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Invalid, Place::Below):
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Invalid, Place::Inside):
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Invalid, Place::Above):
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Invalid, Place::Below):
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Invalid, Place::Inside):
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Invalid, Place::Above):
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Invalid, Place::Inside):
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Invalid, Place::Above):
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Invalid, Place::Below):
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Invalid, Place::Inside):
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Invalid, Place::Above):
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Invalid, Place::Below):
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Invalid, Place::Inside):
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Invalid, Place::Above):
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Invalid, Place::Below):
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Invalid, Place::Inside):
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Invalid, Place::Above):
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Invalid, Place::Below):
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Invalid, Place::Inside):
    {
      // clang-format off
      build_edge(VertexType::Vertical_lo, 0, 1,
                 c1, c.p1.z, c.i, c.j, c.p1,
                 c2, c.p2.z, c.i, c.j + 1, c.p2);
      build_edge(VertexType::Diagonal_lo, 1, -1,
                 c2, c.p2.z, c.i, c.j + 1, c.p2,
                 c4, c.p4.z, c.i + 1, c.j, c.p4);
      build_edge(VertexType::Horizontal_lo, -1, 0,
                 c4, c.p4.z, c.i + 1, c.j, c.p4,
                 c1, c.p1.z, c.i, c.j, c.p1);
      // clang-format on
      close();
      break;
    }

    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Below, Place::Invalid):
    case TRAX_RECT_HASH(Place::Above, Place::Above, Place::Inside, Place::Invalid):
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Above, Place::Invalid):
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Below, Place::Invalid):
    case TRAX_RECT_HASH(Place::Above, Place::Below, Place::Inside, Place::Invalid):
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Above, Place::Invalid):
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Below, Place::Invalid):
    case TRAX_RECT_HASH(Place::Above, Place::Inside, Place::Inside, Place::Invalid):
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Above, Place::Invalid):
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Below, Place::Invalid):
    case TRAX_RECT_HASH(Place::Below, Place::Above, Place::Inside, Place::Invalid):
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Above, Place::Invalid):
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Inside, Place::Invalid):
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Above, Place::Invalid):
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Below, Place::Invalid):
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Inside, Place::Invalid):
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Above, Place::Invalid):
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Below, Place::Invalid):
    case TRAX_RECT_HASH(Place::Inside, Place::Above, Place::Inside, Place::Invalid):
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Above, Place::Invalid):
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Below, Place::Invalid):
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Inside, Place::Invalid):
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Above, Place::Invalid):
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Below, Place::Invalid):
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Invalid):
    {
      // clang-format off
      build_edge(VertexType::Vertical_lo, 0, 1,
                 c1, c.p1.z, c.i, c.j, c.p1,
                 c2, c.p2.z, c.i, c.j + 1, c.p2);
      build_edge(VertexType::Horizontal_lo, 1, 0,
                 c2, c.p2.z, c.i, c.j + 1, c.p2,
                 c3, c.p3.z, c.i + 1, c.j + 1, c.p3);
      build_edge(VertexType::Diagonal_lo, -1, -1,
                 c3, c.p3.z, c.i + 1, c.j + 1, c.p3,
                 c1, c.p1.z, c.i, c.j, c.p1);
      // clang-format on
      close();
      break;
    }

      // At least two corner values are NaN
    default:
      break;
  }
  finish_cell();
}

// Grid cells with one corner missing are contoured as triangles. Such cases
// have no ambiguities about saddle points, and can be processed one edge
// at a time. Since there are 4*27=108 such triangles, and grid cells with
// exactly one missing cell should be rare, we prefer this approach over
// the faster handling of fully valid rectangles above.
// Note that due to the merge algorithm, we must still process the left
// edge first. Hence reducing the size of the switch for triangles at the
// end of the above 'build_linear' method could be tricky.

void JointBuilder::build_edge(VertexType type,
                              int di,
                              int dj,
                              Place c1,
                              float z1,
                              int i1,
                              int j1,
                              const GridPoint& g1,
                              Place c2,
                              float z2,
                              int i2,
                              int j2,
                              const GridPoint& g2)
{
  // never add the second inside corner, it will be inserted by the next edge
  switch (place_hash(c1, c2))
  {
    case TRAX_EDGE_HASH(Place::Below, Place::Below):
    {
      break;
    }
    case TRAX_EDGE_HASH(Place::Below, Place::Inside):
    {
      if (z2 != m_range.lo())
      {
        const auto p = intersect(g1, g2, di, dj, lo(type), m_range.lo());
        add(std::min(i1, i2), std::min(j1, j2), p, m_range.lo());
      }
      break;
    }
    case TRAX_EDGE_HASH(Place::Below, Place::Above):
    {
      const auto p1 = intersect(g1, g2, di, dj, lo(type), m_range.lo());
      const auto p2 = intersect(g1, g2, di, dj, hi(type), m_range.hi());
      add(std::min(i1, i2), std::min(j1, j2), p1, m_range.lo());
      if (p2.type != VertexType::Corner)  // in case z2==range.hi()
        add(std::min(i1, i2), std::min(j1, j2), p2, m_range.hi());
      break;
    }
    case TRAX_EDGE_HASH(Place::Inside, Place::Below):
    {
      if (z1 != m_range.lo())
      {
        const auto p = intersect(g1, g2, di, dj, lo(type), m_range.lo());
        add(i1, j1, VertexType::Corner, g1);
        add(std::min(i1, i2), std::min(j1, j2), p, m_range.lo());
      }
      break;
    }
    case TRAX_EDGE_HASH(Place::Inside, Place::Inside):
    {
      add(i1, j1, VertexType::Corner, g1);
      break;
    }
    case TRAX_EDGE_HASH(Place::Inside, Place::Above):
    {
      const auto p = intersect(g1, g2, di, dj, hi(type), m_range.hi());
      add(i1, j1, VertexType::Corner, g1);
      if (p.type != VertexType::Corner)  // in case z2==range.hi()
        add(std::min(i1, i2), std::min(j1, j2), p, m_range.hi());
      break;
    }
    case TRAX_EDGE_HASH(Place::Above, Place::Below):
    {
      const auto p1 = intersect(g1, g2, di, dj, hi(type), m_range.hi());
      const auto p2 = intersect(g1, g2, di, dj, lo(type), m_range.lo());
      if (g1.z != m_range.hi())
        add(std::min(i1, i2), std::min(j1, j2), p1, m_range.hi());
      else
        add(i1, j1, p1, m_range.hi());
      add(std::min(i1, i2), std::min(j1, j2), p2, m_range.lo());
      break;
    }
    case TRAX_EDGE_HASH(Place::Above, Place::Inside):
    {
      const auto p = intersect(g1, g2, di, dj, hi(type), m_range.hi());
      if (g1.z != m_range.hi())
        add(std::min(i1, i2), std::min(j1, j2), p, m_range.hi());
      else
        add(i1, j1, p, m_range.hi());
      add(i2, j2, VertexType::Corner, g2);
      break;
    }
    case TRAX_EDGE_HASH(Place::Above, Place::Above):
    {
      if (g1.z == m_range.hi() && g2.z == m_range.hi())
        add(i1, j1, VertexType::Corner, g1);
      break;
    }
  }
}

// Isoband for missing values with linear interpolation must complement what is
// done in build_linear, namely handling triangles similarly. Hence we *cannot*
// use build_midpoint, since it will connect centers of edges instead of
// the corners like build_linear does.

void JointBuilder::build_missing(const Cell& c)
{
  auto hash = nan_hash(c.p1.z, c.p2.z, c.p3.z, c.p4.z);

  switch (hash)
  {
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Below, Place::Below):
      break;
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Below, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.p1, !m_isoline);          // B--B
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3, !m_isoline);  // | /|
      add(c.i + 1, c.j, VertexType::Corner, c.p4, true);            // B--I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Inside, Place::Below):
    {
      add(c.i, c.j + 1, VertexType::Corner, c.p2, !m_isoline);  // B--I
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3, true);    // | \|
      add(c.i + 1, c.j, VertexType::Corner, c.p4, !m_isoline);  // B--B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Below, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.p1, !m_isoline);          // I--B
      add(c.i, c.j + 1, VertexType::Corner, c.p2, true);            // |/ |
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3, !m_isoline);  // B--B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Below, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.p1, true);            // B--B
      add(c.i, c.j + 1, VertexType::Corner, c.p2, !m_isoline);  // |\ |
      add(c.i + 1, c.j, VertexType::Corner, c.p4, !m_isoline);  // I--B
      close();
      break;
    }
    default:
    {
      // at least two missing values, hence full cell is missing
      add(c.i, c.j, VertexType::Corner, c.p1, true);
      add(c.i, c.j + 1, VertexType::Corner, c.p2, true);
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3, true);
      add(c.i + 1, c.j, VertexType::Corner, c.p4, true);
      close();
      break;
    }
  }
  finish_cell();
}

void JointBuilder::build_midpoint(const Cell& c, double shell)
{
  // Abort if not contouring missing values and in the shell regions. Note that if there is no
  // shell, the value is NaN, and hence all comparisons will fail.
  if (!m_range.missing())
    if (c.p1.x == -shell || c.p1.y == -shell || c.p2.x == -shell || c.p2.y == +shell ||
        c.p3.x == +shell || c.p3.y == +shell || c.p4.x == +shell || c.p4.y == -shell)
      return;

  auto xa = (c.p4.x + c.p1.x) / 2;  // bottom center
  auto ya = (c.p4.y + c.p1.y) / 2;
  auto xb = (c.p1.x + c.p2.x) / 2;  // left center
  auto yb = (c.p1.y + c.p2.y) / 2;
  auto xc = (c.p2.x + c.p3.x) / 2;  // top center
  auto yc = (c.p2.y + c.p3.y) / 2;
  auto xd = (c.p3.x + c.p4.x) / 2;  // right center
  auto yd = (c.p3.y + c.p4.y) / 2;

  std::size_t hash = 0;
  if (m_range.missing())
    hash = nan_hash(c.p1.z, c.p2.z, c.p3.z, c.p4.z);
  else
  {
    const auto c1 = discrete_place(c.p1.z, m_range);
    const auto c2 = discrete_place(c.p2.z, m_range);
    const auto c3 = discrete_place(c.p3.z, m_range);
    const auto c4 = discrete_place(c.p4.z, m_range);
    hash = place_hash(c1, c2, c3, c4);
  }

  switch (hash)
  {
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Below, Place::Below):
    {
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Below, Place::Inside):
    {
      add(c.i + 1, c.j, VertexType::Corner, c.p4);                       // B--B
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());    // | /|
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());  // B--I
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Inside, Place::Below):
    {
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);                     // B--I
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());    // | \|
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // B--B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Below, Place::Inside, Place::Inside):
    {
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);                 // B--I
      add(c.i + 1, c.j, VertexType::Corner, c.p4);                     // | ||
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());  // B--I
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Below, Place::Below):
    {
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());        // I--B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);                         // |/ |
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // B--B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Below, Place::Inside):
    {
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());        // I--B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);                         // |//|
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // B--I
      close();
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Inside, Place::Below):
    {
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());  // I--I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);                   // |--|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);               // B--B
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Below, Place::Inside, Place::Inside, Place::Inside):
    {
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());  // I--I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);                   // |\ |
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);               // B--I
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Below, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.p1);                         // B--B
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());    // |\ |
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());  // I--B
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Below, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.p1);                           // B--B
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());      // |--|
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());  // I--I
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Inside, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.p1);                         // B--I
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());    // |\\|
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());  // I--B
      close();
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Below, Place::Inside, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.p1);                             // B--I
      add(c.i, c.j, VertexType::Vertical_lo, xb, yb, m_range.lo());        // |/ |
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // I--I
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Below, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.p1);                             // I--B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);                         // || |
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // I--B
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Below, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.p1);                             // I--B
      add(c.i, c.j + 1, VertexType::Corner, c.p2);                         // | \|
      add(c.i, c.j + 1, VertexType::Horizontal_lo, xc, yc, m_range.lo());  // I--I
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Below):
    {
      add(c.i, c.j, VertexType::Corner, c.p1);          // I--I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // | /|
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // I--B
      add(c.i + 1, c.j, VertexType::Vertical_lo, xd, yd, m_range.lo());
      add(c.i, c.j, VertexType::Horizontal_lo, xa, ya, m_range.lo());
      close();
      break;
    }
    case TRAX_RECT_HASH(Place::Inside, Place::Inside, Place::Inside, Place::Inside):
    {
      add(c.i, c.j, VertexType::Corner, c.p1);          // I--I
      add(c.i, c.j + 1, VertexType::Corner, c.p2);      // |  |
      add(c.i + 1, c.j + 1, VertexType::Corner, c.p3);  // I--I
      add(c.i + 1, c.j, VertexType::Corner, c.p4);
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

void isoline_linear(JointMerger& joints, const Cell& c, float limit)
{
  auto hilimit = (std::isnan(limit) ? limit : std::numeric_limits<float>::infinity());
  Range range(limit, hilimit);
  JointBuilder b(joints, range);
  b.set_isoline_mode();
  b.build_linear(c);
}

void isoband_logarithmic(JointMerger& joints, const Cell& c, const Range& range)
{
  JointBuilder b(joints, range);
  b.set_logarithmic_mode();
  b.build_linear(c);
}

void isoline_logarithmic(JointMerger& joints, const Cell& c, float limit)
{
  auto hilimit = (std::isnan(limit) ? limit : std::numeric_limits<float>::infinity());
  Range range(limit, hilimit);
  JointBuilder b(joints, range);
  b.set_isoline_mode();
  b.set_logarithmic_mode();
  b.build_linear(c);
}

void isoband_midpoint(JointMerger& joints, const Cell& c, const Range& range, double shell)
{
  JointBuilder b(joints, range);
  b.build_midpoint(c, shell);
}

}  // namespace CellBuilder

}  // namespace Trax
