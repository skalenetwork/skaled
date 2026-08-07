#include <libskale/SnapshotManager.h>
#include <skutils/btrfs.h>

#include <test/tools/libtesteth/BtrfsTestFixture.h>
#include <test/tools/libtesteth/TestHelper.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

using namespace std;
namespace fs = boost::filesystem;

const string BTRFS_FILE_PATH = dev::test::BtrfsTestMount::defaultImagePath();
const string BTRFS_DIR_PATH = dev::test::BtrfsTestMount::defaultMountPath();

struct BtrfsFixture {
    BtrfsFixture() : btrfsMount( dev::test::BtrfsTestMount::fixedPaths() ) {}

    void dropRoot() { btrfsMount.dropRoot(); }
    void gainRoot() { btrfsMount.gainRoot(); }

    dev::test::BtrfsTestMount btrfsMount;
};

struct NoBtrfsFixture : public dev::test::BtrfsTestEnvironment {
    NoBtrfsFixture() {
        dev::test::BtrfsTestMount::cleanupArtifacts( BTRFS_DIR_PATH, BTRFS_FILE_PATH, true );
        dropRoot();
        int rv = system( ( "mkdir " + BTRFS_DIR_PATH ).c_str() );
        rv = system( ( "mkdir " + BTRFS_DIR_PATH + "/vol1" ).c_str() );
        rv = system( ( "mkdir " + BTRFS_DIR_PATH + "/vol2" ).c_str() );
        ( void ) rv;
        gainRoot();
    }
    ~NoBtrfsFixture() {
        dev::test::BtrfsTestMount::cleanupArtifacts( BTRFS_DIR_PATH, BTRFS_FILE_PATH, true );
    }
};

BOOST_AUTO_TEST_SUITE(
    BtrfsTestSuite, *boost::unit_test::precondition( dev::test::option_all_tests ) )

BOOST_FIXTURE_TEST_CASE( SimplePositiveTest, BtrfsFixture,

    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );

    std::string chainDirName = dev::eth::BlockChain::getChainDirName( dev::eth::ChainParams() );

    // add files 1
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d11" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d11" ) );
#ifndef FAIR
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "filestorage" / "d21" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "filestorage" / "d21" ) );
#endif

    auto latest0 = mgr.getLatestSnapshots();
    std::pair< int, int > expected0{ 0, 0 };
    BOOST_REQUIRE( latest0 == expected0 );

    // create snapshot 1 and check its presense
    mgr.doSnapshot( 1 );
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / chainDirName / "d11" ) );
#ifndef FAIR
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "filestorage" / "d21" ) );
#endif

    // add and remove something
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d12" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d12" ) );

#ifndef FAIR
    fs::remove( fs::path( BTRFS_DIR_PATH ) / "filestorage" / "d21" );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "filestorage" / "d21" ) );
#endif

    auto latest1 = mgr.getLatestSnapshots();
    std::pair< int, int > expected1{ 0, 1 };
    BOOST_REQUIRE( latest1 == expected1 );

    // create snapshot 2 and check files 1 and files 2
    mgr.doSnapshot( 2 );
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / chainDirName / "d11" ) );
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / chainDirName / "d12" ) );

#ifndef FAIR
    BOOST_REQUIRE(
        !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "filestorage" / "d21" ) );
#endif

    // check that files appear/disappear on restore
    mgr.restoreSnapshot( 1 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d11" ) );
#ifndef FAIR
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "filestorage" / "d21" ) );
#endif
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d12" ) );

    fs::path diff12 = mgr.makeOrGetDiff( 2 );
    btrfs.subvolume._delete( ( BTRFS_DIR_PATH + "/snapshots/2/" + chainDirName ).c_str() );
#ifndef FAIR
    btrfs.subvolume._delete( ( BTRFS_DIR_PATH + "/snapshots/2/filestorage" ).c_str() );
