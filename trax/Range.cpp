#include "Range.h"
#include <fmt/format.h>
#include <smartmet/macgyver/Exception.h>
#include <cmath>
#include <stdexcept>

namespace Trax
{
/*
 * Invalid ranges:
 *
 *  1. lo > hi     - lo==hi is allowed, and we increase hi to the next greater number
 *  2. -inf..-inf  - makes no sense
 *  3. inf..inf    - makes no sense
 *  4. lo..nan     - ordering makes no sense
 *  5. nan..hi     - ordering makes no sense
 */

Range::Range(double lo, double hi) : m_lo(lo), m_hi(hi)
{
  if (std::isnormal(lo) && std::isnormal(hi))
  {
    if (lo > hi)
      throw Fmi::Exception(
          BCP, fmt::format("Finite isoband limits must be in ascending order: {} >= {}", lo, hi));

    // Adjust range 5..5 to 5..5+eps since we use condition lo<=value<hi while contouring
    // and the intent is clear
    if (hi == lo)
    {
      m_hi = std::nextafter(hi, hi + 1);
      m_adjusted = true;
    }
  }
  else if (std::isnan(lo) || std::isnan(hi))
  {
    if (!std::isnan(lo) || !std::isnan(hi))
      throw Fmi::Exception(BCP, "Isoband limits must both be NaN, not just one");
  }
  else if (lo >= hi)
    throw Fmi::Exception(
        BCP, fmt::format("Isoband limits must be in ascending order: {} >= {}", lo, hi));
}

// If the range is from lo..hi with finite hi, modify hi to hi+eps
// This is used to make the last finite range include the hi value,
// as in say 90...100 for percentages.

void Range::adjust()
{
  if (m_adjusted)
    return;
  if (!std::isnormal(m_hi))
    return;
  m_hi = std::nextafter(m_hi, m_hi + 1);
  m_adjusted = true;
}

/* Ranges are sorted in the following ascending order so that
 * the possible ranges for a single grid cell can be found quickly:
 *
 * 1. nan..nan to cover missing values
 * 2. -inf..hi
 * 3. -inf..inf
 * 4. lo..hi
 * 5. lo..+inf
 *
 */

bool Range::operator<(const Range& other) const
{
  if (std::isnan(m_lo) || std::isnan(m_hi))
    return true;
  if (!std::isfinite(m_lo))
    return true;
  if (m_lo != other.lo())
    return (m_lo < other.lo());
  return (m_hi < other.hi());
}

// Needed since NaN comparisons are always false
namespace
{
bool same(double value1, double value2)
{
  if (std::isnan(value1))
    return std::isnan(value2);
  if (std::isnan(value2))
    return false;
  return value1 == value2;
}
}  // namespace

bool Range::operator==(const Range& other) const
{
  return same(m_lo, other.m_lo) && same(m_hi, other.m_hi);
}

bool Range::overlaps(double lo, double hi) const
{
  const auto common_lo = std::max(m_lo, lo);
  const auto common_hi = std::min(m_hi, hi);
  return (common_lo <= common_hi);
}

}  // namespace Trax
