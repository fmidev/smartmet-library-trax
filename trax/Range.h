#pragma once

#include <cassert>
#include <vector>

namespace Trax
{
class Range
{
 public:
  Range() = delete;
  Range(double lo, double hi);
  double lo() const { return m_lo; }
  double hi() const { return m_hi; }
  void adjust();
  bool adjusted() const { return m_adjusted; }
  bool operator<(const Range& other) const;
  bool operator==(const Range& other) const;
  bool overlaps(double lo, double hi) const;
  bool missing() const;

 private:
  double m_lo;
  double m_hi;
  bool m_adjusted = false;  // has hi been modified to hi+eps

};  // class Range
}  // namespace Trax
