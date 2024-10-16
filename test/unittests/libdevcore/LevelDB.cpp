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

BOOST_FIXTURE_TEST_SUITE( LevelDBTests, TestOutputHelperFixture )

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

    auto batcher = make_shared<batched_io::rotating_db_io>( td.path(), nPieces, false );
    db::ManuallyRotatingLevelDB rdb( batcher );

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

    auto batcher = make_shared<batched_io::rotating_db_io>( td.path(), nPieces, false );
    db::ManuallyRotatingLevelDB rdb( batcher );

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

    auto batcher = make_shared<batched_io::rotating_db_io>( td.path(), nPieces, false );
    db::ManuallyRotatingLevelDB rdb( batcher );

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
            auto batcher = make_shared<batched_io::rotating_db_io>( td.path(), nPieces, false );
            db::ManuallyRotatingLevelDB rdb( batcher );

            rdb.insert( string( "a" ), to_string(0) );
            for(int i=1; i<=pre_rotate; ++i){
                rdb.rotate();
                rdb.insert( string( "a" ), to_string(i) );
            }

            BOOST_REQUIRE_EQUAL( rdb.lookup(string("a")), to_string(pre_rotate));
        }

        // scope 2
        {
            auto batcher = make_shared<batched_io::rotating_db_io>( td.path(), nPieces, false );
            db::ManuallyRotatingLevelDB rdb( batcher );
            BOOST_REQUIRE_EQUAL( rdb.lookup(string("a")), to_string(pre_rotate));
        }

    }// for pre_rotate
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( HistoricDBTests, TestOutputHelperFixture )

BOOST_AUTO_TEST_CASE( simple_whole_test ) {
    TransientDirectory td;
    auto bio = make_shared<batched_io::BatchedRotatingHistoricDbIO>( td.path() );
    db::RotatingHistoricState rhs( bio );

    rhs.rotate(1001);
    rhs.rotate(1002);

    test_leveldb( &rhs );
}

BOOST_AUTO_TEST_CASE( rotation_rewrite_test ) {
    TransientDirectory td;

    auto batcher = make_shared<batched_io::BatchedRotatingHistoricDbIO>( td.path() );
    db::RotatingHistoricState rdb( batcher );

    rdb.insert( string( "a" ), string( "va" ) );
    rdb.insert( string( "b" ), string( "vb" ) );

    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "a" ) ), string( "va" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "b" ) ), string( "vb" ) );

    rdb.insert( string( "a" ), string( "va_new" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "a" ) ), string( "va_new" ) );

    rdb.rotate(1001);  // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "a" ) ), string( "va_new" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "b" ) ), string( "vb" ) );

    rdb.insert( string( "b" ), string( "vb_new" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "b" ) ), string( "vb_new" ) );
    rdb.insert( string( "a" ), string( "va_new_new" ) );
    BOOST_REQUIRE_EQUAL( rdb.lookup( string( "a" ) ), string( "va_new_new" ) );
}

BOOST_AUTO_TEST_CASE( rotation_reopen_test ){
    TransientDirectory td;

    // make 2 rotations, then reopen

    // scope 1
    {
        auto batcher = make_shared<batched_io::BatchedRotatingHistoricDbIO>( td.path() );
        db::RotatingHistoricState rdb( batcher );

        rdb.insert( string( "a" ), to_string(0) );
        for(int i=1; i<=2; ++i){
            rdb.rotate(i*1000);
            rdb.insert( string( "a" ), to_string(i) );
        }

        BOOST_REQUIRE_EQUAL( rdb.lookup(string("a")), to_string(2));
    }

    // scope 2
    {
        auto batcher = make_shared<batched_io::BatchedRotatingHistoricDbIO>( td.path() );
        db::RotatingHistoricState rdb( batcher );
        BOOST_REQUIRE_EQUAL( rdb.lookup(string("a")), to_string(2));
    }

}

BOOST_AUTO_TEST_CASE( basic_io_test ) {
    TransientDirectory td;
    batched_io::BatchedRotatingHistoricDbIO io( td.path() );

    // check initial state
    auto bn = io.getBlockNumbers();
    BOOST_REQUIRE_EQUAL(bn.size(), 1);
    BOOST_REQUIRE_EQUAL(bn[0], 0);

    // try to get pieces
    BOOST_REQUIRE_EQUAL(io.getPieceByBlockNumber(0), io.getPieceByBlockNumber(1));

    // insert 1
    auto db = io.currentPiece();
    db->insert(db::Slice( "1" ), db::Slice( "foobar" ));

    // rotate
    io.rotate(10);

    // check rotation
    bn = io.getBlockNumbers();
    BOOST_REQUIRE_EQUAL(bn.size(), 2);
    BOOST_REQUIRE_EQUAL(bn[0], 0);
    BOOST_REQUIRE_EQUAL(bn[1], 10);

    // check pieces
    auto piece0 = io.getPieceByBlockNumber( 0 );
    BOOST_REQUIRE( piece0->exists( db::Slice( "1" ) ) );

    auto piece10 = io.getPieceByBlockNumber( 10 );
    BOOST_REQUIRE( !piece10->exists( db::Slice( "1" ) ) );

    // check non-exact requests
    auto piece9 = io.getPieceByBlockNumber( 9 );
    BOOST_REQUIRE( piece9->exists( db::Slice( "1" ) ) );

    auto piece11 = io.getPieceByBlockNumber( 11 );
    BOOST_REQUIRE( !piece11->exists( db::Slice( "1" ) ) );

    auto piece_max = io.getPieceByBlockNumber( UINT64_MAX );
    BOOST_REQUIRE( !piece_max->exists( db::Slice( "1" ) ) );
}

BOOST_AUTO_TEST_CASE( range_test ){
    TransientDirectory td;
    batched_io::BatchedRotatingHistoricDbIO io( td.path() );

    vector<uint64_t> bn{0};
    // create 11 DBs and fill bn
    for(size_t i=0; i<10; ++i){
        // insert i
        bn.push_back((i+1)*10);
        io.rotate((i+1)*10);
    }

    // bn: 0 10 20 30 40 50 .. 100

    auto range = io.getRangeForBlockNumber(0);
    std::equal( range.first, range.second, bn.rend()-1 );

    range = io.getRangeForBlockNumber(9);
    std::equal( range.first, range.second, bn.rend()-1 );

    range = io.getRangeForBlockNumber(10);
    std::equal( range.first, range.second, bn.rend()-1-1 );

    range = io.getRangeForBlockNumber(20);
    std::equal( range.first, range.second, bn.rend()-1-2 );

    range = io.getRangeForBlockNumber(21);
    std::equal( range.first, range.second, bn.rend()-1-2 );

    range = io.getRangeForBlockNumber(29);
    std::equal( range.first, range.second, bn.rend()-1-2 );

    range = io.getRangeForBlockNumber(30);
    std::equal( range.first, range.second, bn.rend()-1-3 );

    range = io.getRangeForBlockNumber(999);
    std::equal( range.first, range.second, bn.rbegin() );

    range = io.getRangeForBlockNumber(UINT64_MAX);
    std::equal( range.first, range.second, bn.rbegin() );
}

BOOST_AUTO_TEST_SUITE_END()
