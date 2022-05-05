#pragma once

#include <cassert>
#include <vector>

namespace Trax
{
class Range
{
 public:
  Range() = delete;
  Range(float lo, float hi);
  float lo() const { return m_lo; }
  float hi() const { return m_hi; }
  void adjust();
  bool adjusted() const { return m_adjusted; }
  bool operator<(const Range& other) const;
  bool operator==(const Range& other) const;
  bool overlaps(float lo, float hi) const;
  bool missing() const;

 private:
  float m_lo;
  float m_hi;
  bool m_adjusted = false;  // has hi been modified to hi+eps

};  // class Range
}  // namespace Trax
