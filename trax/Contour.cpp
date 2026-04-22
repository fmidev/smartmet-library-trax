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

// Enable/disable removing slivers
void Contour::desliver(bool flag)
{
  impl->desliver(flag);
}

// Set thread count: 1 = single-threaded (default), N>1 = N threads, 0 = auto
void Contour::threads(int n)
{
  impl->threads(n);
}

// Set interior densification count, clamped to [0, 10]
void Contour::subdivide(int n)
{
  if (n < 0)
    n = 0;
  else if (n > 10)
    n = 10;
  impl->subdivide(n);
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
