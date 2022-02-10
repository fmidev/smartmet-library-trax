#include "IsobandLimits.h"
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
void IsobandLimits::add(double lo, double hi)
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

void IsobandLimits::sort()
{
  std::sort(m_limits.begin(), m_limits.end());
  adjust_last_finite_isoband(m_limits);
}

}  // namespace Trax
