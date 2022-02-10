#include "Side.h"

namespace Trax
{
std::string to_string(Side side)
{
  switch (side)
  {
    case Side::None:
      return "none";
    case Side::Inside:
      return "inside";
    case Side::Left:
      return "left";
    case Side::Right:
      return "right";
    case Side::Top:
      return "top";
    case Side::Bottom:
      return "bottom";
    default:
      return "stupid compiler";
  }
}
}  // namespace Trax
