#include "IsolineValues.h"
#include <algorithm>

namespace Trax
{
// Add a single isoline value
void IsolineValues::add(double value)
{
  m_values.push_back(value);
}

// Add multiple isoline values
void IsolineValues::add(const std::vector<double> &values)
{
  m_values.insert(m_values.end(), values.begin(), values.end());
}

// We want the values to be sorted into ascending order so that we can quickly
// process the possible isolines for a single grid cell.
bool IsolineValues::valid() const
{
  return std::is_sorted(m_values.begin(), m_values.end());
}

void IsolineValues::sort()
{
  std::sort(m_values.begin(), m_values.end());
}

}  // namespace Trax
