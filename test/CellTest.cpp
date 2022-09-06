#include "Cell.h"
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Cell tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(is_saddle)
{
  BOOST_TEST_MESSAGE("+ [is_saddle(Cell)]");

  using Trax::Cell;
  using GP = Trax::GridPoint;
  using Trax::is_saddle;

  const float x0 = 0, y0 = 0, x1 = 0, y1 = 1, x2 = 1, y2 = 1, x3 = 1, y3 = 0;
  const int i = 0, j = 0;

  Cell cell1(GP(x0, y0, 0), GP(x1, y1, 1), GP(x2, y2, 2), GP(x3, y3, 3), i, j);
  BOOST_CHECK(is_saddle(cell1) == false);

  Cell cell2(GP(x0, y0, 0), GP(x1, y1, 1), GP(x2, y2, 0), GP(x3, y3, 1), i, j);
  BOOST_CHECK(is_saddle(cell2) == true);

  Cell cell3(GP(x0, y0, 0), GP(x1, y1, 0), GP(x2, y2, 0), GP(x3, y3, 0), i, j);
  BOOST_CHECK(is_saddle(cell3) == false);

  Cell cell4(GP(x0, y0, 0), GP(x1, y1, 1), GP(x2, y2, 0), GP(x3, y3, 2), i, j);
  BOOST_CHECK(is_saddle(cell4) == true);

  Cell cell5(GP(x0, y0, 0), GP(x1, y1, 1), GP(x2, y2, 0), GP(x3, y3, -1), i, j);
  BOOST_CHECK(is_saddle(cell5) == false);
}

BOOST_AUTO_TEST_CASE(minmax)
{
  BOOST_TEST_MESSAGE("+ [minmax(Cell)]");

  using Trax::Cell;
  using GP = Trax::GridPoint;
  using Trax::minmax;

  const float x0 = 0, y0 = 0, x1 = 0, y1 = 1, x2 = 1, y2 = 1, x3 = 1, y3 = 0;
  const int i = 0, j = 0;

  Cell cell1(GP(x0, y0, 0), GP(x1, y1, 1), GP(x2, y2, 2), GP(x3, y3, 3), i, j);
  BOOST_CHECK(minmax(cell1) == std::make_pair(0.0F, 3.0F));

  Cell cell2(GP(x0, y0, 0), GP(x1, y1, 1), GP(x2, y2, 0), GP(x3, y3, 1), i, j);
  BOOST_CHECK(minmax(cell2) == std::make_pair(0.0F, 1.0F));

  Cell cell3(GP(x0, y0, 0), GP(x1, y1, 0), GP(x2, y2, 0), GP(x3, y3, 0), i, j);
  BOOST_CHECK(minmax(cell3) == std::make_pair(0.0F, 0.0F));

  Cell cell4(GP(x0, y0, 0), GP(x1, y1, 1), GP(x2, y2, 0), GP(x3, y3, 2), i, j);
  BOOST_CHECK(minmax(cell4) == std::make_pair(0.0F, 2.0F));

  Cell cell5(GP(x0, y0, 0), GP(x1, y1, 1), GP(x2, y2, 0), GP(x3, y3, -1), i, j);
  BOOST_CHECK(minmax(cell5) == std::make_pair(-1.0F, 1.0F));
}
