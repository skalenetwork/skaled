#include <libdevcore/Common.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/Log.h>
#include <libdevcore/ManuallyRotatingLevelDB.h>
#include <libdevcore/SplitDB.h>
#include <libdevcore/TransientDirectory.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/test/unit_test.hpp>

#include <skutils/console_colors.h>

using namespace std;

using namespace dev::test;
using namespace dev;

BOOST_FIXTURE_TEST_SUITE( LevelDBTests, TestOutputHelperFixture )

void test_leveldb( db::DatabaseFace* db ) {
    BOOST_REQUIRE( db->lookup( db::Slice( "no-key" ) ).empty() );

    string str_with_0 = string( "begin and" ) + char( 0 ) + "end";

    db->insert( db::Slice( "other key" ), db::Slice( "val1" ) );
    db->insert( str_with_0, db::Slice( "val2" ) );
    db->insert( db::Slice( "key for 0" ), str_with_0 );

    BOOST_REQUIRE( db->exists( db::Slice( "other key" ) ) );
    BOOST_REQUIRE( db->exists( str_with_0 ) );
    BOOST_REQUIRE( db->exists( db::Slice( "key for 0" ) ) );

    BOOST_REQUIRE( !db->exists( db::Slice( "dummy_key" ) ) );

    BOOST_REQUIRE( db->lookup( db::Slice( "other key" ) ) == "val1" );
    BOOST_REQUIRE( db->lookup( db::Slice( str_with_0 ) ) == "val2" );
    BOOST_REQUIRE( db->lookup( string( "key for 0" ) ) == str_with_0 );
    BOOST_REQUIRE( db->lookup( db::Slice( "dummy_key" ) ) == "" );

    db->kill( str_with_0 );
    BOOST_REQUIRE( !db->exists( str_with_0 ) );
    BOOST_REQUIRE( db->lookup( db::Slice( str_with_0 ) ) == "" );

    // TODO Foreach
    // TODO Batches!
    // Hash
}

BOOST_AUTO_TEST_CASE( ordinary_test ) {
    TransientDirectory td;
    db::LevelDB leveldb( td.path() );

    test_leveldb( &leveldb );
}

BOOST_AUTO_TEST_CASE( split_test ) {
    TransientDirectory td;
    auto p_leveldb = std::make_shared< db::LevelDB >( td.path() );
    db::SplitDB splitdb( p_leveldb );

    db::DatabaseFace* db1 = splitdb.newInterface();
    db::DatabaseFace* db2 = splitdb.newInterface();

    test_leveldb( db1 );
    test_leveldb( db2 );
}

BOOST_AUTO_TEST_SUITE_END()
