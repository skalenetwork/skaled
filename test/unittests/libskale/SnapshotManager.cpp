#include <libskale/SnapshotManager.h>
#include <skutils/btrfs.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <stdlib.h>

using namespace std;
namespace fs = boost::filesystem;

struct BtrfsFixture {
    const string BTRFS_FILE_PATH = "btrfs.file";
    const string BTRFS_DIR_PATH = "btrfs";

    BtrfsFixture() {
        if ( geteuid() != 0 ) {
            cerr << "Need to be root" << endl;
            exit( -1 );
        }
        system( ( "dd if=/dev/zero of=" + BTRFS_FILE_PATH + " bs=1M count=200" ).c_str() );
        system( ( "mkfs.btrfs " + BTRFS_FILE_PATH ).c_str() );
        system( ( "mkdir " + BTRFS_DIR_PATH ).c_str() );

        system( ( "mount " + BTRFS_FILE_PATH + " " + BTRFS_DIR_PATH ).c_str() );

        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol1" ).c_str() );
        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol2" ).c_str() );
        system( ( "mkdir " + BTRFS_DIR_PATH + "/snapshots" ).c_str() );
    }

    ~BtrfsFixture() {
        system( ( "umount " + BTRFS_DIR_PATH ).c_str() );
        system( ( "rmdir " + BTRFS_DIR_PATH ).c_str() );
        system( ( "rm " + BTRFS_FILE_PATH ).c_str() );
    }
};

BOOST_FIXTURE_TEST_SUITE( BtrfsTestSuite, BtrfsFixture )

BOOST_AUTO_TEST_CASE( SampleTest ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );
    mgr.doSnapshot( 2 );
    mgr.doSnapshot( 4 );
    mgr.restoreSnapshot( 2 );
    fs::path diff24 = mgr.makeDiff( 2, 4 );
    fs::remove_all( BTRFS_DIR_PATH + "/snapshots/4" );
    mgr.importDiff( 4, diff24 );
    mgr.restoreSnapshot( 4 );
}

BOOST_AUTO_TEST_SUITE_END()
