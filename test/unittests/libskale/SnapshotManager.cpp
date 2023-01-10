#include <libskale/SnapshotManager.h>
#include <skutils/btrfs.h>

#include <test/tools/libtesteth/TestHelper.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;
namespace fs = boost::filesystem;


std::shared_ptr<std::vector<std::uint8_t>> getSampleLastConsensusBlock() {
    std::shared_ptr<vector<uint8_t>> sampleLastConsensusBlock;
    sampleLastConsensusBlock.reset(new vector<uint8_t> { 1, 2, 3, 4, 5});
    return sampleLastConsensusBlock;
}


int setid_system( const char* cmd, uid_t uid, gid_t gid ) {
    pid_t pid = fork();
    if ( pid ) {
        int status;
        waitpid( pid, &status, 0 );
        return WEXITSTATUS( status );
    }

    int rv;
#if ( !defined __APPLE__ )
    rv = setresuid( uid, uid, uid );
    rv = setresgid( gid, gid, gid );
#endif

    rv = execl( "/bin/sh", "sh", "-c", cmd, ( char* ) NULL );
    ( void ) rv;
    return 0;
}

struct FixtureCommon {
    const string BTRFS_FILE_PATH = "btrfs.file";
    const string BTRFS_DIR_PATH = "btrfs";
    uid_t sudo_uid;
    gid_t sudo_gid;

    void check_sudo() {
#if ( !defined __APPLE__ )
        char* id_str = getenv( "SUDO_UID" );
        if ( id_str == NULL ) {
            cerr << "Please run under sudo" << endl;
            exit( -1 );
        }

        sscanf( id_str, "%d", &sudo_uid );

        //    uid_t ru, eu, su;
        //    getresuid( &ru, &eu, &su );
        //    cerr << ru << " " << eu << " " << su << endl;

        if ( geteuid() != 0 ) {
            cerr << "Need to be root" << endl;
            exit( -1 );
        }

        id_str = getenv( "SUDO_GID" );
        sscanf( id_str, "%d", &sudo_gid );

        gid_t rgid, egid, sgid;
        getresgid( &rgid, &egid, &sgid );
        cerr << "GIDS: " << rgid << " " << egid << " " << sgid << endl;
#endif
    }

    void dropRoot() {
#if ( !defined __APPLE__ )
        int res = setresgid( sudo_gid, sudo_gid, 0 );
        cerr << "setresgid " << sudo_gid << " " << res << endl;
        if ( res < 0 )
            cerr << strerror( errno ) << endl;
        res = setresuid( sudo_uid, sudo_uid, 0 );
        cerr << "setresuid " << sudo_uid << " " << res << endl;
        if ( res < 0 )
            cerr << strerror( errno ) << endl;
#endif
    }

    void gainRoot() {
#if ( !defined __APPLE__ )
        int res = setresuid( 0, 0, 0 );
        if ( res ) {
            cerr << strerror( errno ) << endl;
            assert( false );
        }
        res = setresgid( 0, 0, 0 );
        if ( res ) {
            cerr << strerror( errno ) << endl;
            assert( false );
        }
#endif
    }
};

struct BtrfsFixture : public FixtureCommon {
    BtrfsFixture() {
        check_sudo();

        dropRoot();

        int rv = system( ( "dd if=/dev/zero of=" + BTRFS_FILE_PATH + " bs=1M count=200" ).c_str() );
        rv = system( ( "mkfs.btrfs " + BTRFS_FILE_PATH ).c_str() );
        rv = system( ( "mkdir " + BTRFS_DIR_PATH ).c_str() );

        gainRoot();
        rv = system( ( "mount -o user_subvol_rm_allowed " + BTRFS_FILE_PATH + " " + BTRFS_DIR_PATH )
                    .c_str() );
        rv = chown( BTRFS_DIR_PATH.c_str(), sudo_uid, sudo_gid );
        ( void )rv;
        dropRoot();

        //        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol1" ).c_str() );
        //        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol2" ).c_str() );
        // system( ( "mkdir " + BTRFS_DIR_PATH + "/snapshots" ).c_str() );

        gainRoot();
    }

    ~BtrfsFixture() {
        const char* NC = getenv( "NC" );
        if ( NC )
            return;
        gainRoot();
        int rv = system( ( "umount " + BTRFS_DIR_PATH ).c_str() );
        rv = system( ( "rmdir " + BTRFS_DIR_PATH ).c_str() );
        rv = system( ( "rm " + BTRFS_FILE_PATH ).c_str() );
        ( void ) rv;
    }
};

struct NoBtrfsFixture : public FixtureCommon {
    NoBtrfsFixture() {
        check_sudo();
        dropRoot();
        int rv = system( ( "mkdir " + BTRFS_DIR_PATH ).c_str() );
        rv = system( ( "mkdir " + BTRFS_DIR_PATH + "/vol1" ).c_str() );
        rv = system( ( "mkdir " + BTRFS_DIR_PATH + "/vol2" ).c_str() );
        ( void ) rv;
        gainRoot();
    }
    ~NoBtrfsFixture() {
        gainRoot();
        int rv = system( ( "rm -rf " + BTRFS_DIR_PATH ).c_str() );
        ( void ) rv;
    }
};

