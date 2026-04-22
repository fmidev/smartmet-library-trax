#include "Contour.h"
#include "Geos.h"
#include "IsobandLimits.h"
#include "IsolineValues.h"
#include "TestGrid.h"
#include <boost/test/included/unit_test.hpp>
#include <geos/geom/GeometryFactory.h>
#include <geos/operation/valid/IsValidOp.h>
#include <limits>
#include <string>

const bool WRITE_SCRIPT = false;

using namespace boost::unit_test;
// In place trim
void trim(std::string& value)
{
  const char* spaces = " \t\n\v\f\r";
  value.erase(value.find_last_not_of(spaces) + 1);
  value.erase(0, value.find_first_not_of(spaces));
}

std::string validate(const Trax::GeometryCollection& geom)
{
  const auto factory = geos::geom::GeometryFactory::create();

  auto g = Trax::to_geos_geom(geom, factory);
  geos::operation::valid::IsValidOp validator(g.get());
  if (validator.isValid())
    return {};

  return validator.getValidationError()->toString();
}

float parse_value(const std::string& str)
try
{
  if (str == "-")
    return std::numeric_limits<float>::quiet_NaN();
  if (str == "-inf")
    return -std::numeric_limits<float>::infinity();
  if (str == "inf")
    return std::numeric_limits<float>::infinity();
  return std::stof(str);
}
catch (std::exception& e)
{
  std::cerr << "Invalid number: " << str << "\n";
  throw;
}

void run_file_tests(const std::string& filename, int threads = 1)
{
  // Default contouring settings
  Trax::Contour contourer;
  contourer.threads(threads);

  std::unique_ptr<Trax::TestGrid> grid;
  double x1 = 0;
  double y1 = 0;
  double x2 = 1;
  double y2 = 1;

  // Generate a script for the test to be able to patch a large number of tests simultaneously

  std::string script;

  std::ifstream in(filename);
  if (!in)
    throw std::runtime_error("Failed to open '" + filename + "' for reading");

  std::string command;
  while (in >> command)
  {
    if (command[0] == '#')
    {
      std::getline(in, command);
      script += "#" + command + "\n";
    }
    else if (command == "interpolation")
    {
      in >> command;
      contourer.interpolation(Trax::to_interpolation_type(command));
      script += "interpolation " + command + "\n";
    }
    else if (command == "desliver")
    {
      in >> command;
      contourer.desliver(parse_value(command) != 0);
    }
    else if (command == "bbox")
    {
      in >> x1 >> y1 >> x2 >> y2;
      script += fmt::format("bbox {} {} {} {}\n", x1, y1, x2, y2);
    }
    else if (command == "grid")
    {
      int nx = 0;
      int ny = 0;
      in >> nx >> ny;
      if (nx < 2 || ny < 2)
        throw std::runtime_error("Invalid grid dimensions: " + std::to_string(nx) + ',' +
                                 std::to_string(ny));

      grid.reset(new Trax::TestGrid(nx, ny, x1, y1, x2, y2));

      int row = ny - 1;
      int col = 0;
      int n = nx * ny;
      std::string svalue;
      while (n-- > 0 && in >> svalue)
      {
        auto value = parse_value(svalue);
        grid->set(col++, row, value);
        if (col >= nx)
        {
          col = 0;
          --row;
        }
      }
      if (n > 0)
        throw std::runtime_error("Invalid number of grid elements provided");

      // Update script output
      script += fmt::format("\ngrid {} {}\n", nx, ny);
      for (auto j = ny; j > 0; j--)
      {
        for (auto i = 0; i < nx; i++)
        {
          if (i > 0)
            script += ' ';
          script += fmt::format("{}", (*grid)(i, j - 1));
        }
        script += '\n';
      }
    }
    else if (command == "coords")
    {
      std::string sx, sy;
      double x, y;
      int nx = grid->width();
      int ny = grid->height();
      int n = nx * ny;
      int row = ny - 1;
      int col = 0;
      while (n-- > 0 && in >> sx >> sy)
      {
        x = parse_value(sx);
        y = parse_value(sy);
        grid->set(col++, row, x, y);
        if (col >= nx)
        {
          col = 0;
          --row;
        }
      }
    }
    else if (command == "isoband")
    {
      std::string slo, shi;
      in >> slo >> shi;
      auto lo = parse_value(slo);
      auto hi = parse_value(shi);
      Trax::IsobandLimits limits;
      limits.add(lo, hi);

      std::string wkt;
      std::getline(in, wkt);
      trim(wkt);

#if 0
      std::cout << "Grid of size " << grid->width() << 'x' << grid->height() << ":\n" + grid->dump("        ") << "\n";
#endif

      BOOST_TEST_INFO("Isoband: " << lo << "..." << hi);
      BOOST_TEST_INFO("BBOX: " << x1 << ',' << y1 << " ... " << x2 << ',' << y2);
      BOOST_TEST_INFO("Grid of size " << grid->width() << 'x' << grid->height()
                                      << ":\n" + grid->dump("        "));
      auto result = contourer.isobands(*grid, limits);
      auto result_wkt = result[0].normalize().wkt();
      BOOST_CHECK_EQUAL(result_wkt, wkt);

      auto msg = validate(result[0]);
      if (!msg.empty())
      {
        BOOST_TEST_INFO("Topology error: " << msg);
        BOOST_TEST_INFO("WKT: " << result_wkt);
        BOOST_TEST_INFO("Isoband: " << lo << "..." << hi);
        BOOST_TEST_INFO("BBOX: " << x1 << ',' << y1 << " ... " << x2 << ',' << y2);
        BOOST_TEST_INFO("Grid of size " << grid->width() << 'x' << grid->height()
                                        << ":\n" + grid->dump("        "));
        BOOST_CHECK(msg.empty());
      }

      script += fmt::format("isoband {} {} {}\n", lo, hi, result_wkt);
    }
    else if (command == "isoline")
    {
      std::string svalue;
      in >> svalue;
      auto value = parse_value(svalue);

      Trax::IsolineValues values;
      values.add(value);

      std::string wkt;
      std::getline(in, wkt);
      trim(wkt);

      BOOST_TEST_INFO("Isoline: " << value);
      BOOST_TEST_INFO("BBOX: " << x1 << ',' << y1 << " ... " << x2 << ',' << y2);
      BOOST_TEST_INFO("Grid of size " << grid->width() << 'x' << grid->height()
                                      << ":\n" + grid->dump("        "));
      auto result = contourer.isolines(*grid, values);
      auto result_wkt = result[0].normalize().wkt();
      BOOST_CHECK_EQUAL(result_wkt, wkt);

      auto msg = validate(result[0]);
      if (!msg.empty())
      {
        BOOST_TEST_INFO("Topology error: " << msg);
        BOOST_TEST_INFO("WKT: " << result_wkt);
        BOOST_TEST_INFO("Isoline: " << value);
        BOOST_TEST_INFO("BBOX: " << x1 << ',' << y1 << " ... " << x2 << ',' << y2);
        BOOST_TEST_INFO("Grid of size " << grid->width() << 'x' << grid->height()
                                        << ":\n" + grid->dump("        "));
        BOOST_CHECK(msg.empty());
      }

      script += fmt::format("isoline {} {}\n", value, result_wkt);
    }
    else if (command == "quit")
    {
      script += "quit\n";

      BOOST_TEST_MESSAGE("    quit command encountered");
      break;
    }
    else
      throw std::runtime_error("Unknown command '" + command + "' in '" + filename + "'");
  }

  if (WRITE_SCRIPT)
    // Use this if you need a bulk update of expected results
    std::cout << script;
}

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Trax::Contour tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(isoband_2x2)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband 2x2]");
  run_file_tests("data/isoband_2x2.txt");
}

