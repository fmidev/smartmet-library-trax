#pragma once

namespace Trax
{
struct GridPoint
{
  GridPoint() = default;
  GridPoint(double x, double y, float z) : x(x), y(y), z(z){};
  double x = 0;
  double y = 0;
  float z = 0;
};

}  // namespace Trax
