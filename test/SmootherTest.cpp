#include "Smoother.h"
#include "SmoothOptions.h"
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