BOOST_AUTO_TEST_CASE(isoband_2x2_missing)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband 2x2 missing data]");
  run_file_tests("data/isoband_2x2_missing.txt");
}

BOOST_AUTO_TEST_CASE(midpoint_2x2)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband midpoint 2x2]");
  run_file_tests("data/midpoint_2x2.txt");
}

BOOST_AUTO_TEST_CASE(midpoint_2x2_missing)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband midpoint 2x2 missing data]");
  run_file_tests("data/midpoint_2x2_missing.txt");
}

BOOST_AUTO_TEST_CASE(midpoint_4x3)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband midpoint 4x3]");
  run_file_tests("data/midpoint_4x3.txt");
}

BOOST_AUTO_TEST_CASE(isoband_3x3)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband 3x3]");
  run_file_tests("data/isoband_3x3.txt");
}

BOOST_AUTO_TEST_CASE(isoband_3x3_missing)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband 3x3 missing data]");
  run_file_tests("data/isoband_3x3_missing.txt");
}

BOOST_AUTO_TEST_CASE(isoband_3x3_inf)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband 3x3 inf limits]");
  run_file_tests("data/isoband_3x3_inf.txt");
}

BOOST_AUTO_TEST_CASE(isoband_4x4)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband 4x4]");
  run_file_tests("data/isoband_4x4.txt");
}

BOOST_AUTO_TEST_CASE(isoline_2x2)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoline 2x2]");
  run_file_tests("data/isoline_2x2.txt");
}

BOOST_AUTO_TEST_CASE(isoline_2x2_missing)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoline 2x2 missing data]");
  run_file_tests("data/isoline_2x2_missing.txt");
}