#endif
    fs::remove_all( BTRFS_DIR_PATH + "/snapshots/2" );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" ) );

    mgr.importDiff( 2 );
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / chainDirName / "d11" ) );
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / chainDirName / "d12" ) );
#ifndef FAIR
    BOOST_REQUIRE(
        !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "filestorage" / "d21" ) );
#endif

    mgr.restoreSnapshot( 2 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d12" ) );
#ifndef FAIR
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "filestorage" / "d21" ) );
#endif

    auto latest2 = mgr.getLatestSnapshots();
    std::pair< int, int > expected2{ 1, 2 };
    BOOST_REQUIRE( latest2 == expected2 );

    mgr.doSnapshot( 3 );
    auto latest3 = mgr.getLatestSnapshots();
    std::pair< int, int > expected3{ 2, 3 };
    BOOST_REQUIRE( latest3 == expected3 );

    BOOST_REQUIRE_NO_THROW( mgr.removeSnapshot( 1 ) );
    BOOST_REQUIRE_NO_THROW( mgr.removeSnapshot( 2 ) );
    BOOST_REQUIRE_NO_THROW( mgr.removeSnapshot( 3 ) );
}

BOOST_FIXTURE_TEST_CASE(
    NoBtrfsTest, NoBtrfsFixture, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    BOOST_REQUIRE_THROW( SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) ),
        SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_FIXTURE_TEST_CASE(
    BadPathTest, BtrfsFixture, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams() );
    BOOST_REQUIRE_EXCEPTION(
        SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) / "_invalid" ),
        SnapshotManager::InvalidPath, [this]( const SnapshotManager::InvalidPath& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_invalid";
        } );
}

BOOST_FIXTURE_TEST_CASE( InaccessiblePathTest, BtrfsFixture,
    *boost::unit_test::precondition( []( unsigned long ) -> bool { return false; } ) ) {
    std::string chainDirName = dev::eth::BlockChain::getChainDirName( dev::eth::ChainParams() );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_w" );
    chmod( ( BTRFS_DIR_PATH + "/_no_w" ).c_str(), 0775 );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_w" / chainDirName );
    chmod( ( BTRFS_DIR_PATH + "/_no_w/vol1" ).c_str(), 0777 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_x" );
    chmod( ( BTRFS_DIR_PATH + "/_no_x" ).c_str(), 0774 );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_x" / chainDirName );
    chmod( ( BTRFS_DIR_PATH + "/_no_x/vol1" ).c_str(), 0777 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_r" );
    chmod( ( BTRFS_DIR_PATH + "/_no_r" ).c_str(), 0770 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_x" / "_no_parent_x" );
    chmod( ( BTRFS_DIR_PATH + "/_no_x/_no_parent_x" ).c_str(), 0777 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_r" / "_no_parent_r" );
    chmod( ( BTRFS_DIR_PATH + "/_no_r/_no_parent_r" ).c_str(), 0777 );

    dropRoot();

    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams() );
    BOOST_REQUIRE_EXCEPTION(
        SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) / "_no_w" ),
        SnapshotManager::CannotCreate, [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_no_w" / "snapshots";
        } );

    BOOST_REQUIRE_EXCEPTION(
        SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) / "_no_x" ),
        SnapshotManager::CannotCreate, [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_no_x" / "snapshots";
        } );

    BOOST_REQUIRE_EXCEPTION(
        SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) / "_no_r" ),
        SnapshotManager::CannotCreate, [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_no_x" / "snapshots";
        } );
}

