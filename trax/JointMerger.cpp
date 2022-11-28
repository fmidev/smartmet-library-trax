#include "JointMerger.h"
#include "Cell.h"
#include "Joint.h"
#include <stdexcept>

#if 0
#include <fmt/format.h>
#include <iostream>
#endif

namespace Trax
{
namespace
{
// Find match for joint from the end of the current row
Joint* find_match(const Vertex& vertex,
                  const JointPool::iterator& begin,
                  const JointPool::iterator& end)
{
  auto it = end;

  while (it != begin)
  {
    --it;
    auto* j = *it;

    if (j->vertex == vertex && !j->used)
      return j;
  }
  return nullptr;
}

// Find match for joint from the previous row near hinted position in range
JointPool::iterator find_match(const Vertex& vertex,
                               const JointPool::iterator& hint,
                               const JointPool::iterator& begin,
                               const JointPool::iterator& end)
{
  // Search forward first
  for (auto it = hint; it != end; ++it)
  {
    auto* j = *it;
    if (j->vertex == vertex && !j->used)
      return it;
    if (j->vertex.column > vertex.column + 1)
      break;
  }

  // Search backward next
  for (auto it = hint; it != begin;)
  {
    --it;
    auto* j = *it;
    if (j->vertex == vertex && !j->used)
      return it;
    if (j->vertex.column + 1 < vertex.column)
      break;
  }

  return end;
}

void remove_alt(Joint* joint)
{
  if (joint->alt == nullptr)
    return;

  // Find element pointing to joint

  auto* j = joint;
  while (j->alt != joint)
  {
    j = j->alt;
  }

  // Remove joint from the middle
  j->alt = joint->alt;
  joint->alt = nullptr;

  // Check if we're down to one last joint
  if (j->alt == j)
    j->alt = nullptr;  // no alternatives left
}

// See if two joints belong to the same path of alternatives. This is true
// if the the match is found in the ring for the given joint.
bool is_alt_merged(const Joint* match, Joint* joint)
{
  auto* j = joint;
  while (j->alt != match)  // terminate search if the match is found
  {
    j = j->alt;
    if (j == joint)  // terminate search when the ring has been traversed
      return false;
  }
  return true;
}

// Find an edge to cancel if any
Joint* find_cancellation(Joint* joint, Joint* match)
{
  const auto& vertex = joint->next->vertex;

  // Quick handling of an immediate match
  if (vertex == match->prev->vertex)
    return match;

  // Search other alternates if there are any
  if (match->alt == nullptr)
    return nullptr;

  // search circular ring for alternative whose prev vertex matches the next vertex for joint
  auto* j = match;
  while ((j = j->alt) != match)
    if (vertex == j->prev->vertex)
      return j;

  return nullptr;
}

// Merge alternatives
void add_alt(Joint* match, Joint* joint)
{
  // std::cout << "\tadd alt match=" << match << " joint=" << joint << "\n";
  // Add alternative path for match
  if (match->alt == nullptr && joint->alt == nullptr)
  {
    // std::cout << "Alt 1\n";
    joint->alt = match;  // start new chain
    match->alt = joint;
  }
  else if (match->alt == nullptr)
  {
    // std::cout << "Alt 2\n";
    match->alt = joint->alt;  // insert match to chain joint is in
    joint->alt = match;
  }
  else if (joint->alt == nullptr)
  {
    // std::cout << "Alt 3\n";
    joint->alt = match->alt;  // insert joint to chain match is in
    match->alt = joint;
  }
  else if (!is_alt_merged(match, joint))
  {
    // std::cout << "Alt 4\n";
    std::swap(joint->alt, match->alt);
  }
  else
  {
    // std::cout << "No alt added\n";
  }
}

void cancel_edge(Joint* joint, Joint* j)
{
  // std::cout << "\t\tcanceling\n";
  // Cancel the edge
  auto* prev1 = joint->prev;
  auto* next1 = joint->next;
  auto* prev2 = j->prev;
  prev1->next = j;
  prev2->next = next1->next;
  next1->next->prev = j->prev;  // note the order of these 2 rows
  j->prev = joint->prev;
  next1->used = true;
  joint->used = true;

  // Update chain of alternatives to skip deleted joints
  // std::cout << "Removing alt from " << joint << "\n";
  // std::cout << "Removing alt from " << next1 << "\n";
  remove_alt(joint);
  remove_alt(next1);
}

// Match is the previously inserted one, joint the one to be merged
void merge_joint(Joint* joint, Joint* match)
{
  // Find out if there is a match for the edge to the next vertex in the duplicates
  // listed for the match which could be cancelled. If not, we must add a new duplicate.

  Joint* j = find_cancellation(joint, match);
  if (j == nullptr)
  {
#if 0
    std::cout << "Adding ALT\n";
#endif
    add_alt(match, joint);
  }
  else
  {
#if 0
    std::cout << "Cancelling edge\n";
#endif
    cancel_edge(joint, j);
  }
}

}  // namespace

JointMerger::JointMerger(JointMerger&& other) noexcept
    : m_pool(std::move(other.m_pool)),
      m_row_start(other.m_row_start),
      m_row_end(other.m_row_end),
      m_cell_merge_end(other.m_cell_merge_end),
      m_maxrow(other.m_maxrow)

{
}

// Merge vertices from a grid cell to the end of the row. Note that we do not
// create joints if there is an edge to be cancelled, since creating Joints to
// memory buffers is quite slow compared to the other parts of contouring.
// Note that CellBuilder guarantees that the possible edge to be cancelled
// is defined by the first two vertices. If there are no matches for both,
// we proceed normally and possibly even create one duplicate vertex if
// just one vertex matches.

void JointMerger::merge_cell(const Vertices& vertices)
{
  if (vertices.size() <= 2)  // safety check
    return;

  // Find matches for the first two vertices
  Joint* j1 = nullptr;
  Joint* j2 = nullptr;

  for (auto it = m_last_cell_start, end = m_last_cell_end; it != end; ++it)
  {
    auto* j = *it;
    if (j->vertex == vertices[0])
      j1 = j;
    else if (j->vertex == vertices[1])
      j2 = j;
  }

#if 0
  if (m_last_cell_start != m_last_cell_end)
  {
    std::cout << "Merging cell vertices\n";
    for (auto i = 0UL; i < vertices.size(); i++)
      std::cout << fmt::format("{},{} --> ", vertices[i].x, vertices[i].y);
    std::cout << "\n";
  }
#endif

  if (j1 != nullptr && j2 != nullptr)
  {
#if 0
    std::cout << "\tDirect merge: j1=" << j1 << " j2=" << j2 << "\n";
#endif

    // Merge directly by cancelling the common edge between j1 and j2

    Joint* prev_joint = j2;
    for (auto i = 2UL; i < vertices.size(); i++)
    {
      auto* j = m_pool.create(vertices[i]);
      prev_joint->next = j;
      j->prev = prev_joint;
      prev_joint = j;
    }
    prev_joint->next = j1;
    j1->prev = prev_joint;

    m_cell_merge_end = m_pool.end();
  }
  else
  {
#if 0
    std::cout << "\tIndirect merge\n";
#endif
    // Create all vertices and link them. Mark one as a duplicate if there was a match
    Joint* first_joint = m_pool.create(vertices[0]);
    Joint* prev_joint = first_joint;

    for (auto i = 1UL; i < vertices.size(); i++)
    {
      auto* j = m_pool.create(vertices[i]);
      prev_joint->next = j;  // link consecutive vertices
      j->prev = prev_joint;
      prev_joint = j;
    }
    prev_joint->next = first_joint;  // wrap around
    first_joint->prev = prev_joint;

    // Unoptimized slow merge code
    merge_cell();
  }
}

// Merge elements to earlier ones on the same row. New vertices may only
// merge with the adjacent cell earlier on the same row.
void JointMerger::merge_cell()
{
  bool empty_merge = (m_last_cell_start == m_cell_merge_end);
#if 0
  std::cout << fmt::format("Joints before cell merge:\n {}\n", to_string(m_pool));
#endif

  if (!empty_merge)
  {
    for (auto it = m_cell_merge_end, end = m_pool.end(); it != end; ++it)
    {
      auto* joint = *it;
      auto& vertex = joint->vertex;

      // Only corners and verticals at the left edge can merge
      if (!joint->used && !is_horizontal(vertex.type))
      {
#if 0
        std::cout << fmt::format("\tSearching for {},{} from range {}..{}\n",
                                 vertex.x,
                                 vertex.y,
                                 reinterpret_cast<void*>(*m_last_cell_start),
                                 reinterpret_cast<void*>(*m_cell_merge_end));
#endif
        auto* vertex_match = find_match(vertex, m_last_cell_start, m_cell_merge_end);
        if (vertex_match != nullptr)
        {
          merge_joint(joint, vertex_match);
#if 0
          std::cout << "      after joint " << vertex.x << "," << vertex.y << ":\n"
                    << to_string(m_pool) << "\n";
#endif
        }
      }
    }
  }

  m_cell_merge_end = m_pool.end();

#if 0
  std::cout << "Joints now:\n" << to_string(m_pool) << "\n";
#endif
}

void JointMerger::finish_cell()
{
  // Update range for last cell on this row
  m_last_cell_start = m_last_cell_end;
  m_last_cell_end = m_pool.end();
  m_cell_merge_end = m_last_cell_end;
#if 0
  std::cout << "Cell finished:\n"
            << "\tlast_cell_start = " << *m_last_cell_start << "\n"
            << "\tlast_cell_end = " << *m_last_cell_end << "\n\n";
#endif
}

// Merge new row of elements to the previous one. The assumption is that only
// vertices on the shared horizontal border may merge, and that adjacent cells
// are very likely to occur on both rows, and can be skipped quickly when not.
// In fact, when there is a match, it is very likely that there is a whole
// sequence of matches that can be handled immediately without resorting to
// separate find calls for the vertices.

void JointMerger::merge_row()
{
  // no need to search for matches if the previous row was empty
  const bool prev_row_empty = (m_row_start == m_row_end);

#if 0
  if (!prev_row_empty)
  {
    std::cout << "\n\n\n\nJoints before row merge:\n" << to_string(m_pool) << "\n";
    std::cout << fmt::format("\trange {}..{}\n",
                             reinterpret_cast<void*>(*m_row_start),
                             reinterpret_cast<void*>(*m_row_end));
  }
#endif

  // Only vertices with row == m_maxrow (current value!) have a chance of being merged
  // Only corner and horizontal vertices can be on the bottom row, verticals can be discarded

  auto new_maxrow = m_maxrow;  // calculate new maxrow from the merge (usually just +1)

  auto hint = m_row_start;  // we expect matches to begin at the start of the rows

  // The row to merge starts right after the previous row ending at m_row_end
  for (auto it = m_row_end, end = m_pool.end(); it != end; ++it)
  {
    auto* joint = *it;
    auto& vertex = joint->vertex;

    new_maxrow = std::max(new_maxrow, vertex.row);

    if (!prev_row_empty && !joint->used && vertex.row == m_maxrow && !is_vertical(vertex.type))
    {
#if 0
      std::cout << fmt::format("\tSearching for {},{} from range {}..{}\n",
                               vertex.x,
                               vertex.y,
                               reinterpret_cast<void*>(*m_row_start),
                               reinterpret_cast<void*>(*m_row_end));
#endif
      auto match_iter = find_match(vertex, hint, m_row_start, m_row_end);
      if (match_iter != m_row_end)
      {
        merge_joint(joint, *match_iter);
        hint = match_iter;
#if 0
        std::cout << "Joints after joint merge:\n" << to_string(m_pool) << "\n";
#endif
      }
    }
#if 0
    else if (joint->used)
      std::cout << "\tSkipping used joint\n";
    else if (vertex.row != m_maxrow)
      std::cout << fmt::format("\tSkipping {},{} since vertex row {} != m_maxrow {}\n",
                               vertex.x,
                               vertex.y,
                               vertex.row,
                               m_maxrow);
    else if (is_vertical(vertex.type))
      std::cout << fmt::format(
          "\tSkipping {},{} since vertex type is vertical\n", vertex.x, vertex.y);
#endif
  }

  // New finished row
  m_row_start = m_row_end;
  m_row_end = m_pool.end();

  m_maxrow = new_maxrow;

  // No cell yet on the next row to be processed
  m_last_cell_start = m_pool.end();
  m_last_cell_end = m_pool.end();
  m_cell_merge_end = m_pool.end();

#if 0
  std::cout << "Joints after row merge:\n" << to_string(m_pool) << "\n";
  std::cout << "Finished row = " << *m_row_start << " ... " << *m_row_end << "\n";
  std::cout << "--------------------------------------------------------------\n";
#endif
}

}  // namespace Trax
