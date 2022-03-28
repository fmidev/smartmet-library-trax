#include "Topology.h"
#include "Joint.h"
#include "JointPool.h"
#include "SmallVector.h"
#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/math/constants/constants.hpp>
#include <fmt/format.h>
#include <smartmet/macgyver/Exception.h>

#if 0
#include <iostream>
#endif

namespace Trax
{
// Geometry types for r-tree searches
namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;
using bg_point = bg::model::point<double, 2, bg::cs::cartesian>;
using bg_box = bg::model::box<bg_point>;
using bg_value = std::pair<bg_box, Polygons::iterator>;
using bg_rtree = bgi::rtree<bg_value, bgi::rstar<16>>;  // R* variant

// We may sort polylines base on the head coordinate or the tail coordinate. Head sort
// is for appending to a growing polyline, tail for prepending. Appending is sufficient
// for isobands since the rings always close. Prepending may be needed to grow isolines
// to their maximum possible length.

struct JoinCoordinate
{
  double x;
  double y;

  bool operator<(const JoinCoordinate& other) const
  {
    if (x != other.x)
      return (x < other.x);
    return (y < other.y);
  }
};

using Duplicates = SmallVector<Joint*, 8UL>;

using SortedPolylines = std::multimap<JoinCoordinate, Polyline>;

using JoinCandidate = SortedPolylines::iterator;
using JoinCandidates = std::list<JoinCandidate>;

namespace
{
double turn_angle(double angle1, double angle2)
{
  // max turn left > -180 and max turn right < 180
  auto turn = fmod(angle1 - angle2, 360.0);
  if (turn < -180)
    turn += 360;
  else if (turn > 180)
    turn -= 360;
  return turn;
}

inline bool has_duplicates(Joint* j)
{
  return (j->alt != nullptr);
}

// Find joints with the same vertex (including the joint itself)
Duplicates get_duplicates(Joint* j)
{
  Duplicates ret;
  ret.push_back(j);

  auto* i = j;
  while ((i = i->alt) != j)  // push until reached j again
    ret.push_back(i);

  return ret;
}

JoinCandidates find_append_candidates(const Polyline& polyline, SortedPolylines& polylines)
{
  JoinCandidates result;

  const auto x = polyline.xend();
  const auto y = polyline.yend();

  auto it = polylines.find(JoinCoordinate{x, y});

  for (auto end = polylines.end(); it != end; ++it)
  {
    const auto& coord = it->first;
    if (x == coord.x && y == coord.y)
      result.push_back(it);
    else
      break;
  }
  return result;
}

// Assumes !polylines.empty()
JoinCandidate select_turn(const Polyline& polyline, JoinCandidates& candidates, bool turn_right)
{
  using boost::math::double_constants::radian;

  // std::list::size is slow: if (candidates.size() == 1)
  if (++candidates.begin() == candidates.end())
    return candidates.front();

  // Tail angle of the polyline to append a candidate to
  const auto end_angle = polyline.end_angle();

  // Find best candidate
  auto best_candidate = candidates.front();
  double best_turn = (turn_right ? -999.0 : 999.0);  // anything above 180 will do

#if 0
  std::cout << "Polyline : " << polyline.wkt() << " angle = " << end_angle << "\n";
#endif

  for (auto& candidate : candidates)
  {
    const auto start_angle = candidate->second.start_angle();

#if 0
    std::cout << "\tCandidate angle " << start_angle << " : " << candidate->second.wkt() << "\n";
#endif

    // max turn left > -180 and max turn right < 180
    auto turn = fmod(end_angle - start_angle, 360.0);
    if (turn < -180)
      turn += 360;
    else if (turn > 180)
      turn -= 360;

#if 0
    std::cout << "\tTurn angle = " << turn << "\n";
#endif

    bool better_turn = (turn_right ? turn > best_turn : turn < best_turn);

    if (better_turn)
    {
      best_candidate = candidate;
      best_turn = turn;
    }
  }

  return best_candidate;
}

Joint* select_right_turn(const Vertex& vertex,
                         const Polylines& polylines,
                         const Duplicates& duplicates)
{
  // Select free joint with the rightmost turn

  using boost::math::double_constants::radian;
  const auto angle1 = polylines.back().end_angle();

  Joint* best = nullptr;
  double best_turn = -999;  // possible range is -180...180

  for (auto* j : duplicates)
  {
    if (!j->used)  // Ignore used joints, we only needed them for the num_joints test
    {
      const auto& next_vertex = j->next->vertex;
      const auto angle2 = atan2(next_vertex.y - vertex.y, next_vertex.x - vertex.x) * radian;
      const auto turn = turn_angle(angle1, angle2);
      if (turn > best_turn)
      {
        best = j;
        best_turn = turn;
      }
    }
  }

  if (best_turn == -999)
    throw Fmi::Exception(BCP, fmt::format("All joints already used at {},{}", vertex.x, vertex.y));

  return best;
}

Polylines extract_right_turning_sequence(Joint* joint, bool strict)
{
  Polylines polylines;  // polylines extracted by taking right turns split at double joints
  Polyline polyline;    // grow a polyline until a double joint is encountered

#if 0
  std::cout << "RIGHT\n";
#endif

  while (true)
  {
    const auto& vertex = joint->vertex;

#if 0
    std::cout << fmt::format("\t{},{}\n", vertex.x, vertex.y);
#endif

    polyline.append(vertex);

    // Stop when exterior (and holes attached to it) becomes closed
    if (polylines.empty() && polyline.closed())
    {
      polylines.emplace_back(std::move(polyline));
      return polylines;
    }
    if (!polylines.empty() && polylines.front().xbegin() == polyline.xend() &&
        polylines.front().ybegin() == polyline.yend())
    {
      polylines.emplace_back(std::move(polyline));
      return polylines;
    }

    // Choose rightmost turn. If there are multiple connections, start a new polyline
    // so that we can choose the leftmost turns later on to separate holes touching
    // the exerior.

    if (!has_duplicates(joint))
    {
      // Trivial case of only one possible continuation
      if (joint->used)
      {
        if (polyline.size() == 1)  // collapsed to a corner point, just discard it
          return polylines;
        if (!strict)
        {
          // Ignore the built polyline and try to continue. This problem can happen
          // near the poles when projected coordinates may be inaccurate.
          polylines.clear();
          return polylines;
        }

        throw Fmi::Exception(BCP,
                             fmt::format("Only joint already used at {},{}", vertex.x, vertex.y))
            .addParameter("Column", fmt::format("{}", vertex.column))
            .addParameter("Row", fmt::format("{}", vertex.row))
            .addParameter("Type", to_string(vertex.type));
      }
      joint->used = true;
      joint = joint->next;
    }
    else
    {
      // Start new polyline so we can extract holes later on unless the it contains
      // current vertex only
      if (polyline.size() > 1)
      {
        polylines.emplace_back(std::move(polyline));
        polyline = Polyline{vertex.x, vertex.y};
      }

      // Note that we do not remove the best joint from the sequence of alternatives
      // since eventually we'd lose information on which vertices contain alternatives
      // and hence would not know when to interrupt the right turning sequence. Instead,
      // Joints::duplicates will return used joints too.

      auto duplicates = get_duplicates(joint);
      auto* best = select_right_turn(vertex, polylines, duplicates);
      best->used = true;
      joint = best->next;
    }
  }
}

// Extract a polyline by always taking a left turn
Polyline extract_left_turning_polyline(SortedPolylines& polylines)
{
  // Start from the first available polyline
  auto it = polylines.begin();
  Polyline result = std::move(it->second);
  polylines.erase(it);

  const bool turn_right = false;

  // Note: The first selection may already be a closed hole
  while (!result.closed())
  {
    auto candidates = find_append_candidates(result, polylines);
    if (candidates.empty())
      return result;

    auto candidate = select_turn(result, candidates, turn_right);

    result.append(candidate->second);
    polylines.erase(candidate);
  }
  return result;
}

void extract_left_turning_sequence(Polylines& polylines, Polylines& shells, Holes& holes)
{
  // Quick exit if there is only one polyline
  if (polylines.size() == 1)
  {
    polylines.front().update_bbox();
    if (polylines.front().clockwise())
      shells.emplace_back(std::move(polylines.front()));
    else
      holes.emplace_back(std::move(polylines.front()));
    return;
  }

  // Create a search structure for finding the left turns quickly
  SortedPolylines sorted_polylines;
  for (auto&& polyline : polylines)
  {
    JoinCoordinate xy{polyline.xbegin(), polyline.ybegin()};
    sorted_polylines.insert(std::make_pair(xy, polyline));
  }
  polylines.clear();

  Polylines new_shells;
  Holes new_holes;

  while (!sorted_polylines.empty())
  {
    auto polyline = extract_left_turning_polyline(sorted_polylines);

#if 0
    std::cout << "Left turning polyline: " << polyline.wkt() << "\n";
#endif

    polyline.update_bbox();
    if (polyline.clockwise())
      shells.emplace_back(std::move(polyline));
    else
      holes.emplace_back(std::move(polyline));
  }
}

bg_box make_bg_box(const BBox& bbox)
{
  return bg_box(bg_point(bbox.xmin(), bbox.ymin()), bg_point(bbox.xmax(), bbox.ymax()));
}

bg_rtree build_rtree(Polygons& polygons)
{
  bg_rtree rtree;

  for (auto it = polygons.begin(), end = polygons.end(); it != end; ++it)
    rtree.insert(std::make_pair(make_bg_box(it->exterior().bbox()), it));

  return rtree;
}

std::list<Polygons::iterator> possible_rtree_shells(const bg_rtree& rtree, const Polyline& hole)
{
  auto query_box = make_bg_box(hole.bbox());
  std::vector<bg_value> results;
  rtree.query(bgi::contains(query_box), std::back_inserter(results));

  // Accept only those that contain the hole bbox (should be true for all elements)
  std::list<Polygons::iterator> ret;

  for (const auto& result : results)
    if (result.second->bbox_contains(hole))
      ret.push_back(result.second);

  return ret;
}

std::list<Polygons::iterator> possible_shells(const bg_rtree& rtree, const Polyline& hole)
{
  auto ret = possible_rtree_shells(rtree, hole);

  // We assume all holes have an exterior to avoid an expensive contains() test

#if 0
  if (ret.size() < 2)  // std::list::size is slow
    return ret;
#else
  if (ret.empty())
    return ret;
  if (++ret.begin() == ret.end())
    return ret;
#endif

  // Find ones that actually contain the point

  for (auto it = ret.begin(), end = ret.end(); it != end;)
  {
    if ((*it)->contains(hole))
      ++it;
    else
      it = ret.erase(it);
  }

  return ret;
}

Polygons::iterator innermost_polygon(const std::list<Polygons::iterator>& polygons)
{
  // For valid touching rule polygons the innermost polygon must necessarily have
  // a bbox that is contained by the bbox of all the other available choices since
  // polygons may touch only at a single vertex.

  auto ret = polygons.front();  // first candidate

  for (const auto& it : polygons)
    if (ret != it && ret->exterior().bbox().contains(it->exterior().bbox()))
      ret = it;

  return ret;
}

}  // namespace

/*
 * Build rings by
 *   1. picking the next unused edge from the joints
 *   2. selecting right turn at all branch points
 *   3. until the path closes
 *   4. iterate through the selected vertices but take left turns
 *   5. result will be one clockwise exterior and 0-N holes directly attached
 *      to the exterior or via a chain of holes
 * The process is repeated until all edges have been processed, and the
 * output is polygons and holes which were not in contact with any exterior.
 */

void build_rings(Polylines& shells, Holes& holes, JointPool& joints, bool strict)
{
#if 0
  std::cout << "Joint map at start:\n" << Trax::to_string(joints);
#endif

  for (auto* joint : joints)
  {
    // Skip fully used joints
    if (joint->used)
      continue;

#if 0
    const auto& vertex = joint->vertex;
    std::cout << fmt::format("Start joint:\n\t{},{} --> {}:{},{}\n",
                             vertex.x,
                             vertex.y,
                             joint->used,
                             joint->next->vertex.x,
                             joint->next->vertex.y);
#endif

    // Skip unused multijoints as start points since then we cannot calculate the polyline end angle
    if (!has_duplicates(joint))
    {
      auto polylines = extract_right_turning_sequence(joint, strict);

      if (!polylines.empty())  // the sequence may have been discarded due to a problem
      {
#if 0
      std::cout << "Extracted right turning sequence:\n";
      for (const auto& polyline : polylines)
        std::cout << fmt::format("\t{}\n", polyline.wkt());

      std::cout << "Joint map now:\n" << Trax::to_string(joints);
#endif

        // Now extract a left turning sequence to separate holes touching the exterior
        extract_left_turning_sequence(polylines, shells, holes);
      }
    }
    else
    {
      // std::cout << "Skipping duplicate start point. Duplicates: " << num_joints << "\n";
    }
  }
}

/*
 * Assign holes to polygons utilizing bbox information for speed.
 * If a hole is in multiple polygons, we must choose the innermost
 * polygon to obey topology rules for polygons. Note that in isoline
 * mode there may be unassigned holes if the exterior is cut at the
 * grid borders.
 */

void build_polygons(Polygons& polygons, Polylines& shells, Holes& holes)
{
#if 0
  int counter = 0;
  std::cout << fmt::format(
      "{} : Assigning {} holes to {} polygons\n", ++counter, holes.size(), polygons.size());

  auto i = 0UL;
  for (const auto& poly : polygons)
    std::cout << "Poly " << i++ << " " << poly.wkt()
              << "\n\tbbox = " << poly.exterior().bbox().wkt() << "\n";
  i = 0UL;
  for (const auto& hole : holes)
    std::cout << "Hole " << i++ << " " << hole.wkt() << "\n\tbbox = " << hole.bbox().wkt() << "\n";
  counter = 0;
#endif

  for (auto&& shell : shells)
  {
    if (shell.size() >= 4)  // discard too small shells
      polygons.emplace_back(shell);
  }

  if (holes.empty())
    return;

  // TODO: Should we optimize in case there is just one polygon?

  // Build an R-tree of the polygon exterior bounding boxes for speed

  auto rtree = build_rtree(polygons);

  for (auto it = holes.begin(); it != holes.end();)
  {
    auto& hole = *it;  // shorthand variable

    if (hole.size() < 4)  // discard too small holes
      continue;

    auto candidates = possible_shells(rtree, hole);
#if 0
    std::cout << fmt::format("\t{} candidates for hole {}\n", candidates.size(), counter++);
#endif
    if (candidates.empty())
    {
      ++it;  // should be isolated hole when calculating isolines
    }
    else
    {
      auto polygon = innermost_polygon(candidates);
#if 0
      std::cout << fmt::format("\tInnermost polygon: {}\n", polygon->wkt());
#endif
      polygon->hole(hole);
      it = holes.erase(it);
    }
  }

  // Convert any remaining holes to shells. This can happen when an exterior shell has
  // been stripped of ghost lines when calculating isolines.
  for (auto&& hole : holes)
  {
    hole.reverse();
    polygons.emplace_back(std::move(hole));
  }
}

}  // namespace Trax
