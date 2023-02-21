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
  Grid() = default;
  Grid(const Grid& other) = default;
  Grid(Grid&& other) = default;
  Grid& operator=(const Grid& other) = default;
  Grid& operator=(Grid&& other) = default;

  virtual double x(long i, long j) const = 0;
  virtual double y(long i, long j) const = 0;
  virtual double operator()(long i, long j) const = 0;
  virtual void set(long i, long j, double z) = 0;
  virtual bool valid(long i, long j) const = 0;
  virtual std::size_t width() const = 0;
  virtual std::size_t height() const = 0;
  virtual std::size_t shift() const;
  virtual std::array<long, 4> bbox() const;
  virtual double shell() const;
};

}  // namespace Trax
