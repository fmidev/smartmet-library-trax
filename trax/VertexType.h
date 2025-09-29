#pragma once

#include <cstdint>

namespace Trax
{
// Note: This order is designed to enable lexicographic sorting so that column+1 vertices
// would be the last elements.
enum class VertexType : std::uint8_t
{
  Corner,
  Vertical_lo,
  Vertical_hi,
  Horizontal_lo,
  Horizontal_hi,
  Diagonal_lo,
  Diagonal_hi
};

bool is_vertical(VertexType type);
bool is_horizontal(VertexType type);

VertexType lo(VertexType type);
VertexType hi(VertexType type);

const char* to_string(VertexType type);

}  // namespace Trax
