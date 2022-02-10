// Test grid with a viewbox

#include "Grid.h"
#include <fmt/format.h>
#include <vector>

namespace Trax
{
class TestGrid : public Grid
{
 public:
  TestGrid(long nx, long ny, double x1, double y1, double x2, double y2)
      : m_nx(nx), m_ny(ny), m_x1(x1), m_y1(y1), m_x2(x2), m_y2(y2), m_values(nx * ny, 0.0)
  {
  }

  double x(long i, long j) const override { return m_x1 + i * (m_x2 - m_x1) / (m_nx - 1); }
  double y(long i, long j) const override { return m_y1 + j * (m_y2 - m_y1) / (m_ny - 1); }
  double operator()(long i, long j) const override { return m_values[i + m_nx * j]; }
  void set(long i, long j, double z) override { m_values[i + m_nx * j] = z; }
  bool valid(long i, long j) const override { return true; }
  std::size_t width() const override { return m_nx; }
  std::size_t height() const override { return m_ny; }

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
        out += fmt::format("{}", (*this)(i, j));
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
  std::vector<double> m_values;
};

}  // namespace Trax
