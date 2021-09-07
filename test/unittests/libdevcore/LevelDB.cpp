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
    string r = "0";
    r[0] = ( unsigned char ) ( rand() % 256 );

    h256 initial_hash = db->hashBase();

    BOOST_REQUIRE( db->lookup( db::Slice( r + "no-key" ) ).empty() );

    string str_with_0 = string( r + "begin and" ) + char( 0 ) + "end";

    db->insert( db::Slice( r + "other key" ), db::Slice( r + "val1" ) );
    db->insert( str_with_0, db::Slice( r + "val2" ) );
    db->insert( db::Slice( r + "key for 0" ), str_with_0 );

    BOOST_REQUIRE( db->exists( db::Slice( r + "other key" ) ) );
    BOOST_REQUIRE( db->exists( str_with_0 ) );
    BOOST_REQUIRE( db->exists( db::Slice( r + "key for 0" ) ) );

    BOOST_REQUIRE( !db->exists( db::Slice( r + "dummy_key" ) ) );

    BOOST_REQUIRE( db->lookup( db::Slice( r + "other key" ) ) == r + "val1" );
    BOOST_REQUIRE( db->lookup( db::Slice( str_with_0 ) ) == r + "val2" );
    BOOST_REQUIRE( db->lookup( string( r + "key for 0" ) ) == str_with_0 );
    BOOST_REQUIRE( db->lookup( db::Slice( r + "dummy_key" ) ) == "" );

    db->kill( str_with_0 );
    BOOST_REQUIRE( !db->exists( str_with_0 ) );
    BOOST_REQUIRE( db->lookup( db::Slice( str_with_0 ) ) == "" );

    // forEach
    int cnt = 0;
    db->forEach( [&cnt, r]( db::Slice _key, db::Slice ) -> bool {
        BOOST_REQUIRE( _key.contentsEqual( db::Slice( r + "other key" ).toVector() ) ||
                       _key.contentsEqual( db::Slice( r + "key for 0" ).toVector() ) );
        ++cnt;
        return true;
    } );
    BOOST_REQUIRE_EQUAL( cnt, 2 );

    h256 middle_hash = db->hashBase();
    BOOST_REQUIRE( middle_hash != initial_hash );

    // batch
    std::unique_ptr< db::WriteBatchFace > b = db->createWriteBatch();
    b->insert( db::Slice( r + "b-other key" ), db::Slice( r + "val1" ) );
    b->insert( str_with_0, db::Slice( r + "val2" ) );
    b->insert( db::Slice( r + "b-key for 0" ), str_with_0 );
    b->kill( db::Slice( r + "other key" ) );
    db->commit( std::move( b ) );

    cnt = 0;
    db->forEach( [&cnt, str_with_0, r]( db::Slice _key, db::Slice ) -> bool {
        BOOST_REQUIRE( _key.contentsEqual( db::Slice( r + "b-other key" ).toVector() ) ||
                       _key.contentsEqual( db::Slice( str_with_0 ).toVector() ) ||
                       _key.contentsEqual( db::Slice( r + "b-key for 0" ).toVector() ) ||
                       _key.contentsEqual( db::Slice( r + "key for 0" ).toVector() ) );
        ++cnt;
        return true;
    } );
    BOOST_REQUIRE_EQUAL( cnt, 4 );

    BOOST_REQUIRE( db->hashBase() != h256() );
    BOOST_REQUIRE( db->hashBase() != initial_hash );
    BOOST_REQUIRE( db->hashBase() != middle_hash );
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

    h256 h1 = db1->hashBase();
    h256 h2 = db1->hashBase();
    BOOST_REQUIRE_EQUAL( h1, h2 );

    test_leveldb( db1 );
    test_leveldb( db2 );

    BOOST_REQUIRE( db1->hashBase() != db2->hashBase() );
    BOOST_REQUIRE( db1->hashBase() != h1 );
    BOOST_REQUIRE( db2->hashBase() != h2 );
}

