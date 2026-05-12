#include "Topology.h"
#include "Joint.h"
#include "JointPool.h"
#include "SmallVector.h"
#include <boost/math/constants/constants.hpp>
#include <fmt/format.h>
#include <smartmet/macgyver/Exception.h>
#include <algorithm>
#include <map>

#if 0
#include <iostream>
#endif

namespace Trax
{

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
using JoinCandidates = SmallVector<JoinCandidate, 8UL>;

namespace
{
void add_exception_details(Fmi::Exception& ex,
                           bool verbose,
                           const Polylines& polylines,
                           const Polyline& polyline)
{
  if (verbose)
  {
    auto i = 0UL;
    for (const auto& line : polylines)
      ex.addParameter(fmt::format("Line {}", ++i).c_str(), line.wkt());
    ex.addParameter("Current line", polyline.wkt());
  }
}

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

// Find unused joints with the same vertex (including the joint itself)
Duplicates get_duplicates(Joint* j)
{
  Duplicates ret;
  ret.push_back(j);

  auto* i = j;
  while ((i = i->alt) != j)  // push until reached j again
  {
    if (!i->used)
      ret.push_back(i);
  }

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
  if (candidates.size() == 1)
    return candidates[0];

  // Tail angle of the polyline to append a candidate to
  const auto end_angle = polyline.end_angle();

  // Find best candidate
  auto best_candidate = candidates[0];
  double best_turn = (turn_right ? -999.0 : 999.0);  // anything above 180 will do

#if 0
  std::cout << "Polyline : " << polyline.wkt() << " angle = " << end_angle << "\n";
#endif

  for (const auto& candidate : candidates)
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
  // Safety check
  if (duplicates.empty())
    throw Fmi::Exception(BCP, "No remaining alternative path for selecting a right turn");

  // Avoid angle calculations if there is only one candidate. Removing this may also be
  // unsafe since rings are built in two phases, skipped_joints in the second loop.

  if (duplicates.size() == 1)
    return duplicates[0];

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

bool finish_right_turning_sequence(Polylines& polylines, Polyline& polyline)
{
  if (polylines.empty() && polyline.closed())
  {
    polylines.emplace_back(std::move(polyline));
    return true;
  }
  if (!polylines.empty() && polylines.front().xbegin() == polyline.xend() &&
      polylines.front().ybegin() == polyline.yend())
  {
    polylines.emplace_back(std::move(polyline));
    return true;
  }
  return false;
}

Polylines extract_right_turning_sequence(Joint* joint, bool strict, bool verbose)
{
  Polylines polylines;  // polylines extracted by taking right turns split at double joints
  Polyline polyline;    // grow a polyline until a double joint is encountered
  try
  {
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
      if (finish_right_turning_sequence(polylines, polyline))
        return polylines;

      // Choose rightmost turn. If there are multiple connections, start a new polyline
      // so that we can choose the leftmost turns later on to separate holes touching
      // the exterior. Note that if we have only one vertex so far, we do not need to
      // care about possible duplicates, since we can choose any continuation since
      // there is no need to take the rightmost turn.

      if (!has_duplicates(joint) || polyline.size() == 1)
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
        // Start new polyline so we can extract holes later on unless it contains the
        // current vertex only
        if (polyline.size() > 1)
        {
          polylines.emplace_back(std::move(polyline));
          polyline = Polyline(vertex);
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
  catch (...)
  {
    Fmi::Exception ex(BCP, "Failed to extract right turning sequence", nullptr);
    add_exception_details(ex, verbose, polylines, polyline);
    throw ex;
  }
}

// Extract a polyline by always taking a left turn
Polyline extract_left_turning_polyline(SortedPolylines& polylines)
{
  try
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

      result.append(std::move(candidate->second));
      polylines.erase(candidate);
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract left turning polyline");
  }
}

void extract_left_turning_sequence(Polylines& polylines, Polylines& shells, Holes& holes)
{
  try
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
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract left turning sequence");
  }
}

double bbox_area(const BBox& b)
{
  return (b.xmax() - b.xmin()) * (b.ymax() - b.ymin());
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
  const bool verbose = (joints.size() < 256);
  try
  {
#if 0
    std::cout << "Joint map at start of building rings:\n" << Trax::to_string(joints);
#endif

    // Some joints are bad first candidates since we cannot decide which path turns most to the left
    // if there is no prior point.
    std::list<Joint*> skipped_joints;

    for (auto* joint : joints)
    {
      // Skip fully used joints
      if (joint->used)
        continue;

      // If the next vertex has duplicates, we cannot calculate the angle from which to turn most
      // to the right. On the other hand, if the first vertex has duplicates, we are free to
      // choose any continuation we want, no need to select the rightmost turn.

      if (has_duplicates(joint->next))
        skipped_joints.push_back(joint);
      else
      {
        auto polylines = extract_right_turning_sequence(joint, strict, verbose);

        if (!polylines.empty())  // the sequence may have been discarded due to a problem
          extract_left_turning_sequence(polylines, shells, holes);  // separate holes touching shell
      }
    }

    // Make sure skipped joints were processed
    for (auto* joint : skipped_joints)
    {
      if (!joint->used)
      {
        auto polylines = extract_right_turning_sequence(joint, strict, verbose);
        if (!polylines.empty())
          extract_left_turning_sequence(polylines, shells, holes);
      }
    }
  }
  catch (...)
  {
    Fmi::Exception ex(BCP, "Failed to build rings", nullptr);
    ex.addParameter("Number of joints", std::to_string(joints.size()));
    if (verbose)
    {
      ex.addParameter("Joints", "\n" + to_string(joints, true));
      auto i = 0UL;
      for (const auto& shell : shells)
        ex.addParameter(fmt::format("Shell {}", ++i).c_str(), shell.wkt());
      i = 0UL;
      for (const auto& hole : holes)
        ex.addParameter(fmt::format("Hole {}", ++i).c_str(), hole.wkt());
    }
    throw ex;
  }
}

/*
 * Assign holes to shells via closure order.
 *
 * Insight: when an exterior shell closes (its topmost row has been swept),
 * every hole it contains must already be closed (a hole's vertices lie
 * inside the shell's, so the hole's max_row <= the shell's max_row). And
 * any *other* exterior that also contains the hole must enclose this
 * just-closed exterior too — that outer one is bigger and is still pending.
 * So the first geometric fit at shell-close time is the innermost
 * containing shell: no bbox-area sort, no rtree.
 *
 * Algorithm: tag each ring with (max_row, orientation, bbox_area), sort,
 * sweep once. Holes go into a pending pool; each closing shell drains the
 * pool of holes it geometrically contains. The pool stays small in
 * realistic workloads (its size at any moment is roughly the number of
 * features in flight at that row, not the global hole count).
 *
 * In isoline mode an exterior may be cut at the grid borders and the
 * surviving fragments are CCW; such holes end up unparented in the pool
 * and are emitted as reversed-orientation shells, matching the old code.
 */

namespace
{
struct RingRef
{
  Polyline* ring;     // points into the source Polylines/Holes list (std::list, iterator-stable)
  int max_row;        // close-order key
  double bbox_area;   // tiebreaker
  bool is_shell;      // true = CW exterior, false = CCW hole
};
}  // namespace

void build_polygons(Polygons& polygons, Polylines& shells, Holes& holes)
{
  try
  {
    // Collect all sufficiently large rings, computing the sort keys once.
    std::vector<RingRef> rings;
    rings.reserve(shells.size() + holes.size());

    for (auto& shell : shells)
      if (shell.size() >= 4)
        rings.push_back({&shell, shell.max_row(), bbox_area(shell.bbox()), true});
    for (auto& hole : holes)
      if (hole.size() >= 4)
        rings.push_back({&hole, hole.max_row(), bbox_area(hole.bbox()), false});

    // Sort by (max_row asc, holes before shells at same row, bbox_area asc).
    // Holes-before-shells at the same row guarantees a hole that closes on the
    // same row as its parent is in the pool when the parent drains. Smaller
    // bbox first among shells gives inside-out shell processing at ties.
    std::sort(rings.begin(),
              rings.end(),
              [](const RingRef& a, const RingRef& b)
              {
                if (a.max_row != b.max_row)
                  return a.max_row < b.max_row;
                if (a.is_shell != b.is_shell)
                  return !a.is_shell;  // hole (false) before shell (true)
                return a.bbox_area < b.bbox_area;
              });

    // Pending unassigned holes. Order within the pool does not affect
    // correctness (first geometric fit wins regardless), so swap-and-pop on
    // erase keeps removal O(1).
    std::vector<Polyline*> pool;

    for (const auto& ref : rings)
    {
      if (!ref.is_shell)
      {
        pool.push_back(ref.ring);
        continue;
      }

      // Construct the polygon (this consumes the shell ring).
      Polygon polygon(std::move(*ref.ring));

      if (!pool.empty())
      {
        const auto& shell_bbox = polygon.exterior().bbox();
        for (std::size_t i = 0; i < pool.size();)
        {
          auto* hole = pool[i];
          if (shell_bbox.contains(hole->bbox()) && polygon.exterior().contains(*hole))
          {
            polygon.hole(std::move(*hole));
            pool[i] = pool.back();
            pool.pop_back();
          }
          else
          {
            ++i;
          }
        }
      }

      polygons.emplace_back(std::move(polygon));
    }

    // Leftover holes are unparented (can happen for isolines where the
    // exterior was cut at the grid boundary). Emit each as a reversed-
    // orientation shell, matching the previous implementation's fallback.
    for (auto* hole : pool)
    {
      hole->reverse();
      polygons.emplace_back(std::move(*hole));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to build polygons");
  }
}

}  // namespace Trax
