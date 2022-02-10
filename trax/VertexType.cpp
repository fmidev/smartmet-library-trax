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

const char* to_string(VertexType vtype)
{
  switch (vtype)
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
      // gcc 4.8.5 does not see that all enum class values have been processed
    default:
      return "nan";
  }
}

}  // namespace Trax
