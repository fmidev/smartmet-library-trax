#include "Contour.h"
#include "Impl.h"

namespace Trax
{
// The only available constructor
Contour::Contour(InterpolationType itype) : impl(new Contour::Impl(itype)) {}

// Destructor due to pimpl idiom
Contour::~Contour() = default;

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
