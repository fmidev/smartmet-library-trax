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
  Contour();

  Contour(const Contour&) = delete;
  Contour(Contour&&) = delete;
  Contour& operator=(const Contour&) = delete;
  Contour& operator=(Contour&&) = delete;

  void interpolation(InterpolationType itype);
  void closed_range(bool flag);
  void strict(bool flag);
  void validate(bool flag);
  void desliver(bool flag);
  void threads(int n);  // 1 = single-threaded (default), N>1 = N threads, 0 = auto

  // Interior densification count per cell-level-curve segment.
  // 0 (default) = current linear marching-squares straight segment between edge intersections.
  // N in [1..4] = N-1 extra samples on the true bilinear level curve so curved isoband
  // boundaries (e.g. a single peak surrounded by zeros) no longer render as diamonds.
  // Clamped to [0, 4].
  void subdivide(int n);

  GeometryCollections isobands(const Grid& grid, const IsobandLimits& limits);
  GeometryCollections isolines(const Grid& grid, const IsolineValues& limits);

 private:
  // Implementation details
  class Impl;
  std::unique_ptr<Impl> impl;
};

}  // namespace Trax
