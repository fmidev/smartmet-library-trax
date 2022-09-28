#include "Geos.h"
#include <boost/test/included/unit_test.hpp>
#include <geos/geom/Geometry.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/PrecisionModel.h>
#include <geos/io/WKTWriter.h>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Trax::Geos tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(to_geos_geom)
{
  geos::io::WKTWriter writer;

  namespace gg = geos::geom;

  const gg::PrecisionModel pm(1.0);  // fixed precision, one decimal
  const gg::GeometryFactory::Ptr factory = gg::GeometryFactory::create(&pm);

  BOOST_TEST_MESSAGE("+ [Trax::Geos::to_geos_geom]");

  {
    Trax::GeometryCollection geom;

    BOOST_CHECK_EQUAL(writer.write(Trax::to_geos_geom(geom, factory).get()),
                      "GEOMETRYCOLLECTION EMPTY");

    geom.add(Trax::Polygon({{0, 0, 0, 4, 4, 4, 4, 0, 0, 0}}));
    BOOST_CHECK_EQUAL(writer.write(Trax::to_geos_geom(geom, factory).get()),
                      "POLYGON ((0 0, 0 4, 4 4, 4 0, 0 0))");

    geom.add(Trax::Polygon({{5, 5, 5, 6, 6, 6, 6, 5, 5, 5}}));
    BOOST_CHECK_EQUAL(writer.write(Trax::to_geos_geom(geom, factory).get()),
                      "MULTIPOLYGON (((0 0, 0 4, 4 4, 4 0, 0 0)), ((5 5, 5 6, 6 6, 6 5, 5 5)))");

    geom.add(Trax::Polyline({1, 1, 2, 2}));
    BOOST_CHECK_EQUAL(
        writer.write(Trax::to_geos_geom(geom, factory).get()),
        "GEOMETRYCOLLECTION (MULTIPOLYGON (((0 0, 0 4, 4 4, 4 0, 0 0)), ((5 5, 5 6, 6 6, 6 5, 5 "
        "5))), LINESTRING (1 1, 2 2))");
  }

  {
    Trax::GeometryCollection geom;

    geom.add(Trax::Polyline({1, 1, 2, 2}));
    BOOST_CHECK_EQUAL(writer.write(Trax::to_geos_geom(geom, factory).get()),
                      "LINESTRING (1 1, 2 2)");

    geom.add(Trax::Polyline({5, 5, 6, 6}));
    BOOST_CHECK_EQUAL(writer.write(Trax::to_geos_geom(geom, factory).get()),
                      "MULTILINESTRING ((1 1, 2 2), (5 5, 6 6))");
  }
}
