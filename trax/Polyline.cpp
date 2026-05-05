#include "Polyline.h"
#include "Endian.h"
#include "Vertex.h"
#include <boost/math/constants/constants.hpp>
#include <fmt/format.h>
#include <smartmet/macgyver/Exception.h>
#include <algorithm>
#include <optional>

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

// Iterate every unique vertex of a segmented polyline in order.
// Skips the leading point of every non-leading segment (which duplicates the
// previous segment's tail).
template <typename F>
void for_each_point(const std::vector<Points>& segments, F&& f)
{
  for (std::size_t i = 0; i < segments.size(); ++i)
  {
    const auto& seg = segments[i];
    const std::size_t start = (i == 0) ? 0 : 1;
    for (std::size_t j = start; j < seg.size(); ++j)
      f(seg[j]);
  }
}

// Iterate every consecutive (a,b) point pair across the whole segmented polyline.
template <typename F>
void for_each_pair(const std::vector<Points>& segments, F&& f)
{
  bool have_prev = false;
  Point prev{0, 0, false};
  for_each_point(segments,
                 [&](const Point& p)
                 {
                   if (have_prev)
                     f(prev, p);
                   prev = p;
                   have_prev = true;
                 });
}

}  // namespace

bool Polyline::empty() const
{
  return m_segments.empty() || m_segments.front().empty();
}

std::size_t Polyline::size() const
{
  if (m_segments.empty())
    return 0;
  std::size_t total = 0;
  for (const auto& seg : m_segments)
    total += seg.size();
  // Subtract the duplicate leading point shared by each non-leading segment.
  total -= (m_segments.size() - 1);
  return total;
}

void Polyline::flatten()
{
  if (m_segments.size() <= 1)
    return;

  Points flat;
  flat.reserve(size());
  for_each_point(m_segments, [&](const Point& p) { flat.push_back(p); });

  m_segments.clear();
  m_segments.push_back(std::move(flat));
}

// Begin a new polyline
Polyline::Polyline(std::initializer_list<double> init_list)
{
  if (init_list.size() == 0 || init_list.size() % 2 != 0)
    throw Fmi::Exception(BCP, "Invalid initialization of Polyline from elements");
  m_segments.emplace_back();
  auto& seg = m_segments.back();
  const auto* iter = init_list.begin();
  while (iter != init_list.end())
  {
    double x = *iter++;
    double y = *iter++;
    seg.emplace_back(x, y, false);
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
  for_each_pair(m_segments,
                [&](const Point& a, const Point& b) { area += (b.x - a.x) * (a.y + b.y); });

  // The true area is |area/2|, but we care only about the orientation
  // Treating zero-size polygons as anti-clockwise lead to troubled code elsewhere,
  // hence we use equality as well.
  return area >= 0;
}

std::vector<double> Polyline::xcoordinates() const
{
  std::vector<double> ret;
  ret.reserve(size());
  for_each_point(m_segments, [&](const Point& p) { ret.push_back(p.x); });
  return ret;
}

std::vector<double> Polyline::ycoordinates() const
{
  std::vector<double> ret;
  ret.reserve(size());
  for_each_point(m_segments, [&](const Point& p) { ret.push_back(p.y); });
  return ret;
}

// Polygon exteriors and holes may share only singular vertices and not
// whole edges. Hence any point along any edge should be inside an exterior
// if the hole is inside it.
std::pair<double, double> Polyline::inside_point() const
{
  const auto& seg = m_segments.front();
  const auto x = 0.5 * (seg[0].x + seg[1].x);
  const auto y = 0.5 * (seg[0].y + seg[1].y);
  return std::make_pair(x, y);
}

// Polyline exit angle (between the last two unique points).
double Polyline::end_angle() const
{
  if (size() < 2)
    throw Fmi::Exception(BCP, "Cannot calculate polyline end angle when size < 2");

  // The last two unique points: take the last two of the last segment if it has
  // size >= 2, otherwise the previous segment's tail and this segment's tail.
  const auto& last = m_segments.back();
  Point p1{0, 0, false};
  Point p2{0, 0, false};
  if (last.size() >= 2)
  {
    p1 = last[last.size() - 2];
    p2 = last.back();
  }
  else
  {
    // last segment has size 1 (just the duplicate). Reach into the previous segment.
    const auto& prev = m_segments[m_segments.size() - 2];
    p1 = prev.back();
    p2 = last.back();
    // p1 == p2 in this degenerate case, but the caller's contract assumes
    // non-degenerate input — the above is a guard.
  }
  return atan2(p2.y - p1.y, p2.x - p1.x) * radian;
}

// Polyline start angle (between the first two unique points).
double Polyline::start_angle() const
{
  if (size() < 2)
    throw Fmi::Exception(BCP, "Cannot calculate polyline start angle when size < 2");

  const auto& first = m_segments.front();
  if (first.size() >= 2)
  {
    auto x1 = first[0].x;
    auto y1 = first[0].y;
    auto x2 = first[1].x;
    auto y2 = first[1].y;
    return atan2(y2 - y1, x2 - x1) * radian;
  }
  // first segment size 1 — use the next segment's [1] (or [0] if that's the dup).
  const auto& next = m_segments[1];
  auto x1 = first[0].x;
  auto y1 = first[0].y;
  // next[0] == first[0] by the duplicate-vertex invariant, so use next[1] if present.
  auto x2 = next.size() >= 2 ? next[1].x : next[0].x;
  auto y2 = next.size() >= 2 ? next[1].y : next[0].y;
  return atan2(y2 - y1, x2 - x1) * radian;
}

bool Polyline::bbox_contains(const Polyline& other) const
{
  return bbox().contains(other.bbox());
}

// Does the (closed) polyline contain the other (closed) polyline.
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

  // pnpoly via consecutive-pair iteration. We cannot easily use the skip-runs
  // optimization across segment boundaries, but each segment is contiguous so
  // the inner loop still benefits from spatial coherence.

  bool inside = false;
  for_each_pair(m_segments,
                [&](const Point& a, const Point& b)
                {
                  if ((a.y > y) != (b.y > y))
                    if (x < (a.x - b.x) * (y - b.y) / (a.y - b.y) + b.x)
                      inside = !inside;
                });
  return inside;
}

