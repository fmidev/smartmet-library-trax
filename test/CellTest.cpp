#include "Cell.h"
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Trax::Cell tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(is_saddle)
{
  BOOST_TEST_MESSAGE("+ [Trax::is_saddle(Cell)]");

  const float x0 = 0, y0 = 0, x1 = 0, y1 = 1, x2 = 1, y2 = 1, x3 = 1, y3 = 0;
  const int i = 0, j = 0;

  Trax::Cell cell1(x0, y0, 0, x1, y1, 1, x2, y2, 2, x3, y3, 3, i, j);
  BOOST_CHECK(Trax::is_saddle(cell1) == false);

  Trax::Cell cell2(x0, y0, 0, x1, y1, 1, x2, y2, 0, x3, y3, 1, i, j);
  BOOST_CHECK(Trax::is_saddle(cell2) == true);

  Trax::Cell cell3(x0, y0, 0, x1, y1, 0, x2, y2, 0, x3, y3, 0, i, j);
  BOOST_CHECK(Trax::is_saddle(cell3) == false);

  Trax::Cell cell4(x0, y0, 0, x1, y1, 1, x2, y2, 0, x3, y3, 2, i, j);
  BOOST_CHECK(Trax::is_saddle(cell4) == true);

  Trax::Cell cell5(x0, y0, 0, x1, y1, 1, x2, y2, 0, x3, y3, -1, i, j);
  BOOST_CHECK(Trax::is_saddle(cell5) == false);
}

BOOST_AUTO_TEST_CASE(minmax)
{
  BOOST_TEST_MESSAGE("+ [Trax::minmax(Cell)]");

  const float x0 = 0, y0 = 0, x1 = 0, y1 = 1, x2 = 1, y2 = 1, x3 = 1, y3 = 0;
  const int i = 0, j = 0;

  Trax::Cell cell1(x0, y0, 0, x1, y1, 1, x2, y2, 2, x3, y3, 3, i, j);
  BOOST_CHECK(Trax::minmax(cell1) == std::make_pair(0.0F, 3.0F));

  Trax::Cell cell2(x0, y0, 0, x1, y1, 1, x2, y2, 0, x3, y3, 1, i, j);
  BOOST_CHECK(Trax::minmax(cell2) == std::make_pair(0.0F, 1.0F));

  Trax::Cell cell3(x0, y0, 0, x1, y1, 0, x2, y2, 0, x3, y3, 0, i, j);
  BOOST_CHECK(Trax::minmax(cell3) == std::make_pair(0.0F, 0.0F));

  Trax::Cell cell4(x0, y0, 0, x1, y1, 1, x2, y2, 0, x3, y3, 2, i, j);
  BOOST_CHECK(Trax::minmax(cell4) == std::make_pair(0.0F, 2.0F));

  Trax::Cell cell5(x0, y0, 0, x1, y1, 1, x2, y2, 0, x3, y3, -1, i, j);
  BOOST_CHECK(Trax::minmax(cell5) == std::make_pair(-1.0F, 1.0F));
}
