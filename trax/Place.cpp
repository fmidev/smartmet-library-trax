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

std::string to_string(Place place)
{
  switch (place)
  {
    case Place::Below:
      return "Below";
    case Place::Inside:
      return "Inside";
    case Place::Above:
      return "Above";
  }
  return "Unknown";
}

}  // namespace Trax
