#include "VertexType.h"

namespace Trax
{
bool is_vertical(VertexType type)
{
  return type == VertexType::Vertical_lo || type == VertexType::Vertical_hi;
}

bool is_horizontal(VertexType type)
{
  return type == VertexType::Horizontal_lo || type == VertexType::Horizontal_hi;
}

VertexType lo(VertexType type)
{
  switch (type)
  {
    case VertexType::Horizontal_hi:
      return VertexType::Horizontal_lo;
    case VertexType::Vertical_hi:
      return VertexType::Vertical_lo;
    case VertexType::Diagonal_hi:
      return VertexType::Diagonal_lo;
    default:
      return type;
  }
}

VertexType hi(VertexType type)
{
  switch (type)
  {
    case VertexType::Horizontal_lo:
      return VertexType::Horizontal_hi;
    case VertexType::Vertical_lo:
      return VertexType::Vertical_hi;
    case VertexType::Diagonal_lo:
      return VertexType::Diagonal_hi;
    default:
      return type;
  }
}

const char* to_string(VertexType type)
{
  switch (type)
  {
    case VertexType::Corner:
      return "cor";
    case VertexType::Horizontal_lo:
      return "hlo";
    case VertexType::Horizontal_hi:
      return "hhi";
    case VertexType::Vertical_lo:
      return "vlo";
    case VertexType::Vertical_hi:
      return "vhi";
    case VertexType::Diagonal_lo:
      return "dlo";
    case VertexType::Diagonal_hi:
      return "dhi";
      // gcc 4.8.5 does not see that all enum class values have been processed
    default:
      return "nan";
  }
}

}  // namespace Trax
