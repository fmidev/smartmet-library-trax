#pragma once

#include <vector>

namespace Trax
{
struct Point
{
  Point(double x, double y, bool ghost) : x(x), y(y), ghost(ghost) {}
  double x;
  double y;
  bool ghost;
};

using Points = std::vector<Point>;
}  // namespace Trax