BOOST_AUTO_TEST_CASE(isoline_3x3)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoline 3x3]");
  run_file_tests("data/isoline_3x3.txt");
}

BOOST_AUTO_TEST_CASE(isoline_4x4)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoline 4x4]");
  run_file_tests("data/isoline_4x4.txt");
}

BOOST_AUTO_TEST_CASE(isoband_debug)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband debug cases");
  run_file_tests("data/isoband_debug.txt");
}

// Run all data files with a fixed thread count or with automatic thread selection.
// Results must be identical to the single-threaded runs above.

void run_all_file_tests(int threads)
{
  run_file_tests("data/isoband_2x2.txt", threads);
  run_file_tests("data/isoband_2x2_missing.txt", threads);
  run_file_tests("data/midpoint_2x2.txt", threads);
  run_file_tests("data/midpoint_2x2_missing.txt", threads);
  run_file_tests("data/midpoint_4x3.txt", threads);
  run_file_tests("data/isoband_3x3.txt", threads);
  run_file_tests("data/isoband_3x3_missing.txt", threads);
  run_file_tests("data/isoband_3x3_inf.txt", threads);
  run_file_tests("data/isoband_4x4.txt", threads);
  run_file_tests("data/isoline_2x2.txt", threads);
  run_file_tests("data/isoline_2x2_missing.txt", threads);
  run_file_tests("data/isoline_3x3.txt", threads);
  run_file_tests("data/isoline_4x4.txt", threads);
  run_file_tests("data/isoband_debug.txt", threads);
}

BOOST_AUTO_TEST_CASE(all_tests_4_threads)
{
  BOOST_TEST_MESSAGE("+ [Trax::Contour all tests with 4 threads]");
  run_all_file_tests(4);
}

BOOST_AUTO_TEST_CASE(all_tests_auto_threads)
{
  BOOST_TEST_MESSAGE("+ [Trax::Contour all tests with automatic thread selection]");
  run_all_file_tests(0);
}

// Radar-style test: a single peak of 10 surrounded by zeros. Linear interpolation
// lets the high value dominate (intersections land near the zero corners), while
// logarithmic interpolation pulls intersections towards the peak, shrinking the
// covered area. This verifies that the interpolation mode actually affects the
// output geometry for a classic radar precipitation pattern.
BOOST_AUTO_TEST_CASE(logarithmic_vs_linear_peak)
{
  BOOST_TEST_MESSAGE("+ [Trax::Contour logarithmic vs linear single-peak isoband]");

  // 3x3 grid on a 0..20 viewbox with a single peak of 10 in the center.
  // The four cells sharing the peak all have three zero corners and one 10 corner.
  const int nx = 3;
  const int ny = 3;
  Trax::TestGrid grid(nx, ny, 0.0, 0.0, 20.0, 20.0);
  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i)
      grid.set(i, j, 0.0f);
  grid.set(1, 1, 10.0f);

  // Contour the isoband [1, inf]: everything with value >= 1. The lo-limit of 1
  // is what makes linear and logarithmic behaviour diverge visibly (log1p(1)/log1p(10)
  // ~= 0.29 vs the linear 1/10 = 0.10).
  Trax::IsobandLimits limits;
  limits.add(1.0f, std::numeric_limits<float>::infinity());

  Trax::Contour linear_contourer;
  linear_contourer.interpolation(Trax::InterpolationType::Linear);
  auto linear_result = linear_contourer.isobands(grid, limits);

  Trax::Contour log_contourer;
  log_contourer.interpolation(Trax::InterpolationType::Logarithmic);
  auto log_result = log_contourer.isobands(grid, limits);

  BOOST_REQUIRE_EQUAL(linear_result.size(), 1u);
  BOOST_REQUIRE_EQUAL(log_result.size(), 1u);

  const auto factory = geos::geom::GeometryFactory::create();
  auto linear_geom = Trax::to_geos_geom(linear_result[0], factory);
  auto log_geom = Trax::to_geos_geom(log_result[0], factory);

  const double linear_area = linear_geom->getArea();
  const double log_area = log_geom->getArea();

  BOOST_TEST_INFO("Linear WKT : " << linear_result[0].wkt());
  BOOST_TEST_INFO("Log WKT    : " << log_result[0].wkt());
  BOOST_TEST_INFO("Linear area: " << linear_area);
  BOOST_TEST_INFO("Log area   : " << log_area);

  // Both contours must be non-empty and cover a real region.
  BOOST_CHECK_GT(linear_area, 0.0);
  BOOST_CHECK_GT(log_area, 0.0);

  // Logarithmic interpolation must produce a strictly smaller isoband than linear
  // for this single-peak-in-zeros case. The 0-surrounded peak should not dominate
  // as much under log interpolation.
  BOOST_CHECK_LT(log_area, linear_area);

  // The two geometries must differ. If logarithmic interpolation had no effect
  // (e.g. silently falling back to linear), the WKT strings would be identical.
  BOOST_CHECK_NE(linear_result[0].wkt(), log_result[0].wkt());

  // Analytical expectations for this specific grid:
  //   Linear  : diamond with corners (1,10), (10,1), (19,10), (10,19), area = 162
  //   Log     : diamond with corners (~2.89,10), (10,~2.89), ..., area ~= 101.24
  // Use loose bounds to stay robust against future minor refactors.
  BOOST_CHECK_CLOSE(linear_area, 162.0, 1.0);
  BOOST_CHECK_CLOSE(log_area, 101.24, 2.0);
}

