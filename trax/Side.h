#pragma once

#include <string>

namespace Trax
{
enum class Side
{
  None,
  Inside,
  Left,
  Right,
  Top,
  Bottom
};

std::string to_string(Side side);

}  // namespace Trax
