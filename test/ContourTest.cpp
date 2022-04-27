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

double parse_value(const std::string& str)
try
{
  if (str == "-")
    return std::numeric_limits<double>::quiet_NaN();
  if (str == "-inf")
    return -std::numeric_limits<double>::infinity();
  if (str == "inf")
    return std::numeric_limits<double>::infinity();
  return std::stod(str);
}
catch (std::exception& e)
{
  std::cerr << "Invalid number: " << str << "\n";
  throw;
}

void run_file_tests(const std::string& filename)
{
  // Default contouring settings
  Trax::Contour contourer;

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
      script += "# " + command + "\n";
    }
    else if (command == "interpolation")
    {
      in >> command;
      contourer.interpolation(Trax::to_interpolation_type(command));
      script += "interpolation " + command + "\n";
    }
    else if (command == "bbox")
    {
      in >> x1 >> y1 >> x2 >> y2;
      script += fmt::format("bbox {} {} {} {}\n", x1, y1, x2, y2);
    }
    else if (command == "shell")
    {
      double mincoord;
      double maxcoord;
      in >> mincoord >> maxcoord;
      contourer.bbox(mincoord, maxcoord);
      script += fmt::format("shell {} {}\n", mincoord, maxcoord);
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
      double value;
      in >> value;
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

BOOST_AUTO_TEST_CASE(midpoint_2x2)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband midpoint 2x2]");
  run_file_tests("data/midpoint_2x2.txt");
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

// #define RUN_REALLY_BIG_TESTS 1
#ifdef RUN_REALLY_BIG_TESTS
BOOST_AUTO_TEST_CASE(isoband_4x3)
{
  BOOST_TEST_MESSAGE("+ [Trax::Builder::isoband 4x3]");
  run_file_tests("data/isoband_4x3.txt");
}
#endif