// Append a new coordinate. Discard duplicates which appear from apthological cases
// such as an isoband range value appearing only in one corner of a grid cell.
void Polyline::append(const Vertex& vertex)
{
  if (m_segments.empty())
  {
    m_segments.emplace_back();
    m_segments.back().reserve(256);  // do not settle for a small initial allocation
    m_segments.back().emplace_back(vertex.x, vertex.y, vertex.ghost);
    return;
  }

  auto& last = m_segments.back();
  if (last.empty() || last.back().x != vertex.x || last.back().y != vertex.y)
    last.emplace_back(vertex.x, vertex.y, vertex.ghost);
}

// Append another polyline by COPY. Used when the caller cannot relinquish
// ownership of the source. Keeps the segmented invariant by appending segments.
void Polyline::append(const Polyline& other)
{
  if (other.m_segments.empty())
    return;

  // The first segment of `other` starts with the join vertex (= our last vertex).
  // We push it as-is; the segmented iteration helpers will skip its leading dup.
  for (const auto& seg : other.m_segments)
    m_segments.push_back(seg);
}

// Append another polyline by MOVE. The hot-path overload: ownership of each
// source segment's heap allocation transfers into m_segments, so no point data
// is memcopied. The cost reduces to growing the outer vector, which doubles
// geometrically and is amortized O(K) over K joins.
void Polyline::append(Polyline&& other)
{
  if (other.m_segments.empty())
    return;

  m_segments.reserve(m_segments.size() + other.m_segments.size());
  for (auto& seg : other.m_segments)
    m_segments.push_back(std::move(seg));
  other.m_segments.clear();
}

// Reverse the winding order. Reverses each segment internally and reverses
// the order of segments. The duplicate-at-boundary invariant is preserved.
void Polyline::reverse()
{
  for (auto& seg : m_segments)
    std::reverse(seg.begin(), seg.end());
  std::reverse(m_segments.begin(), m_segments.end());
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
  bool first = true;
  for_each_point(m_segments,
                 [&](const Point& p)
                 {
                   if (!first)
                     ret += ',';
                   ret += fmt::format("{} {}", p.x, p.y);
                   first = false;
                 });
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

  for_each_point(m_segments,
                 [&](const Point& p)
                 {
                   double x = p.x;
                   double y = p.y;
                   out.write((const char*)&x, 8);
                   out.write((const char*)&y, 8);
                 });
}

// Normalize coordinates to lexicographic order for testing purposes.
// Test/debug-only path; flatten first to keep the implementation simple.
Polyline& Polyline::normalize()
{
  flatten();
  if (m_segments.empty() || m_segments[0].empty())
    return *this;

  auto& points = m_segments[0];

  if (!closed())
  {
    const auto n = points.size() - 1;
    // Lexicographically smallest end vertex first
    if (points[0].x > points[n].x || (points[0].x == points[n].x && points[0].y > points[n].y))
      reverse();
    return *this;
  }

  // Find lexicographically smallest element

  auto n = points.size() - 1;  // minus one since the last vertex is the same as the first one
  auto best = 0UL;
  for (auto i = 1UL; i < n; i++)
  {
    if ((points[i].x < points[best].x) ||
        (points[i].x == points[best].x && points[i].y < points[best].y))
      best = i;
  }

  // Rotate until it is the first element
  if (best != 0)
  {
    auto pos = points.begin();
    std::advance(pos, best);
    std::rotate(points.begin(), pos, --points.end());

    // Fix closing vertex
    points[n] = points[0];
  }
  return *this;
}

