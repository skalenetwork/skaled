#include <libdevcore/TransientDirectory.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>

#include <libdevcore/SharedSpace.h>

#include <thread>

using namespace std;

using namespace dev::test;
using namespace dev;

BOOST_FIXTURE_TEST_SUITE( SharedSpaceTests, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( All ) {
    TransientDirectory td;
    SharedSpace space(td.path());

    space.lock();
    BOOST_ASSERT(!space.try_lock());

    auto th = std::thread([&space](){
        space.unlock();
    });
    th.join();

    BOOST_ASSERT(space.try_lock());
    BOOST_ASSERT(!space.try_lock());
    space.unlock();
}

BOOST_AUTO_TEST_SUITE_END()
