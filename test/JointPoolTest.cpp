#include "Joint.h"
#include "JointPool.h"
#include "Vertex.h"
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Trax::JointPool tests";
  unit_test_log.set_threshold_level(log_messages);

  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(create)
{
  BOOST_TEST_MESSAGE("+ [Trax::JointPool::create]");

  const int n = 100;

  Trax::JointPool pool(8);
  for (int i = 0; i < n; i++)
    pool.create(Trax::Vertex(i, i, Trax::VertexType::Corner, i, i, false));

  int i = 0;
  for (auto it = pool.begin(), end = pool.end(); it != end; ++it, i++)
  {
    auto* j = *it;
    auto& vertex = j->vertex;
    BOOST_CHECK_EQUAL(vertex.column, i);
  }
}
