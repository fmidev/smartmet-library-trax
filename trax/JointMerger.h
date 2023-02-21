#pragma once

#include "JointPool.h"
#include <cstdint>

namespace Trax
{
struct Cell;

/*
 * JointMerger merges joints either by appending to the end of the topmost row
 * or by merging with the previous row. The JointMerger is a friend of the JointPool
 * containing the joints in order to be able to find matching joints as quickly
 * as possible without having to use operator[] only, which requires iterative
 * calculations to find the correct buffer and the element in it.
 *
 * Non-copyable since JointPool is.
 */

class JointMerger
{
 public:
  ~JointMerger() = default;
  JointMerger() = default;
  JointMerger(JointMerger&& other) noexcept;

  JointMerger(const JointMerger& other) = delete;
  JointMerger& operator=(const JointMerger& other) = delete;
  JointMerger& operator=(JointMerger&& other) = delete;

  // Create a new joint from the pool
  Joint* create(const Vertex& vertex) { return m_pool.create(vertex); }

  JointPool& pool() { return m_pool; }

  // Merge new cell elements to to earlier ones on the same row
  void merge_cell();

  // Merge a full grid cell
  void merge_cell(const Cell& c);

  // Merge a closed ring from a grid cell
  void merge_cell(const Vertices& vertices);

  void finish_cell();

  // Merge new row of elements to the previous one
  void merge_row();

 private:
  // Find match from the previous row [minpos...maxpos] and a search hint
  // Joint* find_match(const Vertex& vertex, std::size_t minpos, std::size_t maxpos, std::size_t
  // hint) const;

  JointPool m_pool;                                     // joints from one or more merged rows
  JointPool::iterator m_row_start{m_pool.end()};        // start position of topmost finished row
  JointPool::iterator m_row_end{m_pool.end()};          // end position of topmost finished row
  JointPool::iterator m_last_cell_start{m_pool.end()};  // range of last cell
  JointPool::iterator m_last_cell_end{m_pool.end()};    // merge may include last cell plus one ring
  JointPool::iterator m_cell_merge_end{m_pool.end()};  // from current cell, this iterator tracks it
  std::int32_t m_maxrow = 0;  // maximum row number in the data so far in the topmost finished row
};

}  // namespace Trax
