#include "Polyline.h"
#include "Endian.h"
#include "Vertex.h"
#include <boost/math/constants/constants.hpp>
#include <boost/optional.hpp>
#include <fmt/format.h>
#include <smartmet/macgyver/Exception.h>
#include <algorithm>
#include <stdexcept>

using boost::math::double_constants::radian;

namespace Trax
{
namespace
{
void add_padding(Points& points, bool was_closed)
{
  if (was_closed)
    points.push_back(points[1]);  // A-B-C-...-X-A ==> A-B-C-...-X-A-B
}

void remove_padding(Points& points)
{
  // A closed polygon must be A-B-C-A, and since we added one to get A-B-C-A-B
  // and the size is less than that, a silved polygon was found which must be fully erased.

  auto n = points.size();
  if (n < 5)
  {
    points.clear();
    return;
  }

  // Now we can have due to our padding at the end either a padded polyline A-B-...-X-A-B
  // or one that has been modified

  if (points[1] == points.back())
  {
    points.pop_back();               // A-B-C-X-A-B ==> A-B-C-X-A
    if (points[0] != points.back())  // Was A deleted from the end?
    {
      points.erase(points.begin());      // B-C-X-A ==> B-C-X
      points.push_back(points.front());  //         ==> B-C-X-B
    }
  }
  else if (points.front() == points[n - 2])
  {
    points.pop_back();  // A-B-C-X-A-B ==> A-X-A-B ==> A-X-A
  }
  else
  {
    points.erase(points.begin());  // A-B-C-X-A-B ==> X-...-X
    points[0] = points.back();
  }
}

void remove_sliver(Points& points, bool& found_sliver, std::size_t pos, std::size_t& out_pos)
{
  // We avoid using std::sqrt/std::hypot for speed. 1e-7 is approx float precision
  const auto squared_mindistance = 1e-6 * 1e-6;

  const auto& a = points[out_pos - 2];
  const auto& b = points[out_pos - 1];
  const auto& c = points[pos];

  const auto ac = squared_distance(a, c);
  const auto ab = squared_distance(a, b);
  const auto bc = squared_distance(b, c);

  if (ac / ab <= squared_mindistance)
  {
    found_sliver = true;
    if (a < c)
    {
      --out_pos;  // a-b-c  ==> a
    }
    else
    {
      out_pos -= 2;
      points[out_pos++] = c;  // a-b-c ==> c
    }
  }
  else if (bc / ab <= squared_mindistance)
  {
    found_sliver = true;
    if (b < c)
    {
      // a-b-c ==> a-b
    }
    else
    {
      // a-b-c ==> a-c
      --out_pos;
      points[out_pos++] = c;
    }
  }

  else
  {
    if (found_sliver)
      points[out_pos] = points[pos];

    ++out_pos;
  }
}

}  // namespace

// Begin a new polyline
Polyline::Polyline(std::initializer_list<double> init_list)
{
  if (init_list.size() == 0 || init_list.size() % 2 != 0)
    throw Fmi::Exception(BCP, "Invalid initialization of Polyline from elements");
  const auto* iter = init_list.begin();
  while (iter != init_list.end())
  {
    double x = *iter++;
    double y = *iter++;
    m_points.emplace_back(x, y, false);
  }
  update_bbox();
}

// A polyline is closed if it contains at least 3 points and the end points are equal
bool Polyline::closed() const
{
  if (size() < 3)
    return false;
  return (xbegin() == xend() && ybegin() == yend());
}

// A polyline is clockwise if it is closed and the signed area is positive.
bool Polyline::clockwise() const
{
  if (!closed())
    return false;

  double area = 0;
  auto n = size();
  for (std::size_t i = 0; i < n - 1; i++)
    area += (m_points[i + 1].x - m_points[i].x) * (m_points[i].y + m_points[i + 1].y);

  // The true area is |area/2|, but we care only about the orientation

  // Treating zero-size polygons as anti-clockwise lead to troubled code elsewhere,
  // hence we use equality as well.
  return area >= 0;
}

std::vector<double> Polyline::xcoordinates() const
{
  std::vector<double> ret;
  const auto n = m_points.size();
  ret.reserve(n);
  for (auto i = 0UL; i < n; i++)
    ret.push_back(m_points[i].x);
  return ret;
}

std::vector<double> Polyline::ycoordinates() const
{
  std::vector<double> ret;
  const auto n = m_points.size();
  ret.reserve(n);
  for (auto i = 0UL; i < n; i++)
    ret.push_back(m_points[i].y);
  return ret;
}

// Polygon exteriors and holes may share only singular vertices and not
// whole edges. Hence any point along any edge should be inside an exterior
// if the hole is inside it.

std::pair<double, double> Polyline::inside_point() const
{
  const auto x = 0.5 * (m_points[0].x + m_points[1].x);
  const auto y = 0.5 * (m_points[0].y + m_points[1].y);
  return std::make_pair(x, y);
}

// Polyline exit angle
double Polyline::end_angle() const
{
  if (m_points.size() < 2)
    throw Fmi::Exception(BCP, "Cannot calculate polyline end angle when size < 2");

  const auto n = m_points.size() - 2;

  auto x1 = m_points[n].x;
  auto y1 = m_points[n].y;
  auto x2 = m_points[n + 1].x;
  auto y2 = m_points[n + 1].y;
  return atan2(y2 - y1, x2 - x1) * radian;
}

// Polyline start angle
double Polyline::start_angle() const
{
  if (m_points.size() < 2)
    throw Fmi::Exception(BCP, "Cannot calculate polyline start angle when size < 2");

  auto x1 = m_points[0].x;
  auto y1 = m_points[0].y;
  auto x2 = m_points[1].x;
  auto y2 = m_points[1].y;
  return atan2(y2 - y1, x2 - x1) * radian;
}

bool Polyline::bbox_contains(const Polyline& other) const
{
  return bbox().contains(other.bbox());
}

// Does the (closed) polyline contain the other (closed) polyline
bool Polyline::contains(const Polyline& other) const
{
  // Quick exit based on bounding box containment
  if (!bbox_contains(other))
  {
    return false;
  }

  // A hole may touch the exterior at a vertex, but may not share
  // an edge with it. Hence choosing the middle of a vertex as a test
  // point is safe provided there are no rounding errors, huge values
  // causing problems, highly distorted grids or something similar
  // making the calculations not robust.

  const auto test_point = other.inside_point();
  const auto x = test_point.first;
  const auto y = test_point.second;

  // Refs 1-2 are for clarity since ref 3 only provides an idea for optimization by Stuart MacMartin
  //
  // 1: http://www.faqs.org/faqs/graphics/algorithms-faq/ question 2.03
  // 2: https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html
  // 3: http://www.realtimerendering.com/resources/RTNews/html//rtnv5n3.html#art3

  const auto n = m_points.size();
  bool inside = false;
  for (std::size_t i = 1; i < n; i++)
  {
    // Skip continuously while below or above y
    if (m_points[i - 1].y < y)
    {
      while (i < n && m_points[i].y < y)
        i++;
    }
    else if (m_points[i - 1].y > y)
    {
      while (i < n && m_points[i].y > y)
        i++;
    }

    if (i >= n)
      break;  // reached end without crossing y

    auto x1 = m_points[i - 1].x;
    auto y1 = m_points[i - 1].y;
    auto x2 = m_points[i].x;
    auto y2 = m_points[i].y;
    if ((y1 > y) != (y2 > y))
      if (x < (x1 - x2) * (y - y2) / (y1 - y2) + x2)
        inside = !inside;
  }

  return inside;
}

// Append a new coordinate. Discard duplicates which appear from apthological cases
// such as an isoband range value appearing only in one corner of a grid cell.
void Polyline::append(const Vertex& vertex)
{
  if (m_points.empty())
  {
    m_points.reserve(256);  // do not settle for a small initial allocation
    m_points.emplace_back(vertex.x, vertex.y, vertex.ghost);
  }
  else if (m_points.back().x != vertex.x || m_points.back().y != vertex.y)  // ignore duplicates
    m_points.emplace_back(vertex.x, vertex.y, vertex.ghost);
}

// Append another polyline directly
void Polyline::append(const Polyline& other)
{
  m_points.insert(m_points.end(), ++other.m_points.begin(), other.m_points.end());
}

// Reverse the winding order
void Polyline::reverse()
{
  const auto n = size();
  for (auto i = 0UL, j = n - 1; i < j; i++, j--)
    std::swap(m_points[i], m_points[j]);
}

// Export normal WKT
std::string Polyline::wkt() const
{
  std::string ret = "LINESTRING ";
  ret += wkt_body();
  return ret;
}

// Export WKT body only for use in multilinestrings etc
std::string Polyline::wkt_body() const
{
  std::string ret = "(";

  const auto n = size();
  for (auto i = 0UL; i < n; i++)
  {
    if (i > 0)
      ret += ',';
    ret += fmt::format("{} {}", m_points[i].x, m_points[i].y);
  }
  ret += ')';
  return ret;
}

void Polyline::wkb(std::ostringstream& out) const
{
  out.put(byteorder());
  uint type = 2;  // Line
  out.write((const char*)&type, sizeof(type));
  wkb_body(out);
}

void Polyline::wkb_body(std::ostringstream& out) const
{
  uint n = size();
  out.write((const char*)&n, sizeof(n));

  for (uint i = 0; i < n; i++)
  {
    double x = m_points[i].x;
    double y = m_points[i].y;

    // printf("Point %f,%f\n",x,y);
    out.write((const char*)&x, 8);
    out.write((const char*)&y, 8);
  }
}

// Normalize coordinates to lexicographic order for testing purposes
Polyline& Polyline::normalize()
{
  if (!closed())
  {
    const auto n = size() - 1;
    // Lexicographically smallest end vertex first
    if (m_points[0].x > m_points[n].x ||
        (m_points[0].x == m_points[n].x && m_points[0].y > m_points[n].y))
      reverse();
    return *this;
  }

  // Find lexicographically smallest element

  auto n = size() - 1;  // minus one since the last vertex is the same as the first one
  auto best = 0UL;
  for (auto i = 1UL; i < n; i++)
  {
    if ((m_points[i].x < m_points[best].x) ||
        (m_points[i].x == m_points[best].x && m_points[i].y < m_points[best].y))
      best = i;
  }

  // Rotate until it is the first element
  if (best != 0)
  {
    auto pos = m_points.begin();
    std::advance(pos, best);
    std::rotate(m_points.begin(), pos, --m_points.end());

    // Fix closing vertex
    m_points[n] = m_points[0];
  }
  return *this;
}

// For normalizing collections of polylines
bool Polyline::operator<(const Polyline& other) const
{
  if (this == &other)
    return false;

  const auto n = std::min(size(), other.size());
  for (auto i = 0UL; i < n; i++)
  {
    if (m_points[i].x != other.m_points[i].x)
      return m_points[i].x < other.m_points[i].x;
    if (m_points[i].y != other.m_points[i].y)
      return m_points[i].y < other.m_points[i].y;
  }
  return false;
}

void Polyline::update_bbox()
{
  m_bbox.init(m_points);
}

bool Polyline::has_ghosts() const
{
  return std::any_of(m_points.begin(), m_points.end(), [](const Point& p) { return p.ghost; });
}

struct ValidRange
{
  std::size_t begin;
  std::size_t end;
};

boost::optional<ValidRange> find_valid_range(const Points& points, std::size_t startpos)
{
  const auto n = points.size();
  while (startpos < n && points[startpos].ghost)
    ++startpos;
  if (startpos >= n)
    return {};

  auto endpos = startpos;
  while (endpos < n && !points[endpos].ghost)
    ++endpos;

  return ValidRange{startpos, endpos};
}

// precondition: has_ghosts is true, closed is true
void Polyline::remove_ghosts(Polylines& new_polylines)
{
#if 0
  std::cout << "Removing ghosts from " << wkt() << "\n";
  for (auto i = 0UL; i < m_points.size(); i++)
    std::cout << fmt::format(
        "\t{} : {},{} {}\n", i, m_points[i].x, m_points[i].y, m_points[i].ghost ? "?" : "-");
#endif

  // Extract valid ranges
  std::vector<ValidRange> ranges;
  const auto n = m_points.size();
  auto startpos = 0UL;
  while (startpos < n)
  {
    auto range = find_valid_range(m_points, startpos);
    if (!range)
      break;
    if (range->end - range->begin > 1)
      ranges.emplace_back(*range);
    startpos = range->end;
  }

  // Detect whether there is a wraparound
  const bool wraparound =
      (ranges.size() > 1 && ranges.front().begin == 0 && ranges.back().end == n);

  auto sz = ranges.size();
  if (wraparound)  // if there is wraparound, the last range is handled along with the first one
    --sz;

  for (auto i = 0UL; i < sz; i++)
  {
    Polyline line;
    const auto& range = ranges[i];
    if (i == 0 && wraparound)
    {
      const auto& range2 = ranges.back();
      // std::cout << "Keeping wraparound range " << range2.begin << "..." << range2.end << "\n";
      for (auto j = range2.begin; j < range2.end - 1; j++)
        line.m_points.push_back(m_points[j]);
    }
    // std::cout << "Keeping  range " << range.begin << "..." << range.end << "\n";
    for (auto j = range.begin; j < range.end; j++)
      line.m_points.push_back(m_points[j]);
    new_polylines.emplace_back(std::move(line));
  }
}

// Remove slivers, return true if no vertices remain (should be extremely rare)
bool Polyline::desliver()
{
  if (empty())
    return true;

  // Look for a sequence of points p1-p2-p3 where p1 and p3 are extremely close. When found, keep
  // only p1 or p3 based on lexicographic sort of coordinates so that the selection does not depend
  // on the order of the vertices, and hence exteriors and holes will be handled identically. This
  // is relevant since the exterior of any isoband is likely to be a hole hole in another isoband.
  //
  // If we have a sequence a-b-c-d where c is a sliver point, we may end up with a-b or a-d
  // Since the number of points can only decrease, we to sliver removal in place, and just shrink
  // the vector at the end if necessary.

  bool found_sliver = false;
  auto out_pos = 2UL;

  const bool was_closed = closed();

  // To simplify handling if the polyline is closed we duplicate the start of the polyline at the
  // end and then just fix things if something was removed. This is easier than doing modular
  // indexing or having more elaborate special code after the main loop

#if 0
  for (auto i = 0UL; i < m_points.size(); i++)
    std::cout << fmt::format("{}\t{}\t{}\n", i, m_points[i].x, m_points[i].y);
#endif

  add_padding(m_points, was_closed);  // A-B-X-A ==> A-B-X-A-B

  auto n = m_points.size();

  for (auto pos = 2UL; pos < n; ++pos)
    remove_sliver(m_points, found_sliver, pos, out_pos);

  // We never increase the size but the compiler does not know it and hence
  // requires a default constructor for Point. We avoid the error by providing
  // a useless dummy point for resize(n,point);

  if (found_sliver)
    m_points.resize(out_pos, Point(0, 0, false));

  if (!was_closed)
    return empty();

  remove_padding(m_points);

#if 0
  std::cout << "\nAfter:\n";
  for (auto i = 0UL; i < m_points.size(); i++)
    std::cout << fmt::format("{}\t{}\t{}\n", i, m_points[i].x, m_points[i].y);
#endif

  if (m_points.empty())
    return true;

  // Necessary to keep the geometry valid for clipping or we may run into an eternal loop
  if (!closed())
    throw Fmi::Exception(BCP, "Failed to close polygon properly");

  // In case there was only a sliver to not leave a single vertex
  if (m_points.size() < 2)
    m_points.clear();

  return empty();
}

}  // namespace Trax
