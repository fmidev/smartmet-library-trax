#include "SmoothOptions.h"
#include "Smoother.h"
#include "TestGrid.h"
#include <boost/test/included/unit_test.hpp>
#include <cmath>
#include <memory>

using namespace boost::unit_test;

namespace
{
std::shared_ptr<Trax::TestGrid> make_grid(long nx, long ny, float fill)
{
  auto g = std::make_shared<Trax::TestGrid>(nx, ny, 0.0, 0.0, nx - 1.0, ny - 1.0);
  for (long j = 0; j < ny; j++)
    for (long i = 0; i < nx; i++)
      g->set(i, j, fill);
  return g;
}

// A TestGrid that declares an x-period, to exercise periodic (longitude
// wrap-around) smoothing.
class PeriodicGrid : public Trax::TestGrid
{
 public:
  PeriodicGrid(long nx, long ny, std::size_t period)
      : Trax::TestGrid(nx, ny, 0.0, 0.0, nx - 1.0, ny - 1.0), m_period(period)
  {
  }
  std::size_t xperiod() const override { return m_period; }

 private:
  std::size_t m_period;
};

std::shared_ptr<PeriodicGrid> make_periodic(long nx, long ny, std::size_t period)
{
  return std::make_shared<PeriodicGrid>(nx, ny, period);
}

// A TestGrid whose operator() returns a sentinel for indices outside
// [0,width) x [0,height), mimicking a grid (such as the contour engine's
// PaddedGrid) that exposes a bbox extending beyond its value dimensions.
class PaddedTestGrid : public Trax::TestGrid
{
 public:
  static constexpr float kPad = -999.0F;
  PaddedTestGrid(long nx, long ny) : Trax::TestGrid(nx, ny, 0.0, 0.0, nx - 1.0, ny - 1.0) {}
  float operator()(long i, long j) const override
  {
    if (i < 0 || j < 0 || i >= static_cast<long>(width()) || j >= static_cast<long>(height()))
      return kPad;
    return Trax::TestGrid::operator()(i, j);
  }
};

Trax::SmoothOptions box(int radius, int passes, Trax::SmoothBoundary boundary, bool preserve)
{
  Trax::SmoothOptions o;
  o.method = Trax::SmoothMethod::Box;
  o.boundary = boundary;
  o.radius = radius;
  o.passes = passes;
  o.preserve_missing = preserve;
  return o;
}

Trax::SmoothOptions median(int radius, int passes, Trax::SmoothBoundary boundary, bool preserve)
{
  Trax::SmoothOptions o;
  o.method = Trax::SmoothMethod::Median;
  o.boundary = boundary;
  o.radius = radius;
  o.passes = passes;
  o.preserve_missing = preserve;
  return o;
}

Trax::SmoothOptions morph(
    int radius, int passes, Trax::MorphologyOp op, Trax::SmoothBoundary boundary, bool preserve)
{
  Trax::SmoothOptions o;
  o.method = Trax::SmoothMethod::Morphology;
  o.boundary = boundary;
  o.radius = radius;
  o.passes = passes;
  o.morphology = op;
  o.preserve_missing = preserve;
  return o;
}

Trax::SmoothOptions savgol(int size, int degree)
{
  Trax::SmoothOptions o;
  o.method = Trax::SmoothMethod::SavitzkyGolay;
  o.radius = size;  // window half-width
  o.degree = degree;
  return o;
}
}  // namespace

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Smoother tests";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(inactive_returns_same_grid)
{
  BOOST_TEST_MESSAGE("+ [inactive options return the source grid unchanged]");
  auto g = make_grid(5, 5, 3.0F);
  std::shared_ptr<const Trax::Grid> in = g;

  auto out = Trax::smooth(in, box(0, 3, Trax::SmoothBoundary::Normalized, true));
  BOOST_CHECK(out == in);  // same pointer, no work done

  Trax::SmoothOptions none;  // method == None
  BOOST_CHECK(Trax::smooth(in, none) == in);
}

