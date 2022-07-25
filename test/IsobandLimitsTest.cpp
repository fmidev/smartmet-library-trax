#include "IsobandLimits.h"
#include <boost/test/included/unit_test.hpp>
#include <limits>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Trax::IsobandLimits tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(sort)
{
  BOOST_TEST_MESSAGE("+ [Trax::Range::sort]");
  auto inf = std::numeric_limits<float>::infinity();
  auto nan = std::numeric_limits<float>::quiet_NaN();

  {
    Trax::IsobandLimits limits;

    // Adding these in reverse order of wanted result
    limits.add(60, inf);
    limits.add(40, 60);
    limits.add(30, 40);
    limits.add(10, 20);
    limits.add(5, 10);
    limits.add(1, 5);
    limits.add(-inf, 1);
    limits.add(nan, nan);

    BOOST_TEST_INFO("Limits before: " << limits.dump());
    limits.sort(true);
    BOOST_TEST_INFO("Limits after: " << limits.dump());
    BOOST_CHECK(limits.valid());
  }

  // Bug case encountered in production in handling of -inf
  {
    Trax::IsobandLimits limits;

    // Adding these in the actual order of the WMS configuration file
    limits.add(0.52, inf);
    limits.add(0.43, 0.52);
    limits.add(0.35, 0.43);
    limits.add(0.28, 0.35);
    limits.add(0.21, 0.28);
    limits.add(0.16, 0.21);
    limits.add(0.12, 0.16);
    limits.add(-inf, 0.12);

    BOOST_TEST_INFO("Limits before: " << limits.dump());
    limits.sort(false);
    BOOST_TEST_INFO("Limits after: " << limits.dump());
    BOOST_CHECK(limits.valid());
  }
}
