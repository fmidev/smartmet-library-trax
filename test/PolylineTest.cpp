#include "Polyline.h"
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Trax::Polyline tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(wkt)
{
  BOOST_TEST_MESSAGE("+ [Trax::Polyline::wkt]");

  Trax::Polyline line1{0, 1, 2, 3};
  BOOST_CHECK_EQUAL(line1.wkt(), "LINESTRING (0 1,2 3)");

  Trax::Polyline line2{0, 1, 2, 3, 4, 5};
  BOOST_CHECK_EQUAL(line2.wkt(), "LINESTRING (0 1,2 3,4 5)");
}

BOOST_AUTO_TEST_CASE(closed)
{
  BOOST_TEST_MESSAGE("+ [Trax::Polyline::closed]");
  Trax::Polyline line1{0, 0, 0, 1, 1, 1, 0, 0};
  BOOST_CHECK(line1.closed() == true);

  Trax::Polyline line2{0, 0, 0, 1, 1, 1, 0, 1};
  BOOST_CHECK(line2.closed() == false);

  Trax::Polyline line3{0, 0, 0, 1, 1, 1, 1, 0};
  BOOST_CHECK(line3.closed() == false);
}

BOOST_AUTO_TEST_CASE(clockwise)
{
  BOOST_TEST_MESSAGE("+ [Trax::Polyline::clockwise]");
  Trax::Polyline line1{0, 0, 0, 1, 1, 1, 0, 0};
  BOOST_CHECK(line1.clockwise() == true);

  Trax::Polyline line2{0, 0, 1, 0, 1, 1, 0, 0};
  BOOST_CHECK(line2.clockwise() == false);
}

BOOST_AUTO_TEST_CASE(reverse)
{
  BOOST_TEST_MESSAGE("+ [Trax::Polyline::reverse]");
  Trax::Polyline line1{0, 1, 2, 3};
  line1.reverse();
  BOOST_CHECK_EQUAL(line1.wkt(), "LINESTRING (2 3,0 1)");

  Trax::Polyline line2{0, 1, 2, 3, 4, 5};
  line2.reverse();
  BOOST_CHECK_EQUAL(line2.wkt(), "LINESTRING (4 5,2 3,0 1)");
}

BOOST_AUTO_TEST_CASE(normalize)
{
  BOOST_TEST_MESSAGE("+ [Trax::Polyline::normalize]");

  Trax::Polyline line1{0, 0, 1, 1};
  BOOST_CHECK_EQUAL(line1.normalize().wkt(), "LINESTRING (0 0,1 1)");

  Trax::Polyline line2{0, 0, 0, 1, 1, 1, 1, 0, 0, 0};
  BOOST_CHECK_EQUAL(line2.normalize().wkt(), "LINESTRING (0 0,0 1,1 1,1 0,0 0)");

  Trax::Polyline line3{0, 1, 1, 1, 1, 0, 0, 0, 0, 1};
  BOOST_CHECK_EQUAL(line3.normalize().wkt(), "LINESTRING (0 0,0 1,1 1,1 0,0 0)");

  Trax::Polyline line4{1, 1, 1, 0, 0, 0, 0, 1, 1, 1};
  BOOST_CHECK_EQUAL(line4.normalize().wkt(), "LINESTRING (0 0,0 1,1 1,1 0,0 0)");
}