BOOST_AUTO_TEST_SUITE( BtrfsTestSuite,
    *boost::unit_test::precondition( dev::test::option_all_tests ) )

BOOST_FIXTURE_TEST_CASE( SimplePositiveTest, BtrfsFixture,
    
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    // add files 1
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d11" );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" ) );

    auto latest0 = mgr.getLatestSnasphots();
    std::pair< int, int > expected0 { 0, 0 };
    BOOST_REQUIRE( latest0 == expected0 );

    // create snapshot 1 and check its presense
    mgr.doSnapshot( 1 , getSampleLastConsensusBlock());
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "vol2" / "d21" ) );

    // add and remove something
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d12" );
    fs::remove( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d12" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" ) );

    auto latest1 = mgr.getLatestSnasphots();
    std::pair< int, int > expected1 { 0, 1 };
    BOOST_REQUIRE( latest1 == expected1 );

    // create snapshot 2 and check files 1 and files 2
    mgr.doSnapshot( 2 , getSampleLastConsensusBlock());
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" / "d12" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol2" / "d21" ) );

    // check that files appear/disappear on restore
    mgr.restoreSnapshot( 1 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d12" ) );

    fs::path diff12 = mgr.makeOrGetDiff( 2 );
    btrfs.subvolume._delete( ( BTRFS_DIR_PATH + "/snapshots/2/vol1" ).c_str() );
    btrfs.subvolume._delete( ( BTRFS_DIR_PATH + "/snapshots/2/vol2" ).c_str() );
    fs::remove_all( BTRFS_DIR_PATH + "/snapshots/2" );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" ) );

    mgr.importDiff( 2 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" / "d12" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol2" / "d21" ) );

    mgr.restoreSnapshot( 2 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d12" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" ) );

    auto latest2 = mgr.getLatestSnasphots();
    std::pair< int, int > expected2 { 1, 2 };
    BOOST_REQUIRE( latest2 == expected2 );

    mgr.doSnapshot( 3 , getSampleLastConsensusBlock());
    auto latest3 = mgr.getLatestSnasphots();
    std::pair< int, int > expected3 { 2, 3 };
    BOOST_REQUIRE( latest3 == expected3 );

    BOOST_REQUIRE_NO_THROW( mgr.removeSnapshot( 1 ) );
    BOOST_REQUIRE_NO_THROW( mgr.removeSnapshot( 2 ) );
    BOOST_REQUIRE_NO_THROW( mgr.removeSnapshot( 3 ) );
}

BOOST_FIXTURE_TEST_CASE( NoBtrfsTest, NoBtrfsFixture,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    BOOST_REQUIRE_THROW( SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} ),
        SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_FIXTURE_TEST_CASE( BadPathTest, BtrfsFixture,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    BOOST_REQUIRE_EXCEPTION(
        SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ) / "_invalid", {"vol1", "vol2"} ),
        SnapshotManager::InvalidPath, [this]( const SnapshotManager::InvalidPath& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_invalid";
        } );
}

BOOST_FIXTURE_TEST_CASE( InaccessiblePathTest, BtrfsFixture,
    *boost::unit_test::precondition( []( unsigned long ) -> bool { return false; } ) ) {
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_w" );
    chmod( ( BTRFS_DIR_PATH + "/_no_w" ).c_str(), 0775 );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_w" / "vol1" );
    chmod( ( BTRFS_DIR_PATH + "/_no_w/vol1" ).c_str(), 0777 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_x" );
    chmod( ( BTRFS_DIR_PATH + "/_no_x" ).c_str(), 0774 );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_x" / "vol1" );
    chmod( ( BTRFS_DIR_PATH + "/_no_x/vol1" ).c_str(), 0777 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_r" );
    chmod( ( BTRFS_DIR_PATH + "/_no_r" ).c_str(), 0770 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_x" / "_no_parent_x" );
    chmod( ( BTRFS_DIR_PATH + "/_no_x/_no_parent_x" ).c_str(), 0777 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_r" / "_no_parent_r" );
    chmod( ( BTRFS_DIR_PATH + "/_no_r/_no_parent_r" ).c_str(), 0777 );

    dropRoot();

    BOOST_REQUIRE_EXCEPTION( SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ) / "_no_w", {"vol1"} ),
        SnapshotManager::CannotCreate, [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_no_w" / "snapshots";
        } );

    BOOST_REQUIRE_EXCEPTION( SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ) / "_no_x", {"vol1"} ),
        SnapshotManager::CannotCreate, [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_no_x" / "snapshots";
        } );

    BOOST_REQUIRE_EXCEPTION( SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ) / "_no_r", {"vol1"} ),
        SnapshotManager::CannotCreate, [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_no_x" / "snapshots";
        } );
}

