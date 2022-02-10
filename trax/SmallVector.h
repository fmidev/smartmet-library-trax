#pragma once

#include <array>

namespace Trax
{
template <typename T, std::size_t Dim>
class SmallVector
{
 public:
  using iterator = typename std::array<T, Dim>::iterator;
  using const_iterator = typename std::array<T, Dim>::const_iterator;

  SmallVector() = default;
  std::size_t size() const { return m_size; }
  bool empty() const { return m_size == 0; }
  void clear() { m_size = 0; }
  const T& operator[](std::size_t i) const { return m_data[i]; }
  T& operator[](std::size_t i) { return m_data[i]; }
  const T& back() const { return m_data[m_size - 1]; }
  T& back() { return m_data[m_size - 1]; }
  void push_back(T value) { m_data[m_size++] = value; }
  void pop_back() { --m_size; }
  void resize(std::size_t sz) { m_size = sz; }

  iterator begin() { return m_data.begin(); }
  iterator end()
  {
    auto pos = m_data.begin();
    std::advance(pos, m_size);
    return pos;
  }

  const_iterator begin() const { return m_data.begin(); }
  const_iterator end() const
  {
    auto pos = m_data.begin();
    std::advance(pos, m_size);
    return pos;
  }

 private:
  std::size_t m_size = 0;
  std::array<T, Dim> m_data;
};

}  // namespace Trax
