#pragma once

#include <utility>  // std::pair

namespace Trax
{
struct Cell
{
  Cell() = delete;

  // clang-format off
  Cell(float X1, float Y1, float Z1,
       float X2, float Y2, float Z2,
       float X3, float Y3, float Z3,
       float X4, float Y4, float Z4,
       int I, int J)
      : x1(X1), y1(Y1), z1(Z1),
        x2(X2), y2(Y2), z2(Z2),
        x3(X3), y3(Y3), z3(Z3),
        x4(X4), y4(Y4), z4(Z4),
        i(I), j(J)
  {
  }
  // clang-format on

  float x1, y1, z1;  // below left
  float x2, y2, z2;  // above left
  float x3, y3, z3;  // above right
  float x4, y4, z4;  // below right
  int i;             // below left corner indices
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
