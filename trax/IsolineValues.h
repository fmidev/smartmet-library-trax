#pragma once

#include <vector>

namespace Trax
{
class IsolineValues
{
 public:
  void add(double value);
  void add(const std::vector<double> &value);

  bool empty() const { return m_values.empty(); }
  std::size_t size() const { return m_values.size(); }

  void sort();
  bool valid() const;

  double operator[](std::size_t i) const { return m_values[i]; }
  double at(std::size_t i) const { return m_values.at(i); }

 private:
  std::vector<double> m_values;

};  // class IsolineValues

}  // namespace Trax