BOOST_AUTO_TEST_CASE(constant_field_is_preserved)
{
  BOOST_TEST_MESSAGE("+ [a constant field stays constant under all boundary modes]");
  for (auto b : {Trax::SmoothBoundary::Normalized,
                 Trax::SmoothBoundary::Replicate,
                 Trax::SmoothBoundary::Reflect})
  {
    auto g = make_grid(7, 6, 42.0F);
    auto out = Trax::smooth(g, box(2, 3, b, true));
    for (long j = 0; j < 6; j++)
      for (long i = 0; i < 7; i++)
        BOOST_CHECK_CLOSE((*out)(i, j), 42.0F, 1e-3);
  }
}

BOOST_AUTO_TEST_CASE(nan_does_not_poison_neighbours)
{
  BOOST_TEST_MESSAGE("+ [a missing cell stays missing and does not corrupt its neighbours]");
  auto g = make_grid(5, 5, 10.0F);
  g->set(2, 2, std::numeric_limits<float>::quiet_NaN());

  // preserve_missing: the hole stays NaN, everything else stays 10 (only 10s in each window)
  auto kept = Trax::smooth(g, box(1, 1, Trax::SmoothBoundary::Normalized, true));
  BOOST_CHECK(std::isnan((*kept)(2, 2)));
  for (long j = 0; j < 5; j++)
    for (long i = 0; i < 5; i++)
      if (!(i == 2 && j == 2))
        BOOST_CHECK_CLOSE((*kept)(i, j), 10.0F, 1e-3);

  // without preserve_missing the hole is filled from valid neighbours (all 10)
  auto filled = Trax::smooth(g, box(1, 1, Trax::SmoothBoundary::Normalized, false));
  BOOST_CHECK_CLOSE((*filled)(2, 2), 10.0F, 1e-3);
}

BOOST_AUTO_TEST_CASE(no_overshoot)
{
  BOOST_TEST_MESSAGE("+ [smoothed values stay within the input data range]");
  auto g = make_grid(9, 9, 0.0F);
  // A single tall spike surrounded by zeros: a low-pass filter must not exceed it.
  float lo = 0.0F;
  float hi = 100.0F;
  g->set(4, 4, hi);

  auto out = Trax::smooth(g, box(2, 3, Trax::SmoothBoundary::Normalized, true));
  for (long j = 0; j < 9; j++)
    for (long i = 0; i < 9; i++)
    {
      const float v = (*out)(i, j);
      BOOST_CHECK(v >= lo - 1e-4F && v <= hi + 1e-4F);
    }
  // The peak must be attenuated (energy spread to neighbours).
  BOOST_CHECK((*out)(4, 4) < hi);
}

BOOST_AUTO_TEST_CASE(known_1d_normalized_average)
{
  BOOST_TEST_MESSAGE("+ [hand-checked single-pass radius-1 average on a ramp row]");
  // One row of a 5x2 grid set to a ramp 0,1,2,3,4; second row identical so the
  // y-pass is a no-op (averaging equal rows). radius=1, passes=1, Normalized.
  auto g = make_grid(5, 2, 0.0F);
  for (long j = 0; j < 2; j++)
    for (long i = 0; i < 5; i++)
      g->set(i, j, static_cast<float>(i));

  auto out = Trax::smooth(g, box(1, 1, Trax::SmoothBoundary::Normalized, true));

  // Edge i=0: mean(0,1)=0.5 ; interior i=1: mean(0,1,2)=1 ; i=2:2 ; i=3:3 ; edge i=4: mean(3,4)=3.5
  const float expected[5] = {0.5F, 1.0F, 2.0F, 3.0F, 3.5F};
  for (long i = 0; i < 5; i++)
    BOOST_CHECK_CLOSE((*out)(i, 0), expected[i], 1e-3);
}

