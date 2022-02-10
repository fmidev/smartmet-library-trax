#pragma once

#include "Range.h"
#include <vector>

namespace Trax
{
class IsobandLimits
{
 public:
  void add(double lo, double hi);
  void add(const std::vector<Range> &limits);

  bool empty() const { return m_limits.empty(); }
  std::size_t size() const { return m_limits.size(); }

  void sort();
  bool valid() const;

  const Range &operator[](std::size_t i) const { return m_limits[i]; }
  const Range &at(std::size_t i) const { return m_limits.at(i); }

 private:
  std::vector<Range> m_limits;

};  // class IsobandLimits
}  // namespace Trax