// For normalizing collections of polylines
bool Polyline::operator<(const Polyline& other) const
{
  if (this == &other)
    return false;

  // Lexicographic compare across segmented storage. Because both sides skip
  // duplicate join vertices, we use for_each_point and pull pairs in lockstep.

  // Materialize on demand. operator< is used for normalize/sort in tests; not
  // a hot path, so flattening on the fly is acceptable.
  std::vector<Point> a;
  a.reserve(size());
  for_each_point(m_segments, [&](const Point& p) { a.push_back(p); });

  std::vector<Point> b;
  b.reserve(other.size());
  for_each_point(other.m_segments, [&](const Point& p) { b.push_back(p); });

  const auto n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; i++)
  {
    if (a[i].x != b[i].x)
      return a[i].x < b[i].x;
    if (a[i].y != b[i].y)
      return a[i].y < b[i].y;
  }
  return false;
}

void Polyline::update_bbox()
{
  // BBox::init takes a Points reference. Build a flat view if needed.
  if (m_segments.size() == 1)
  {
    m_bbox.init(m_segments[0]);
    return;
  }
  Points flat;
  flat.reserve(size());
  for_each_point(m_segments, [&](const Point& p) { flat.push_back(p); });
  m_bbox.init(flat);
}

bool Polyline::has_ghosts() const
{
  for (const auto& seg : m_segments)
    if (std::any_of(seg.begin(), seg.end(), [](const Point& p) { return p.ghost; }))
      return true;
  return false;
}

struct ValidRange
{
  std::size_t begin;
  std::size_t end;
};

namespace
{
std::optional<ValidRange> find_valid_range(const Points& points, std::size_t startpos)
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
}  // namespace

// precondition: has_ghosts is true, closed is true
void Polyline::remove_ghosts(Polylines& new_polylines)
{
  // The existing logic is index-based and operates on a single contiguous
  // Points buffer; flatten first to reuse it unchanged.
  flatten();
  if (m_segments.empty())
    return;

  const auto& points = m_segments[0];

  // Extract valid ranges
  std::vector<ValidRange> ranges;
  const auto n = points.size();
  auto startpos = 0UL;
  while (startpos < n)
  {
    auto range = find_valid_range(points, startpos);
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
    line.m_segments.emplace_back();
    auto& line_points = line.m_segments.back();
    const auto& range = ranges[i];
    if (i == 0 && wraparound)
    {
      const auto& range2 = ranges.back();
      for (auto j = range2.begin; j < range2.end - 1; j++)
        line_points.push_back(points[j]);
    }
    for (auto j = range.begin; j < range.end; j++)
      line_points.push_back(points[j]);
    new_polylines.emplace_back(std::move(line));
  }
}

// Remove slivers, return true if no vertices remain (should be extremely rare).
// Existing logic is in-place index arithmetic; flatten first then run unchanged.
bool Polyline::desliver()
{
  if (empty())
    return true;

  flatten();
  auto& points = m_segments[0];

  bool found_sliver = false;
  auto out_pos = 2UL;

  const bool was_closed = closed();

  // To simplify handling if the polyline is closed we duplicate the start of the polyline at the
  // end and then just fix things if something was removed.

  add_padding(points, was_closed);  // A-B-X-A ==> A-B-X-A-B

  auto n = points.size();

  for (auto pos = 2UL; pos < n; ++pos)
    remove_sliver(points, found_sliver, pos, out_pos);

  // We never increase the size but the compiler does not know it and hence
  // requires a default constructor for Point. We avoid the error by providing
  // a useless dummy point for resize(n,point);

  if (found_sliver)
    points.resize(out_pos, Point(0, 0, false));

  if (!was_closed)
    return empty();

  remove_padding(points);

  if (points.empty())
  {
    m_segments.clear();
    return true;
  }

  // Necessary to keep the geometry valid for clipping or we may run into an eternal loop
  if (!closed())
    throw Fmi::Exception(BCP, "Failed to close polygon properly");

  // In case there was only a sliver to not leave a single vertex
  if (points.size() < 2)
  {
    points.clear();
    m_segments.clear();
  }

  return empty();
}

}  // namespace Trax
