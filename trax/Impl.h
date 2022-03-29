#pragma once
#include "Builder.h"
#include "Contour.h"
#include "IsobandLimits.h"
#include "IsolineValues.h"

namespace Trax
{
class Contour::Impl
{
 public:
  Impl() = default;

  void interpolation(InterpolationType itype) { m_itype = itype; }
  void closed_range(bool flag) { m_closed_range = flag; }
  void strict(bool flag) { m_strict = flag; }
  void validate(bool flag) { m_validate = flag; }

  // Calculate full set of contours
  GeometryCollections isobands(const Grid& grid, const IsobandLimits& limits);
  GeometryCollections isolines(const Grid& grid, const IsolineValues& limits);

 private:
  // Prepare for isoline calculations
  void init(const IsolineValues& values, std::size_t width, std::size_t height);

  // Prepare for isoband calculations
  void init(const IsobandLimits& limits, std::size_t width, std::size_t height);

  // Move the result for the caller
  GeometryCollections result();

  // Update all isolines from a single cell
  void isoline(const Cell& cell);

  // Update all isobands from a single cell
  void isoband(const Cell& cell);

  // Finalize results
  void finish_row();
  void finish_isolines();
  void finish_isobands();

  // Update a specific isoline from a single cell
  void isoline(int index, const Cell& c);

  // Update a specific isoband from a single cell
  void isoband(int index, const Cell& c);

  void isoband_linear(int index, const Cell& c);

  void isoband_midpoint(int index, const Cell& c);

  // Based on min/max values update contour indexes to be checked
  bool update_isolines_to_check(const MinMax& minmax);
  bool update_isobands_to_check(const MinMax& minmax);

  // ---- Member variables ----

  InterpolationType m_itype = InterpolationType::Linear;

  // Should the last range be closed [lo,hi] instead of half-open [lo,hi[
  bool m_closed_range = false;

  // Do not allow any internal errors in strict mode. Some projections
  // may cause problems for example near the poles due to rounding errors
  // in projection calculations, hence our default is false.
  bool m_strict = false;

  // Perform geometry validation with GEOS. This can be very slow.
  bool m_validate = false;

  // Requested isolines
  IsolineValues m_isoline_values;

  // Requested isobands
  IsobandLimits m_isoband_limits;

  // Unfinished geometries, one for each contour
  using Builders = std::vector<Builder>;
  Builders m_builders;

  // The range of isolinevalues or isobandlimits to be processed.
  //
  // Moving to the next cell to the right adjusts the values based on the assumption that the
  // contours are almost always almost the same.

  int m_min_index = 0;
  int m_max_index = 0;
};

}  // namespace Trax
