#pragma once

#include <vector>

namespace Trax
{
class IsolineValues
{
 public:
  void add(float value);
  void add(const std::vector<float> &value);

  bool empty() const { return m_values.empty(); }
  std::size_t size() const { return m_values.size(); }

  void sort();
  bool valid() const;

  float operator[](std::size_t i) const { return m_values[i]; }
  float at(std::size_t i) const { return m_values.at(i); }

  std::size_t original_position(std::size_t i) const { return m_positions[i]; }

 private:
  std::vector<float> m_values;
  std::vector<std::size_t> m_positions;  // original positions of the ranges

};  // class IsolineValues

}  // namespace Trax