BOOST_FIXTURE_TEST_CASE(
    SnapshotTest, BtrfsFixture, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2 ) );
    BOOST_REQUIRE_THROW( mgr.doSnapshot( 2 ), SnapshotManager::SnapshotPresent );

    chmod( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" ).c_str(), 0 );

    dropRoot();

    BOOST_REQUIRE_EXCEPTION( mgr.doSnapshot( 3 ), SnapshotManager::CannotRead,
        [this]( const SnapshotManager::CannotRead& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "snapshots" / "3";
        } );

    gainRoot();
    chmod( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" ).c_str(), 0111 );
    dropRoot();

    BOOST_REQUIRE_EXCEPTION( mgr.doSnapshot( 3 ), SnapshotManager::CannotCreate,
        [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "snapshots" / "3";
        } );

    // cannot delete
    BOOST_REQUIRE_THROW( mgr.restoreSnapshot( 2 ), SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_FIXTURE_TEST_CASE(
    RestoreTest, BtrfsFixture, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );

    BOOST_REQUIRE_THROW( mgr.restoreSnapshot( 2 ), SnapshotManager::SnapshotAbsent );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2 ) );

    BOOST_REQUIRE_NO_THROW( mgr.restoreSnapshot( 2 ) );

    BOOST_REQUIRE_EQUAL(
        0, btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" /
                                      dev::eth::BlockChain::getChainDirName( *chainParams ) )
                                        .c_str() ) );

    BOOST_REQUIRE_THROW( mgr.restoreSnapshot( 2 ), SnapshotManager::CannotPerformBtrfsOperation );
}

#ifndef FAIR
BOOST_FIXTURE_TEST_CASE(
    DiffTest, BtrfsFixture, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );
    mgr.doSnapshot( 2 );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "filestorage" / "dir" );
    mgr.doSnapshot( 4 );

    BOOST_REQUIRE_THROW( mgr.makeOrGetDiff( 3 ), SnapshotManager::SnapshotAbsent );
    BOOST_REQUIRE_NO_THROW( mgr.makeOrGetDiff( 2 ) );

    fs::path tmp;
    BOOST_REQUIRE_NO_THROW( tmp = mgr.makeOrGetDiff( 4 ) );
    fs::remove( tmp );

    BOOST_REQUIRE_NO_THROW( tmp = mgr.makeOrGetDiff( 2 ) );
    fs::remove( tmp );

    BOOST_REQUIRE_NO_THROW( tmp = mgr.makeOrGetDiff( 2 ) );
    fs::remove( tmp );

    // strange - but ok...
    BOOST_REQUIRE_NO_THROW( tmp = mgr.makeOrGetDiff( 4 ) );
    BOOST_REQUIRE_GT( fs::file_size( tmp ), 0 );
    fs::remove( tmp );

    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "filestorage" ).c_str() );

    BOOST_REQUIRE_THROW(
        tmp = mgr.makeOrGetDiff( 4 ), SnapshotManager::CannotPerformBtrfsOperation );
}
#endif

// TODO Tests to check no files left in /tmp?!

BOOST_FIXTURE_TEST_CASE(
    ImportTest, BtrfsFixture, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );

    BOOST_REQUIRE_THROW( mgr.importDiff( 8 ), SnapshotManager::InvalidPath );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2 ) );
    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 4 ) );

    fs::path diff24;
    BOOST_REQUIRE_NO_THROW( diff24 = mgr.makeOrGetDiff( 4 ) );

    BOOST_REQUIRE_THROW( mgr.importDiff( 4 ), SnapshotManager::SnapshotPresent );

    std::string chainDirName = dev::eth::BlockChain::getChainDirName( dev::eth::ChainParams() );

    // delete dest
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / chainDirName ).c_str() );
#ifndef FAIR
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "filestorage" ).c_str() );
#endif
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" );

    BOOST_REQUIRE_NO_THROW( mgr.importDiff( 4 ) );

    // delete dest
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / chainDirName ).c_str() );
#ifndef FAIR
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "filestorage" ).c_str() );
#endif
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" );

    // no source
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / chainDirName ).c_str() );

    // BOOST_REQUIRE_THROW( mgr.importDiff( 2, 4 ), SnapshotManager::CannotPerformBtrfsOperation );

