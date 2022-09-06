#pragma once

namespace Trax
{
struct GridPoint
{
  GridPoint() = default;
  GridPoint(float x, float y, float z) : x(x), y(y), z(z){};
  float x = 0;
  float y = 0;
  float z = 0;
};

}  // namespace Trax