BOOST_AUTO_TEST_CASE(periodic_x_wraps)
{
  BOOST_TEST_MESSAGE("+ [periodic x smoothing wraps across the antimeridian]");
  // 4 distinct columns, row = [1,0,0,0]; two identical rows so the y-pass is a
  // no-op. radius=1, single pass, Normalized.
  auto g = make_periodic(4, 2, 4);
  for (long j = 0; j < 2; j++)
  {
    g->set(0, j, 1.0F);
    g->set(1, j, 0.0F);
    g->set(2, j, 0.0F);
    g->set(3, j, 0.0F);
  }
  auto out = Trax::smooth(g, box(1, 1, Trax::SmoothBoundary::Normalized, true));

  // col0 window wraps to include col3: mean(col3,col0,col1)=mean(0,1,0)=1/3
  BOOST_CHECK_CLOSE((*out)(0, 0), 1.0F / 3.0F, 1e-3);
  // col3 window wraps to include col0: mean(col2,col3,col0)=mean(0,0,1)=1/3
  BOOST_CHECK_CLOSE((*out)(3, 0), 1.0F / 3.0F, 1e-3);

  // Without periodicity the same data smooths differently at the edges.
  auto g2 = make_grid(4, 2, 0.0F);
  for (long j = 0; j < 2; j++)
    g2->set(0, j, 1.0F);
  auto edge = Trax::smooth(g2, box(1, 1, Trax::SmoothBoundary::Normalized, true));
  BOOST_CHECK_CLOSE((*edge)(0, 0), 0.5F, 1e-3);  // mean(col0,col1)=mean(1,0)
  BOOST_CHECK_CLOSE((*edge)(3, 0), 0.0F, 1e-3);  // mean(col2,col3)=0
}

BOOST_AUTO_TEST_CASE(periodic_duplicate_wrap_column)
{
  BOOST_TEST_MESSAGE("+ [a duplicated wrap column is not double-counted]");
  // width 5, period 4: column 4 duplicates column 0.
  auto g = make_periodic(5, 2, 4);
  for (long j = 0; j < 2; j++)
  {
    g->set(0, j, 1.0F);
    g->set(1, j, 0.0F);
    g->set(2, j, 0.0F);
    g->set(3, j, 0.0F);
    g->set(4, j, 1.0F);  // duplicate of column 0
  }
  auto out = Trax::smooth(g, box(1, 1, Trax::SmoothBoundary::Normalized, true));

  // col0 wraps with col3: mean(0,1,0)=1/3, and the duplicate col4 must equal col0
  BOOST_CHECK_CLOSE((*out)(0, 0), 1.0F / 3.0F, 1e-3);
  BOOST_CHECK_CLOSE((*out)(4, 0), (*out)(0, 0), 1e-3);
}

BOOST_AUTO_TEST_CASE(geometry_is_shared)
{
  BOOST_TEST_MESSAGE("+ [the smoothed grid reuses the source coordinates and dimensions]");
  auto g = make_grid(6, 4, 1.0F);
  auto out = Trax::smooth(g, box(1, 2, Trax::SmoothBoundary::Normalized, true));
  BOOST_CHECK_EQUAL(out->width(), g->width());
  BOOST_CHECK_EQUAL(out->height(), g->height());
  for (long j = 0; j < 4; j++)
    for (long i = 0; i < 6; i++)
    {
      BOOST_CHECK_CLOSE(out->x(i, j), g->x(i, j), 1e-6);
      BOOST_CHECK_CLOSE(out->y(i, j), g->y(i, j), 1e-6);
    }
}

BOOST_AUTO_TEST_CASE(buffergrid_delegates_out_of_range_reads)
{
  BOOST_TEST_MESSAGE("+ [the smoothed grid delegates out-of-range reads to the source]");
  // A source whose bbox extends past its value dimensions must keep its
  // padding/virtual-cell behaviour: the BufferGrid may not index past its
  // value buffer for those cells, it must defer to the source.
  auto g = std::make_shared<PaddedTestGrid>(5, 5);
  for (long j = 0; j < 5; j++)
    for (long i = 0; i < 5; i++)
      g->set(i, j, 10.0F);

  auto out = Trax::smooth(g, box(1, 1, Trax::SmoothBoundary::Normalized, true));

  // In-range cells carry the smoothed value (all 10 -> 10).
  BOOST_CHECK_CLOSE((*out)(2, 2), 10.0F, 1e-3);
  // Out-of-range cells are delegated to the source sentinel, not read OOB.
  BOOST_CHECK_CLOSE((*out)(-1, 0), PaddedTestGrid::kPad, 1e-3);
  BOOST_CHECK_CLOSE((*out)(5, 0), PaddedTestGrid::kPad, 1e-3);
  BOOST_CHECK_CLOSE((*out)(0, 5), PaddedTestGrid::kPad, 1e-3);
  BOOST_CHECK_CLOSE((*out)(0, -1), PaddedTestGrid::kPad, 1e-3);
}

