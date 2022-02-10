#pragma once

#include "Cell.h"
#include "GeometryCollection.h"
#include "Grid.h"
#include "InterpolationType.h"
#include <memory>

namespace Trax
{
class IsobandLimits;
class IsolineValues;

class Contour
{
 public:
  ~Contour();
  Contour(InterpolationType itype);

  Contour() = delete;
  Contour(const Contour&) = delete;
  Contour(Contour&&) = delete;
  Contour& operator=(const Contour&) = delete;
  Contour& operator=(Contour&&) = delete;

  GeometryCollections isobands(const Grid& grid, const IsobandLimits& limits);
  GeometryCollections isolines(const Grid& grid, const IsolineValues& limits);

 private:
  // Implementation details
  class Impl;
  std::unique_ptr<Impl> impl;
};

}  // namespace Trax
