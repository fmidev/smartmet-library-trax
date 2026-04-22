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
  // 0 (default) = off, classic linear marching-squares straight segment between edge
  // intersections. N>=2 splits each cell level-curve segment into N sub-segments with
  // N-1 samples on the true bilinear level curve so curved isoband boundaries
  // (e.g. a single peak surrounded by zeros) no longer render as diamonds.
  // N=1 is a no-op by construction (one segment, zero interior samples).
  // Clamped to [0, 10].
  void subdivide(int n);

  GeometryCollections isobands(const Grid& grid, const IsobandLimits& limits);
  GeometryCollections isolines(const Grid& grid, const IsolineValues& limits);

 private:
  // Implementation details
  class Impl;
  std::unique_ptr<Impl> impl;
};

}  // namespace Trax