// --------------------------------------------------------------------------
// Median
// --------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(median_removes_isolated_spike)
{
  BOOST_TEST_MESSAGE("+ [median erases an isolated spike instead of spreading it]");
  // A single tall spike on a flat field: its window is dominated by the
  // background, so the median replaces it outright (a box filter would instead
  // smear the spike's energy across the neighbourhood).
  auto g = make_grid(5, 5, 10.0F);
  g->set(2, 2, 100.0F);

  auto out = Trax::smooth(g, median(1, 1, Trax::SmoothBoundary::Normalized, true));
  for (long j = 0; j < 5; j++)
    for (long i = 0; i < 5; i++)
      BOOST_CHECK_CLOSE((*out)(i, j), 10.0F, 1e-3);
}

BOOST_AUTO_TEST_CASE(median_preserves_step_edge)
{
  BOOST_TEST_MESSAGE("+ [median keeps a step edge sharp, inventing no intermediate values]");
  // Left half 0, right half 10. Every window is a majority of one side, so no
  // output value ever lands strictly between the two levels.
  auto g = make_grid(6, 3, 0.0F);
  for (long j = 0; j < 3; j++)
    for (long i = 3; i < 6; i++)
      g->set(i, j, 10.0F);

  auto out = Trax::smooth(g, median(1, 1, Trax::SmoothBoundary::Normalized, true));
  for (long j = 0; j < 3; j++)
    for (long i = 0; i < 6; i++)
    {
      const float v = (*out)(i, j);
      BOOST_CHECK(std::abs(v - 0.0F) < 1e-3F || std::abs(v - 10.0F) < 1e-3F);  // never in between
      BOOST_CHECK_CLOSE(v, (i < 3 ? 0.0F : 10.0F), 1e-3);  // step stays in place
    }
}

BOOST_AUTO_TEST_CASE(median_nan_is_isolated)
{
  BOOST_TEST_MESSAGE("+ [median preserves the missing footprint and ignores NaN neighbours]");
  auto g = make_grid(5, 5, 10.0F);
  g->set(2, 2, std::numeric_limits<float>::quiet_NaN());

  auto kept = Trax::smooth(g, median(1, 1, Trax::SmoothBoundary::Normalized, true));
  BOOST_CHECK(std::isnan((*kept)(2, 2)));
  for (long j = 0; j < 5; j++)
    for (long i = 0; i < 5; i++)
      if (!(i == 2 && j == 2))
        BOOST_CHECK_CLOSE((*kept)(i, j), 10.0F, 1e-3);

  // Without preserve_missing the hole is filled from the valid neighbours.
  auto filled = Trax::smooth(g, median(1, 1, Trax::SmoothBoundary::Normalized, false));
  BOOST_CHECK_CLOSE((*filled)(2, 2), 10.0F, 1e-3);
}

BOOST_AUTO_TEST_CASE(median_periodic_duplicate_wrap_column)
{
  BOOST_TEST_MESSAGE("+ [median wraps in x and keeps a duplicate wrap column consistent]");
  // width 5, period 4: column 4 duplicates column 0 and must match it on output.
  auto g = make_periodic(5, 3, 4);
  for (long j = 0; j < 3; j++)
  {
    g->set(0, j, 1.0F);
    g->set(1, j, 5.0F);
    g->set(2, j, 9.0F);
    g->set(3, j, 5.0F);
    g->set(4, j, 1.0F);  // duplicate of column 0
  }
  auto out = Trax::smooth(g, median(1, 1, Trax::SmoothBoundary::Normalized, true));
  for (long j = 0; j < 3; j++)
    BOOST_CHECK_CLOSE((*out)(4, j), (*out)(0, j), 1e-3);
}

// --------------------------------------------------------------------------
// Morphology
// --------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(morphology_open_preserves_plateau)
{
  BOOST_TEST_MESSAGE(
      "+ [opening preserves a peak broader than the structuring element, value intact]");
  // A 3x3 plateau of 100 equals the (2r+1)x(2r+1) element for r=1, so opening
  // returns it unchanged at full magnitude -- the property a box blur lacks.
  auto g = make_grid(7, 7, 0.0F);
  for (long j = 2; j <= 4; j++)
    for (long i = 2; i <= 4; i++)
      g->set(i, j, 100.0F);

  auto out =
      Trax::smooth(g, morph(1, 1, Trax::MorphologyOp::Open, Trax::SmoothBoundary::Replicate, true));
  for (long j = 0; j < 7; j++)
    for (long i = 0; i < 7; i++)
    {
      const float expected = (i >= 2 && i <= 4 && j >= 2 && j <= 4) ? 100.0F : 0.0F;
      BOOST_CHECK_CLOSE((*out)(i, j), expected, 1e-3);
    }
}

