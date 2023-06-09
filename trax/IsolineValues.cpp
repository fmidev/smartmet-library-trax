#include "IsolineValues.h"
#include <fmt/format.h>
#include <macgyver/Exception.h>
#include <algorithm>

namespace Trax
{
namespace
{
// We want NaN to be the first element. NaN breaks std::sort for not satisfying the ordering
// requirements
auto isovalue_cmp = [](float a, float b)
{
  if (std::isnan(a))
    return true;
  if (std::isnan(b))
    return false;
  return a < b;
};

bool both_nan(float a, float b)
{
  return std::isnan(a) && std::isnan(b);
}

bool same(float a, float b)
{
  return (a == b || both_nan(a, b));
}

}  // namespace

// Add a single isoline value
void IsolineValues::add(float value)
{
  m_values.push_back(value);
}

// Add multiple isoline values
void IsolineValues::add(const std::vector<float> &values)
{
  m_values.insert(m_values.end(), values.begin(), values.end());
}

// We want the values to be sorted into ascending order so that we can quickly
// process the possible isolines for a single grid cell.
bool IsolineValues::valid() const
{
  return std::is_sorted(m_values.begin(), m_values.end(), isovalue_cmp);
}

void IsolineValues::sort()
{
  auto input_values = m_values;
  std::sort(m_values.begin(), m_values.end(), isovalue_cmp);

  for (auto value : m_values)
  {
    bool ok = false;
    for (auto j = 0UL; j < input_values.size() && !ok; j++)
    {
      if (same(input_values[j], value))
      {
        m_positions.push_back(j);
        ok = true;
      }
    }

    if (!ok)
      throw Fmi::Exception(BCP, "Found no original position for isoline limit");
  }
}

std::string IsolineValues::dump() const
{
  std::string ret;
  for (auto value : m_values)
  {
    if (ret.empty())
      ret += '[';
    else
      ret += ',';
    ret += fmt::format("{}", value);
  }
  ret += ']';
  return ret;
}

}  // namespace Trax
