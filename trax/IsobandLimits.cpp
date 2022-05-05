#include "IsobandLimits.h"
#include <macgyver/Exception.h>
#include <algorithm>
#include <cmath>

namespace Trax
{
namespace
{
// If the last isoband is lo..hi with finite hi, modify hi to hi+eps
void adjust_last_finite_isoband(std::vector<Range>& limits)
{
  if (limits.empty())
    return;

  auto& range = limits.at(limits.size() - 1);
  range.adjust();
}
}  // namespace

// Add a single isoband
void IsobandLimits::add(float lo, float hi)
{
  m_limits.emplace_back(Range(lo, hi));
}

// Add multiple isobands
void IsobandLimits::add(const std::vector<Range>& limits)
{
  for (const auto& range : limits)
    m_limits.push_back(range);
}

// The given limits are valid if they are lexiographically sorted
bool IsobandLimits::valid() const
{
  return std::is_sorted(m_limits.begin(), m_limits.end());
}

// Sort the ranges into increasing order and record the original position of the range
void IsobandLimits::sort(bool closed_range)
{
  auto input_limits = m_limits;
  std::sort(m_limits.begin(), m_limits.end());

  for (const auto& limits : m_limits)
  {
    bool ok = false;
    for (auto j = 0UL; j < input_limits.size() && !ok; j++)
    {
      if (input_limits[j] == limits)
      {
        m_positions.push_back(j);
        ok = true;
      }
    }

    if (!ok)
      throw Fmi::Exception(BCP, "Found no original position for isoband range");
  }

  if (closed_range)
    adjust_last_finite_isoband(m_limits);
}

}  // namespace Trax
