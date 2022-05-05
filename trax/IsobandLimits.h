#pragma once

#include "Range.h"
#include <vector>

namespace Trax
{
class IsobandLimits
{
 public:
  void add(float lo, float hi);
  void add(const std::vector<Range> &limits);

  bool empty() const { return m_limits.empty(); }
  std::size_t size() const { return m_limits.size(); }

  void sort(bool closed_range);
  bool valid() const;

  const Range &operator[](std::size_t i) const { return m_limits[i]; }
  const Range &at(std::size_t i) const { return m_limits.at(i); }

  std::size_t original_position(std::size_t i) const { return m_positions[i]; }

 private:
  std::vector<Range> m_limits;
  std::vector<std::size_t> m_positions;  // original positions of the ranges

};  // class IsobandLimits
}  // namespace Trax