BOOST_FIXTURE_TEST_CASE( SnapshotTest, BtrfsFixture,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2, getSampleLastConsensusBlock() ) );
    BOOST_REQUIRE_THROW( mgr.doSnapshot( 2, getSampleLastConsensusBlock() ),
       SnapshotManager::SnapshotPresent );

    chmod( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" ).c_str(), 0 );

    dropRoot();

    BOOST_REQUIRE_EXCEPTION( mgr.doSnapshot( 3 ,
        getSampleLastConsensusBlock()), SnapshotManager::CannotRead,
        [this]( const SnapshotManager::CannotRead& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "snapshots" / "3";
        } );

    gainRoot();
    chmod( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" ).c_str(), 0111 );
    dropRoot();

    BOOST_REQUIRE_EXCEPTION( mgr.doSnapshot( 3, getSampleLastConsensusBlock() ),
        SnapshotManager::CannotCreate,
        [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "snapshots" / "3";
        } );

    // cannot delete
    BOOST_REQUIRE_THROW( mgr.restoreSnapshot( 2 ),
                                 SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_FIXTURE_TEST_CASE( RestoreTest, BtrfsFixture,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    BOOST_REQUIRE_THROW( mgr.restoreSnapshot( 2 ),
                                              SnapshotManager::SnapshotAbsent );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2 , getSampleLastConsensusBlock()) );

    BOOST_REQUIRE_NO_THROW( mgr.restoreSnapshot( 2 ) );

    BOOST_REQUIRE_EQUAL(
        0, btrfs.subvolume._delete(
               ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" ).c_str() ) );
    BOOST_REQUIRE_THROW( mgr.restoreSnapshot( 2 ), SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_FIXTURE_TEST_CASE( DiffTest, BtrfsFixture,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );
    mgr.doSnapshot( 2, getSampleLastConsensusBlock() );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "vol1" / "dir" );
    mgr.doSnapshot( 4, getSampleLastConsensusBlock() );

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

    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol1" ).c_str() );

    BOOST_REQUIRE_THROW(
        tmp = mgr.makeOrGetDiff( 4 ), SnapshotManager::CannotPerformBtrfsOperation );
}

// TODO Tests to check no files left in /tmp?!

BOOST_FIXTURE_TEST_CASE( ImportTest, BtrfsFixture,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    BOOST_REQUIRE_THROW( mgr.importDiff( 8 ), SnapshotManager::InvalidPath );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2, getSampleLastConsensusBlock() ) );
    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 4, getSampleLastConsensusBlock() ) );

    fs::path diff24;
    BOOST_REQUIRE_NO_THROW( diff24 = mgr.makeOrGetDiff( 4 ) );

    BOOST_REQUIRE_THROW( mgr.importDiff( 4 ), SnapshotManager::SnapshotPresent );

    // delete dest
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol1" ).c_str() );
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol2" ).c_str() );
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" );

    BOOST_REQUIRE_NO_THROW( mgr.importDiff( 4 ) );

    // delete dest
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol1" ).c_str() );
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol2" ).c_str() );
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" );

    // no source
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" ).c_str() );

    // BOOST_REQUIRE_THROW( mgr.importDiff( 2, 4 ), SnapshotManager::CannotPerformBtrfsOperation );

    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol2" ).c_str() );
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" );
    // BOOST_REQUIRE_THROW( mgr.importDiff( 2, 4 ), SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_FIXTURE_TEST_CASE( SnapshotRotationTest, BtrfsFixture,
    
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 1, getSampleLastConsensusBlock() ) );
    sleep( 1 );
    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 0, getSampleLastConsensusBlock() ) );
    sleep( 1 );
    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2, getSampleLastConsensusBlock() ) );
    sleep( 1 );
    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 3, getSampleLastConsensusBlock() ) );

    BOOST_REQUIRE_NO_THROW( mgr.leaveNLastSnapshots( 2 ) );

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "0" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "3" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" ) );
}

BOOST_FIXTURE_TEST_CASE( DiffRotationTest, BtrfsFixture,
    
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

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
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    mgr.doSnapshot( 1 , getSampleLastConsensusBlock() );
    mgr.doSnapshot( 2, getSampleLastConsensusBlock() );

    mgr.cleanupButKeepSnapshot( 1 );

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" ) );

    BOOST_REQUIRE_NO_THROW( mgr.restoreSnapshot( 1 ) );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2, getSampleLastConsensusBlock() ) );
}

BOOST_FIXTURE_TEST_CASE( CleanupTest, BtrfsFixture,

    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    mgr.doSnapshot( 1, getSampleLastConsensusBlock() );
    mgr.doSnapshot( 2, getSampleLastConsensusBlock() );

    mgr.cleanup();

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots") );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1") );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2") );

    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "diffs" ) );

    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol2" ) );
}

BOOST_AUTO_TEST_SUITE_END()
