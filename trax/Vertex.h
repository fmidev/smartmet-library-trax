#pragma once

#include "SmallVector.h"
#include "VertexType.h"
#include <cstdint>

namespace Trax
{
struct Vertex
{
  Vertex(std::uint32_t i, std::uint32_t j, VertexType vt, double x, double y, bool gh)
      : x(x), y(y), column(i), row(j), type(vt), ghost(gh)
  {
  }
  Vertex() = default;

  float x = 0;  // using float instead of double gives another performance boost
  float y = 0;
  std::uint32_t column = 0;              // we need column at the minimum for fast merge of rows
  std::uint32_t row = 0;                 // but also use row for exact vertex comparisons.
  VertexType type = VertexType::Corner;  // intersection type
  bool ghost = false;                    // true if the value is not exactly range.lo()
};

using Vertices = SmallVector<Vertex, 8UL>;  // maximum from a single grid cell

// When looking for vertex matches the rows are +-1 but columns may differ wildly, hence the order.
inline bool operator==(const Vertex& v1, const Vertex& v2)
{
  return v1.column == v2.column && v1.row == v2.row && v1.type == v2.type;
}

inline bool operator!=(const Vertex& v1, const Vertex& v2)
{
  return v1.column != v2.column || v1.row != v2.row || v1.type != v2.type;
}

}  // namespace Trax
