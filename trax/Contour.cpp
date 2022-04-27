#include "Contour.h"
#include "Impl.h"

namespace Trax
{
// The only available constructor
Contour::Contour() : impl(new Contour::Impl()) {}

// Destructor due to pimpl idiom
Contour::~Contour() = default;

// Set interpolation type
void Contour::interpolation(InterpolationType itype)
{
  impl->interpolation(itype);
}

// Mark how to handle the last interval
void Contour::closed_range(bool flag)
{
  impl->closed_range(flag);
}

// Enable/disable strict mode
void Contour::strict(bool flag)
{
  impl->strict(flag);
}

// Enable/disable geometry validation
void Contour::validate(bool flag)
{
  impl->validate(flag);
}

// Set bbox for inverting isobands for missing values
void Contour::bbox(double mincoord, double maxcoord)
{
  impl->bbox(mincoord, maxcoord);
}

// Contour full grid
GeometryCollections Contour::isobands(const Grid& grid, const IsobandLimits& limits)
{
  return impl->isobands(grid, limits);
}

// Contour full grid for isolines
GeometryCollections Contour::isolines(const Grid& grid, const IsolineValues& limits)
{
  return impl->isolines(grid, limits);
}

}  // namespace Trax