// Bilinear subdivision replaces the straight diagonal of a corner-triangle cell
// with N-1 samples on the true bilinear level curve. On the single-peak grid the
// resulting isoband is the four-lobed bilinear region bounded by hyperbolas instead
// of a sharp diamond; its area is the same across subdivide values once every cell
// already sees the curve (the four cells contribute identical shapes by symmetry),
// so we verify (a) subdivide==0 reproduces the linear baseline exactly and
// (b) any subdivide >= 2 converges to the analytical bilinear area within tolerance.
BOOST_AUTO_TEST_CASE(subdivide_peak_bilinear_curve)
{
  BOOST_TEST_MESSAGE("+ [Trax::Contour subdivide single-peak bilinear curve]");

  const int nx = 3;
  const int ny = 3;
  Trax::TestGrid grid(nx, ny, 0.0, 0.0, 20.0, 20.0);
  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i)
      grid.set(i, j, 0.0f);
  grid.set(1, 1, 10.0f);

  Trax::IsobandLimits limits;
  limits.add(1.0f, std::numeric_limits<float>::infinity());

  auto area_for = [&](int n) {
    Trax::Contour c;
    c.interpolation(Trax::InterpolationType::Linear);
    c.subdivide(n);
    auto res = c.isobands(grid, limits);
    BOOST_REQUIRE_EQUAL(res.size(), 1u);
    const auto factory = geos::geom::GeometryFactory::create();
    return Trax::to_geos_geom(res[0], factory)->getArea();
  };

  // subdivide == 0 is the linear baseline (straight diagonal diamond, area == 162)
  const double base = area_for(0);
  BOOST_CHECK_CLOSE(base, 162.0, 1.0);

  // subdivide == 1 is a no-op by construction (one sub-segment, zero interior samples).
  BOOST_CHECK_EQUAL(area_for(1), base);

  // Analytical bilinear area per cell: integral over the unit square of
  // [f(u,v) > 1] where f(u,v) = 10 u (1-v)  is 0.9 + 0.1*ln(0.1) = 0.6697.
  // Four cells * 100 world units = 267.88.
  const double bilinear_ref = 4.0 * (0.9 + 0.1 * std::log(0.1)) * 100.0;

  // Densification adds samples that pull the straight line to the hyperbola.
  // Each step should bring the area closer to the analytical bilinear reference.
  const double a2 = area_for(2);
  const double a4 = area_for(4);
  const double a8 = area_for(8);
  const double a10 = area_for(10);

  BOOST_TEST_INFO("subdivide=0  area : " << base);
  BOOST_TEST_INFO("subdivide=2  area : " << a2);
  BOOST_TEST_INFO("subdivide=4  area : " << a4);
  BOOST_TEST_INFO("subdivide=8  area : " << a8);
  BOOST_TEST_INFO("subdivide=10 area : " << a10);
  BOOST_TEST_INFO("bilinear analytical area : " << bilinear_ref);

  // Monotone approach toward the bilinear reference (from below: the piecewise-linear
  // approximation undershoots the true area of the region bounded by the hyperbola).
  BOOST_CHECK_GT(a2, base);
  BOOST_CHECK_GT(a4, a2);
  BOOST_CHECK_GT(a8, a4);
  BOOST_CHECK_GE(a10, a8);
  BOOST_CHECK_LT(a10, bilinear_ref);

  // At subdivide=8 the piecewise-linear approximation is within a couple percent of
  // the analytical bilinear reference; at subdivide=10 even closer.
  BOOST_CHECK_CLOSE(a8, bilinear_ref, 3.0);
  BOOST_CHECK_CLOSE(a10, bilinear_ref, 2.0);
}

// #define RUN_REALLY_BIG_TESTS 1
#ifdef RUN_REALLY_BIG_TESTS
BOOST_AUTO_TEST_CASE(isoband_4x3)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband 4x3]");
  run_file_tests("data/isoband_4x3.txt");
}
#endif
