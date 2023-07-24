#pragma once

#include <cmath>
#include <vector>

namespace Trax
{
struct Point
{
  Point(double x, double y, bool ghost) : x(x), y(y), ghost(ghost) {}

  double x;
  double y;
  bool ghost;

  bool operator<(const Point& other) const
  {
    if (x != other.x)
      return x < other.x;
    return y < other.y;
  }
};

using Points = std::vector<Point>;

// When we want to avoid std::sqrt and std::hypot for speed
inline double squared_distance(const Point& p1, const Point& p2)
{
  auto dx = p1.x - p2.x;
  auto dy = p1.y - p2.y;
  return dx * dx + dy * dy;
}

inline bool operator!=(const Point& p1, const Point& p2)
{
  return (p1.x != p2.x || p1.y != p2.y);
}

inline bool operator==(const Point& p1, const Point& p2)
{
  return !(p1 != p2);
}

}  // namespace Trax