BOOST_AUTO_TEST_CASE(morphology_open_removes_bright_spike)
{
  BOOST_TEST_MESSAGE("+ [opening removes a bright feature smaller than the element]");
  auto g = make_grid(7, 7, 0.0F);
  g->set(3, 3, 100.0F);  // single-cell spike, smaller than the 3x3 element

  auto out =
      Trax::smooth(g, morph(1, 1, Trax::MorphologyOp::Open, Trax::SmoothBoundary::Replicate, true));
  for (long j = 0; j < 7; j++)
    for (long i = 0; i < 7; i++)
      BOOST_CHECK_CLOSE((*out)(i, j), 0.0F, 1e-3);
}

BOOST_AUTO_TEST_CASE(morphology_close_fills_pit)
{
  BOOST_TEST_MESSAGE("+ [closing fills a dark pit smaller than the element]");
  auto g = make_grid(7, 7, 100.0F);
  g->set(3, 3, 0.0F);  // single-cell pit

  auto out = Trax::smooth(
      g, morph(1, 1, Trax::MorphologyOp::Close, Trax::SmoothBoundary::Replicate, true));
  for (long j = 0; j < 7; j++)
    for (long i = 0; i < 7; i++)
      BOOST_CHECK_CLOSE((*out)(i, j), 100.0F, 1e-3);
}

BOOST_AUTO_TEST_CASE(morphology_no_overshoot_and_constant_preserved)
{
  BOOST_TEST_MESSAGE("+ [open-close stays within range and leaves a constant field untouched]");
  // Constant field is a fixed point of every morphological operation.
  auto cg = make_grid(6, 5, 7.0F);
  auto cout = Trax::smooth(
      cg, morph(2, 1, Trax::MorphologyOp::OpenClose, Trax::SmoothBoundary::Normalized, true));
  for (long j = 0; j < 5; j++)
    for (long i = 0; i < 6; i++)
      BOOST_CHECK_CLOSE((*cout)(i, j), 7.0F, 1e-3);

  // Open-close never produces a value outside the input range.
  auto g = make_grid(9, 9, 3.0F);
  g->set(4, 4, 50.0F);
  g->set(2, 6, -20.0F);
  auto out = Trax::smooth(
      g, morph(1, 2, Trax::MorphologyOp::OpenClose, Trax::SmoothBoundary::Normalized, true));
  for (long j = 0; j < 9; j++)
    for (long i = 0; i < 9; i++)
    {
      const float v = (*out)(i, j);
      BOOST_CHECK(v >= -20.0F - 1e-3F && v <= 50.0F + 1e-3F);
    }
}

BOOST_AUTO_TEST_CASE(morphology_nan_is_isolated)
{
  BOOST_TEST_MESSAGE("+ [morphology preserves the missing footprint without poisoning neighbours]");
  auto g = make_grid(5, 5, 10.0F);
  g->set(2, 2, std::numeric_limits<float>::quiet_NaN());

  auto out = Trax::smooth(
      g, morph(1, 1, Trax::MorphologyOp::OpenClose, Trax::SmoothBoundary::Normalized, true));
  BOOST_CHECK(std::isnan((*out)(2, 2)));
  for (long j = 0; j < 5; j++)
    for (long i = 0; i < 5; i++)
      if (!(i == 2 && j == 2))
        BOOST_CHECK_CLOSE((*out)(i, j), 10.0F, 1e-3);
}

BOOST_AUTO_TEST_CASE(morphology_periodic_duplicate_wrap_column)
{
  BOOST_TEST_MESSAGE("+ [morphology wraps in x and keeps a duplicate wrap column consistent]");
  // width 5, period 4: column 4 duplicates column 0 and must match it on output.
  auto g = make_periodic(5, 3, 4);
  for (long j = 0; j < 3; j++)
  {
    g->set(0, j, 8.0F);
    g->set(1, j, 2.0F);
    g->set(2, j, 0.0F);
    g->set(3, j, 2.0F);
    g->set(4, j, 8.0F);  // duplicate of column 0
  }
  auto out = Trax::smooth(
      g, morph(1, 1, Trax::MorphologyOp::OpenClose, Trax::SmoothBoundary::Normalized, true));
  for (long j = 0; j < 3; j++)
    BOOST_CHECK_CLOSE((*out)(4, j), (*out)(0, j), 1e-3);
}

