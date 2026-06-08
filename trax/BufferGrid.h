#pragma once

#include "Grid.h"
#include <memory>
#include <vector>

// A concrete, self-owned Grid holding a contiguous value buffer.
//
// This is the output of Trax::smooth(): an independent grid that can be fed
// straight into Contour::isobands()/isolines() or handed to any other Grid
// consumer. It owns the (smoothed) values but shares the source grid for all
// geometry, so coordinates are never duplicated:
//
//   - operator()/set() read/write the owned value buffer
//   - x()/y()/valid()/shift()/shell()/bbox() delegate to the source grid
//
// The source is held by shared_ptr so the BufferGrid stays valid independently
// of the caller's references. Smoothing is index-space, so delegating geometry
// is exact: cell (i,j) keeps the same corners and validity as in the source.
//
// Assumption: the source's valid() does not depend on live data values (e.g.
// it uses a precomputed validity mask, as NormalGrid does). With
// SmoothOptions::preserve_missing the NaN footprint is unchanged anyway, so
// delegation stays consistent even if valid() does inspect values.

namespace Trax
{
class BufferGrid : public Grid
{
 public:
  BufferGrid(std::shared_ptr<const Grid> source, std::vector<float> values);

  double x(long i, long j) const override { return m_source->x(i, j); }
  double y(long i, long j) const override { return m_source->y(i, j); }

  // Only the in-range region [0,width) x [0,height) carries smoothed values.
  // A source may expose a bbox() that extends beyond it and handle those
  // indices specially in its own operator() (e.g. a grid that pads its borders
  // with out-of-range NaN cells for missing-value contouring). Delegate such
  // out-of-range reads to the source so that padding/virtual cells keep their
  // original behaviour instead of indexing past the value buffer.
  float operator()(long i, long j) const override
  {
    if (i < 0 || j < 0 || static_cast<std::size_t>(i) >= m_width ||
        static_cast<std::size_t>(j) >= m_height)
      return (*m_source)(i, j);
    return m_values[i + m_width * j];
  }
  void set(long i, long j, float z) override { m_values[i + m_width * j] = z; }

  bool valid(long i, long j) const override { return m_source->valid(i, j); }

  std::size_t width() const override { return m_width; }
  std::size_t height() const override { return m_height; }
  std::size_t shift() const override { return m_source->shift(); }
  double shell() const override { return m_source->shell(); }
  std::size_t xperiod() const override { return m_source->xperiod(); }
  std::array<long, 4> bbox() const override { return m_source->bbox(); }

  // Direct contiguous access for the smoother and other bulk operations,
  // bypassing virtual per-cell dispatch.
  const std::vector<float>& values() const { return m_values; }
  std::vector<float>& values() { return m_values; }

 private:
  std::shared_ptr<const Grid> m_source;
  std::size_t m_width;
  std::size_t m_height;
  std::vector<float> m_values;
};

}  // namespace Trax
