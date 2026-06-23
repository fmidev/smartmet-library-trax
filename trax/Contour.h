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
  // Per request: desired number of row-bands to split contouring into (1 = single-threaded).
  // This has an effect ONLY when a worker pool has also been enabled via set_worker_threads();
  // with no pool (or n <= 1) the request runs single-threaded. Both settings are required for
  // parallelism — see set_worker_threads.
  void threads(int n);

  // Size of the process-wide worker pool that band tasks are dispatched to. Inject this once at
  // startup (e.g. the server sets it from a percentage of hardware_concurrency); n == 0 disables
  // the pool. Call before any contouring begins.
  //
  // BOTH settings are required for parallelism, and they are independent:
  //   - set_worker_threads(N>0) alone does nothing unless a request also calls threads(k>1);
  //   - threads(k>1) alone runs single-threaded unless the pool has been enabled here.
  // Any combination is safe — when either is missing, contouring simply runs single-threaded.
  static void set_worker_threads(std::size_t n);

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
