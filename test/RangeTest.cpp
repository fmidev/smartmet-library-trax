#include "Range.h"
#include <boost/test/included/unit_test.hpp>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Trax::Range tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(adjust)
{
  BOOST_TEST_MESSAGE("+ [Trax::Range::adjust]");
  Trax::Range range1(0, 1);
  BOOST_CHECK(range1.lo() == 0);
  BOOST_CHECK(range1.hi() == 1);
  range1.adjust();
  BOOST_CHECK(range1.lo() == 0);
  BOOST_CHECK(range1.lo() != range1.hi());
  BOOST_CHECK(range1.hi() > 1);

  Trax::Range range2(1, 1);
  BOOST_CHECK(range2.lo() == 1);
  BOOST_CHECK(range2.hi() > 1);
  BOOST_CHECK(range2.hi() < 1.0001);
  double hi = range2.hi();

  range2.adjust();
  BOOST_CHECK(range2.hi() == hi);
}

BOOST_AUTO_TEST_CASE(adjust_zero)
{
  BOOST_TEST_MESSAGE("+ [Trax::Range::adjust with zero or infinite limits]");
  // A closed range ending at zero must include zero
  Trax::Range range(-5, 0);
  range.adjust();
  BOOST_CHECK(range.adjusted());
  BOOST_CHECK(range.hi() > 0);
  BOOST_CHECK(range.hi() < 1e-30);

  Trax::Range negative(-5, -0.5);
  negative.adjust();
  BOOST_CHECK(negative.hi() > -0.5);

  // Infinite limits are left alone
  Trax::Range open(0, std::numeric_limits<float>::infinity());
  open.adjust();
  BOOST_CHECK(!open.adjusted());
  BOOST_CHECK(std::isinf(open.hi()));
}

BOOST_AUTO_TEST_CASE(lessthan)
{
  BOOST_TEST_MESSAGE("+ [Trax::Range::operator<]");
  Trax::Range range01(0, 1);
  Trax::Range range02(0, 2);
  Trax::Range range11(1, 1);
  Trax::Range range12(1, 2);
  BOOST_CHECK(range01 < range02);
  BOOST_CHECK(range01 < range11);
  BOOST_CHECK(range01 < range12);
  BOOST_CHECK(range02 < range11);
  BOOST_CHECK(range02 < range12);
  BOOST_CHECK(range11 < range12);
}
