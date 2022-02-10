#include "Place.h"

namespace Trax
{
Place discrete_place(double value, const Range& range)
{
  if (value < range.lo())
    return Place::Below;
  if (value >= range.hi())
    return Place::Below;
  return Place::Inside;
}

Place place(double value, double limit)
{
  if (value < limit)
    return Place::Below;
  return Place::Above;
}

}  // namespace Trax
