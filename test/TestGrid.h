// Test grid with a viewbox

#include "Grid.h"
#include <fmt/format.h>
#include <cmath>
#include <vector>

namespace Trax
{
class TestGrid : public Grid
{
 public:
  TestGrid(long nx, long ny, double x1, double y1, double x2, double y2)
      : m_nx(nx),
        m_ny(ny),
        m_x1(x1),
        m_y1(y1),
        m_x2(x2),
        m_y2(y2),
        m_values(nx * ny, 0.0),
        m_x(nx * ny, 0.0),
        m_y(nx * ny, 0.0)
  {
    for (auto j = 0; j < m_ny; j++)
      for (auto i = 0; i < m_nx; i++)
      {
        auto xx = m_x1 + i * (m_x2 - m_x1) / (m_nx - 1);
        auto yy = m_y1 + j * (m_y2 - m_y1) / (m_ny - 1);
        set(i, j, xx, yy);
      }
  }

  std::size_t width() const override { return m_nx; }
  std::size_t height() const override { return m_ny; }

  double x(long i, long j) const override { return m_x[i + m_nx * j]; }
  double y(long i, long j) const override { return m_y[i + m_nx * j]; }
  float operator()(long i, long j) const override { return m_values[i + m_nx * j]; }
  float get(long i, long j) const { return m_values[i + m_nx * j]; }

  void set(long i, long j, float z) override { m_values[i + m_nx * j] = z; }

  void set(long i, long j, double x, double y)
  {
    m_x[i + m_nx * j] = x;
    m_y[i + m_nx * j] = y;
  }

  bool valid(long i, long j) const override
  {
#if 0    
    return !std::isnan(get(i, j)) && !std::isnan(get(i + 1, j)) && !std::isnan(get(i, j + 1)) &&
           !std::isnan(get(i + 1, j + 1)) && !std::isnan(x(i, j)) && !std::isnan(x(i + 1, j)) &&
           !std::isnan(x(i, j + 1)) && !std::isnan(x(i + 1, j + 1)) && !std::isnan(y(i, j)) &&
           !std::isnan(y(i + 1, j)) && !std::isnan(y(i, j + 1)) && !std::isnan(y(i + 1, j + 1));
#else
    return !std::isnan(x(i, j)) && !std::isnan(x(i + 1, j)) && !std::isnan(x(i, j + 1)) &&
           !std::isnan(x(i + 1, j + 1)) && !std::isnan(y(i, j)) && !std::isnan(y(i + 1, j)) &&
           !std::isnan(y(i, j + 1)) && !std::isnan(y(i + 1, j + 1));
#endif
  }

  std::string dump(const std::string& indent) const
  {
    std::string out;
    for (long j = m_ny - 1; j >= 0; j--)
    {
      for (long i = 0; i < m_nx; i++)
      {
        if (i == 0)
          out += indent;
        else
          out += ' ';
        auto value = (*this)(i, j);
        if (std::isnan(value))
          out += '-';
        else
          out += fmt::format("{}", value);
      }
      out += '\n';
    }
    return out;
  }

 private:
  const long m_nx;
  const long m_ny;
  const double m_x1;
  const double m_y1;
  const double m_x2;
  const double m_y2;
  std::vector<float> m_values;
  std::vector<double> m_x;
  std::vector<double> m_y;
};  // namespace Trax

}  // namespace Trax
