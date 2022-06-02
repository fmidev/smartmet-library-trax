#pragma once

#include <array>
#include <cstddef>

// Required GRID interface

namespace Trax
{
class Grid
{
 public:
  virtual ~Grid();

  virtual double x(long i, long j) const = 0;
  virtual double y(long i, long j) const = 0;
  virtual double operator()(long i, long j) const = 0;
  virtual void set(long i, long j, double z) = 0;
  virtual bool valid(long i, long j) const = 0;
  virtual std::size_t width() const = 0;
  virtual std::size_t height() const = 0;
  virtual std::size_t shift() const { return 0UL; }
  virtual std::array<std::size_t, 4> bbox() const { return {0, 0, width() - 2, height() - 2}; }
};

}  // namespace Trax
