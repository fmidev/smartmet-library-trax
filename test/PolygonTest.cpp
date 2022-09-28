#include "Polygon.h"
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Trax::Polygon tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(wkt)
{
  BOOST_TEST_MESSAGE("+ [Trax::Polygon::wkt]");

  Trax::Polygon poly1{{0, 0, 0, 4, 4, 4, 4, 0, 0, 0}};
  BOOST_CHECK_EQUAL(poly1.wkt(), "POLYGON ((0 0,0 4,4 4,4 0,0 0))");

  Trax::Polygon poly2{{0, 0, 0, 4, 4, 4, 4, 0, 0, 0}, {1, 1, 1, 2, 2, 2, 2, 1, 1, 1}};
  BOOST_CHECK_EQUAL(poly2.wkt(), "POLYGON ((0 0,0 4,4 4,4 0,0 0),(1 1,1 2,2 2,2 1,1 1))");

  Trax::Polygon poly3{{0, 0, 0, 4, 4, 4, 4, 0, 0, 0},
                      {1, 1, 1, 2, 2, 2, 2, 1, 1, 1},
                      {2, 2, 2, 3, 3, 3, 3, 2, 2, 2}};
  BOOST_CHECK_EQUAL(poly3.wkt(),
                    "POLYGON ((0 0,0 4,4 4,4 0,0 0),(1 1,1 2,2 2,2 1,1 1),(2 2,2 3,3 3,3 2,2 2))");
}

BOOST_AUTO_TEST_CASE(hole)
{
  BOOST_TEST_MESSAGE("+ [Trax::Polygon::hole]");
  Trax::Polygon poly1{{0, 0, 0, 4, 4, 4, 4, 0, 0, 0}};
  poly1.hole({1, 1, 2, 2});
  BOOST_CHECK_EQUAL(poly1.wkt(), "POLYGON ((0 0,0 4,4 4,4 0,0 0),(1 1,2 2))");
}