// --------------------------------------------------------------------------
// Savitzky-Golay (legacy method)
// --------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(savgol_inactive_returns_same_grid)
{
  BOOST_TEST_MESSAGE("+ [degree 0 is inactive and returns the source grid unchanged]");
  auto in = make_grid(5, 5, 1.0F);
  BOOST_CHECK(Trax::smooth(in, savgol(2, 0)) == in);  // no degree -> no work
}

BOOST_AUTO_TEST_CASE(savgol_constant_field_is_preserved)
{
  BOOST_TEST_MESSAGE("+ [every Savitzky-Golay kernel reproduces a constant field exactly]");
  // The integer kernels are normalized so their coefficients sum to the
  // denominator; a constant field is therefore a fixed point for any (size,
  // degree), including across the mirror boundary.
  auto g = make_grid(9, 9, 42.0F);
  for (int size = 1; size <= 3; ++size)
    for (int degree = 1; degree <= 5; ++degree)
    {
      auto out = Trax::smooth(g, savgol(size, degree));
      for (long j = 0; j < 9; j++)
        for (long i = 0; i < 9; i++)
          BOOST_CHECK_CLOSE((*out)(i, j), 42.0F, 1e-3);
    }
}

BOOST_AUTO_TEST_CASE(savgol_degree1_is_box_average)
{
  BOOST_TEST_MESSAGE("+ [degree 1 reduces to a plain 3x3 box average]");
  // Flat zero field with a single 9 at the centre. A degree-1, size-1 kernel is
  // all ones over 9 cells, so the spike's energy spreads to 9/9 = 1 across its
  // 3x3 neighbourhood and cells whose window misses the centre stay 0.
  auto g = make_grid(5, 5, 0.0F);
  g->set(2, 2, 9.0F);

  auto out = Trax::smooth(g, savgol(1, 1));
  BOOST_CHECK_CLOSE((*out)(2, 2), 1.0F, 1e-3);  // centre averaged down
  BOOST_CHECK_CLOSE((*out)(1, 1), 1.0F, 1e-3);  // diagonal neighbour sees the spike
  BOOST_CHECK_CLOSE((*out)(0, 0), 0.0F, 1e-3);  // window does not reach the spike
}

BOOST_AUTO_TEST_CASE(savgol_rejects_windows_touching_nan)
{
  BOOST_TEST_MESSAGE("+ [a NaN centre stays NaN; a window touching NaN keeps its original value]");
  // Background 10, a 100 spike at (1,1), and a NaN at (2,2). The spike's 3x3
  // window includes the NaN, so it is rejected and the original 100 is kept
  // (a fill would have averaged it down) -- the distinctive legacy behaviour.
  auto g = make_grid(5, 5, 10.0F);
  g->set(1, 1, 100.0F);
  g->set(2, 2, std::numeric_limits<float>::quiet_NaN());

  auto out = Trax::smooth(g, savgol(1, 1));
  BOOST_CHECK(std::isnan((*out)(2, 2)));          // NaN centre preserved
  BOOST_CHECK_CLOSE((*out)(1, 1), 100.0F, 1e-3);  // window touched NaN -> original kept
  BOOST_CHECK_CLOSE((*out)(0, 4), 10.0F, 1e-3);   // far from NaN -> smoothed constant
}

BOOST_AUTO_TEST_CASE(savgol_unsupported_kernel_passes_through)
{
  BOOST_TEST_MESSAGE("+ [a (size,degree) with no kernel returns the values unchanged]");
  // The 3x3 window only defines degrees 1 and 2; degree 3 has no kernel, so the
  // legacy filter (and this port) leaves the data untouched.
  auto g = make_grid(5, 5, 0.0F);
  g->set(2, 2, 9.0F);

  auto out = Trax::smooth(g, savgol(1, 3));
  BOOST_CHECK_CLOSE((*out)(2, 2), 9.0F, 1e-3);  // spike intact
  BOOST_CHECK_CLOSE((*out)(1, 1), 0.0F, 1e-3);  // background intact
}
