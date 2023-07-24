#include "Endian.h"
#include <cstdint>

namespace Trax
{
unsigned char byteorder()
{
  unsigned char byteOrder = 1;
  std::uint16_t word = 1;
  // codechecker_false_positive [knownConditionTrueFalse] This is a well known idiom
  if (*reinterpret_cast<std::uint8_t*>(&word) == 0)
    byteOrder = 1;
  return byteOrder;
}

}  // namespace Trax
