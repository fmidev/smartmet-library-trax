#include "GeometryCollection.h"
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Trax::GeometryCollection tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(wkt)
{
  BOOST_TEST_MESSAGE("+ [Trax::GeometryCollection::wkt]");

  {
    Trax::GeometryCollection geom;

    BOOST_CHECK_EQUAL(geom.wkt(), "GEOMETRYCOLLECTION EMPTY");

    geom.add(Trax::Polygon({{0, 0, 0, 4, 4, 4, 4, 0, 0, 0}}));
    BOOST_CHECK_EQUAL(geom.wkt(), "POLYGON ((0 0,0 4,4 4,4 0,0 0))");

    geom.add(Trax::Polygon({{5, 5, 5, 6, 6, 6, 6, 5, 5, 5}}));
    BOOST_CHECK_EQUAL(geom.wkt(), "MULTIPOLYGON (((0 0,0 4,4 4,4 0,0 0)),((5 5,5 6,6 6,6 5,5 5)))");

    geom.add(Trax::Polyline({1, 1, 2, 2}));
    BOOST_CHECK_EQUAL(
        geom.wkt(),
        "GEOMETRYCOLLECTION (MULTIPOLYGON (((0 0,0 4,4 4,4 0,0 0)),((5 5,5 6,6 6,6 5,5 5))),LINESTRING (1 1,2 2))");
  }

  {
    Trax::GeometryCollection geom;

    geom.add(Trax::Polyline({1, 1, 2, 2}));
    BOOST_CHECK_EQUAL(geom.wkt(), "LINESTRING (1 1,2 2)");

    geom.add(Trax::Polyline({5, 5, 6, 6}));
    BOOST_CHECK_EQUAL(geom.wkt(), "MULTILINESTRING ((1 1,2 2),(5 5,6 6))");
  }
}