#ifndef FAIR
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "filestorage" ).c_str() );
#endif
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" );
    // BOOST_REQUIRE_THROW( mgr.importDiff( 2, 4 ), SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_FIXTURE_TEST_CASE( SnapshotRotationTest, BtrfsFixture,

    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 1 ) );
    sleep( 1 );
    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 0 ) );
    sleep( 1 );
    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2 ) );
    sleep( 1 );
    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 3 ) );

    BOOST_REQUIRE_NO_THROW( mgr.leaveNLastSnapshots( 2 ) );

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "0" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "3" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" ) );
}

BOOST_FIXTURE_TEST_CASE( DiffRotationTest, BtrfsFixture,

    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );

    fs::path diff12 = mgr.getDiffPath( 2 );
    {
        std::ofstream os( diff12.c_str() );
        os.close();
    }
    sleep( 1 );
    fs::path diff13 = mgr.getDiffPath( 3 );
    {
        std::ofstream os( diff13.c_str() );
        os.close();
    }
    sleep( 1 );
    fs::path diff14 = mgr.getDiffPath( 4 );
    {
        std::ofstream os( diff14.c_str() );
        os.close();
    }

    BOOST_REQUIRE_NO_THROW( mgr.leaveNLastDiffs( 2 ) );

    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "diffs" / "2" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "diffs" / "3" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "diffs" / "4" ) );
}

BOOST_FIXTURE_TEST_CASE( RemoveSnapshotTest, BtrfsFixture,

    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );

    mgr.doSnapshot( 1 );
    mgr.doSnapshot( 2 );

    mgr.cleanupButKeepSnapshot( 1 );

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" ) );

    BOOST_REQUIRE_NO_THROW( mgr.restoreSnapshot( 1 ) );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2 ) );
}

BOOST_FIXTURE_TEST_CASE( CleanupTest, BtrfsFixture,

    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams{} );
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );

    mgr.doSnapshot( 1 );
    mgr.doSnapshot( 2 );

    mgr.cleanup();

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" ) );

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "diffs" ) );

    std::string chainDirName = dev::eth::BlockChain::getChainDirName( dev::eth::ChainParams() );

    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / chainDirName ) );
#ifndef FAIR
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "filestorage" ) );
#endif
}

#ifdef HISTORIC_STATE
BOOST_FIXTURE_TEST_CASE(
    ArchiveNodeTest, BtrfsFixture, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    std::shared_ptr< dev::eth::ChainParams > chainParams( new dev::eth::ChainParams() );
    chainParams->setArchiveMode();
    SnapshotManager mgr( chainParams, fs::path( BTRFS_DIR_PATH ) );

    std::string chainDirName = dev::eth::BlockChain::getChainDirName( dev::eth::ChainParams() );

    // add files to core volumes
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d11" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / chainDirName / "d11" ) );

#ifndef FAIR
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "filestorage" / "d21" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "filestorage" / "d21" ) );
#endif
    // archive part
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "historic_roots" / "d31" );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "historic_state" / "d41" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "historic_roots" / "d31" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "historic_state" / "d41" ) );

    // create snapshot 1 and check its presense
    mgr.doSnapshot( 1 );
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / chainDirName / "d11" ) );
#ifndef FAIR
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "filestorage" / "d21" ) );
#endif
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "historic_roots" / "d31" ) );
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "historic_state" / "d41" ) );

    // make diff for archive node
    BOOST_REQUIRE_NO_THROW( mgr.makeOrGetDiff( 1 ) );

    // delete dest
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / chainDirName ).c_str() );
#ifndef FAIR
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "filestorage" ).c_str() );
#endif
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "historic_roots" ).c_str() );
    btrfs.subvolume._delete(
        ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "historic_state" ).c_str() );
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" );

    BOOST_REQUIRE_NO_THROW( mgr.importDiff( 1 ) );

    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / chainDirName / "d11" ) );
#ifndef FAIR
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "filestorage" / "d21" ) );
#endif
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "historic_roots" / "d31" ) );
    BOOST_REQUIRE(
        fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "historic_state" / "d41" ) );
}
#endif

BOOST_AUTO_TEST_SUITE_END()
