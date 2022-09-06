#pragma once

#include "GridPoint.h"
#include <utility>  // std::pair

namespace Trax
{
struct Cell
{
  Cell() = delete;

  // clang-format off
  Cell(const GridPoint& P1, const GridPoint& P2, const GridPoint& P3, const GridPoint& P4, int I, int J)
      : p1(P1), p2(P2), p3(P3), p4(P4), i(I), j(J)
  {
  }
  // clang-format on

  GridPoint p1;  // below left
  GridPoint p2;  // above left
  GridPoint p3;  // above right
  GridPoint p4;  // below right
  int i;         // below left corner indices
  int j;
};

// Test if the cell contains a saddle point
bool is_saddle(const Cell& cell);

// Test which diagonal has larger values
bool first_diagonal_larger(const Cell& cell);

// Return minmax values of a grid cell
using MinMax = std::pair<float, float>;
MinMax minmax(const Cell& cell);

}  // namespace Trax