BOOST_AUTO_TEST_CASE( rotation_test ) {
    TransientDirectory td;
    const int nPieces = 5;
    const int rotateInterval = 10;
    const int nPreserved = ( nPieces - 1 ) * rotateInterval;

    db::ManuallyRotatingLevelDB rdb( td.path(), nPieces );

    for ( int i = 0; i < nPreserved * 3; ++i ) {
        if ( i % rotateInterval == 0 )
            rdb.rotate();

        rdb.insert( to_string( i ), "val " + to_string( i ) );

        if ( i >= nPreserved - 1 ) {
            string old = to_string( i - nPreserved + 1 );
            BOOST_REQUIRE_EQUAL( rdb.lookup( old ), "val " + old );
        }  // if
        else if ( i > nPreserved + rotateInterval ) {
            string old = to_string( i - nPreserved - rotateInterval );
            BOOST_REQUIRE_EQUAL( rdb.exists( old ), false );
            BOOST_REQUIRE_EQUAL( rdb.lookup( old ), "" );
        }  // else if

        if ( i > rotateInterval && ( i + rotateInterval / 2 ) % rotateInterval == 0 ) {
            int cnt = 0;
            rdb.forEach( [&cnt]( db::Slice, db::Slice ) -> bool {
                ++cnt;
                return true;
            } );
            BOOST_REQUIRE( cnt > rotateInterval );
        }  // if check all
    }
}

BOOST_AUTO_TEST_CASE( rotation_rewrite_test ) {
    TransientDirectory td;
    const int nPieces = 3;

    db::ManuallyRotatingLevelDB rdb( td.path(), nPieces );

    rdb.insert( string( "a" ), string( "va" ) );
    rdb.insert( string( "b" ), string( "vb" ) );

    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "a" ) ), string( "va" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "b" ) ), string( "vb" ) );

    rdb.insert( string( "a" ), string( "va_new" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "a" ) ), string( "va_new" ) );

    rdb.rotate();  // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "a" ) ), string( "va_new" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "b" ) ), string( "vb" ) );

    rdb.insert( string( "b" ), string( "vb_new" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "b" ) ), string( "vb_new" ) );
    rdb.insert( string( "a" ), string( "va_new_new" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "a" ) ), string( "va_new_new" ) );
}

BOOST_AUTO_TEST_CASE( rotation_circle_test ){
    TransientDirectory td;
    const int nPieces = 3;

    db::ManuallyRotatingLevelDB rdb( td.path(), nPieces );

    rdb.insert( string( "a" ), string( "va1" ) );
    rdb.rotate();
    rdb.insert( string( "a" ), string( "va2" ) );

    int cnt = 0;
    for(int i=0; i<nPieces; ++i){
        if(rdb.exists(string("a"))){
            BOOST_REQUIRE_EQUAL( rdb.lookup( string( "a" ) ), string( "va2" ) );
            cnt++;
        }
        rdb.rotate();
    }// for

    BOOST_REQUIRE(!rdb.exists(string("a")));
    BOOST_REQUIRE_EQUAL(cnt, nPieces);
}

BOOST_AUTO_TEST_CASE( rotation_reopen_test ){
    TransientDirectory td;
    const int nPieces = 5;

    // pre_rotate = how many rotations do before reopen
    for(int pre_rotate = 0; pre_rotate < nPieces; pre_rotate++){
        // scope 1
        {
            db::ManuallyRotatingLevelDB rdb( td.path(), nPieces );

            rdb.insert( string( "a" ), to_string(0) );
            for(int i=1; i<=pre_rotate; ++i){
                rdb.rotate();
                rdb.insert( string( "a" ), to_string(i) );
            }

            BOOST_REQUIRE_EQUAL( rdb.lookup(string("a")), to_string(pre_rotate));
        }

        // scope 2
        {
            db::ManuallyRotatingLevelDB rdb( td.path(), nPieces );
            BOOST_REQUIRE_EQUAL( rdb.lookup(string("a")), to_string(pre_rotate));
        }

    }// for pre_rotate
}

BOOST_AUTO_TEST_SUITE_END()
