#include <libskale/StateProgressLog.h>

#include <libdevcore/TransientDirectory.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <fstream>

using namespace skale;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE( StateProgressLogSuite )

std::string readFileContent( const fs::path& path ) {
    std::ifstream file( path.string() );
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

BOOST_AUTO_TEST_CASE( directory_creation ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    fs::path expectedDir = fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR;
    BOOST_CHECK( fs::exists( expectedDir ) );
    BOOST_CHECK( fs::is_directory( expectedDir ) );
}

BOOST_AUTO_TEST_CASE( mark_block_commit_started ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );
    log.markBlockCommitStarted( 12345 );

    fs::path logFile = fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR /
                       StateProgressLog::PROGRESS_LOG_FILE;
    BOOST_REQUIRE( fs::exists( logFile ) );

    std::string content = readFileContent( logFile );
    BOOST_CHECK_EQUAL( content, "12345: started\n" );
}

BOOST_AUTO_TEST_CASE( mark_block_commit_completed ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );
    log.markBlockCommitCompleted( 67890 );

    fs::path logFile = fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR /
                       StateProgressLog::PROGRESS_LOG_FILE;
    BOOST_REQUIRE( fs::exists( logFile ) );

    std::string content = readFileContent( logFile );
    BOOST_CHECK_EQUAL( content, "67890: completed\n" );
}

BOOST_AUTO_TEST_CASE( is_block_commit_completed_true ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );
    log.markBlockCommitCompleted( 100 );

    BOOST_CHECK( log.isBlockCommitCompleted( 100 ) );
}

BOOST_AUTO_TEST_CASE( is_block_commit_completed_false_when_started ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );
    log.markBlockCommitStarted( 100 );

    BOOST_CHECK( !log.isBlockCommitCompleted( 100 ) );
}

BOOST_AUTO_TEST_CASE( is_block_commit_completed_false_wrong_block ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );
    log.markBlockCommitCompleted( 100 );

    BOOST_CHECK( !log.isBlockCommitCompleted( 101 ) );
}

BOOST_AUTO_TEST_CASE( is_block_commit_completed_false_no_file ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    BOOST_CHECK( !log.isBlockCommitCompleted( 100 ) );
}

BOOST_AUTO_TEST_CASE( file_overwrite ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitStarted( 1 );
    log.markBlockCommitCompleted( 1 );
    log.markBlockCommitStarted( 2 );

    fs::path logFile = fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR /
                       StateProgressLog::PROGRESS_LOG_FILE;
    std::string content = readFileContent( logFile );
    BOOST_CHECK_EQUAL( content, "2: started\n" );
}

BOOST_AUTO_TEST_CASE( large_block_number ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    uint64_t largeBlockNumber = 18446744073709551615ULL;
    log.markBlockCommitCompleted( largeBlockNumber );

    BOOST_CHECK( log.isBlockCommitCompleted( largeBlockNumber ) );

    fs::path logFile = fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR /
                       StateProgressLog::PROGRESS_LOG_FILE;
    std::string content = readFileContent( logFile );
    BOOST_CHECK_EQUAL( content, "18446744073709551615: completed\n" );
}

BOOST_AUTO_TEST_CASE( zero_block_number ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitCompleted( 0 );

    BOOST_CHECK( log.isBlockCommitCompleted( 0 ) );

    fs::path logFile = fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR /
                       StateProgressLog::PROGRESS_LOG_FILE;
    std::string content = readFileContent( logFile );
    BOOST_CHECK_EQUAL( content, "0: completed\n" );
}

BOOST_AUTO_TEST_CASE( started_then_completed_sequence ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitStarted( 42 );
    BOOST_CHECK( !log.isBlockCommitCompleted( 42 ) );

    log.markBlockCommitCompleted( 42 );
    BOOST_CHECK( log.isBlockCommitCompleted( 42 ) );
}

BOOST_AUTO_TEST_CASE( persistence_across_instances ) {
    dev::TransientDirectory tempDir;

    {
        StateProgressLog log( tempDir.path() );
        log.markBlockCommitCompleted( 500 );
    }

    {
        StateProgressLog log( tempDir.path() );
        BOOST_CHECK( log.isBlockCommitCompleted( 500 ) );
    }
}

BOOST_AUTO_TEST_CASE( persistence_started_status ) {
    dev::TransientDirectory tempDir;

    {
        StateProgressLog log( tempDir.path() );
        log.markBlockCommitStarted( 600 );
    }

    {
        StateProgressLog log( tempDir.path() );
        BOOST_CHECK( !log.isBlockCommitCompleted( 600 ) );
    }
}

BOOST_AUTO_TEST_CASE( corrupted_file_content ) {
    dev::TransientDirectory tempDir;

    fs::path progressLogDir =
        fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR;
    fs::create_directories( progressLogDir );

    fs::path logFile = progressLogDir / StateProgressLog::PROGRESS_LOG_FILE;
    {
        std::ofstream file( logFile.string() );
        file << "not a valid format";
    }

    StateProgressLog log( tempDir.path() );
    BOOST_CHECK( !log.isBlockCommitCompleted( 0 ) );
    BOOST_CHECK( !log.isBlockCommitCompleted( 100 ) );
}

BOOST_AUTO_TEST_CASE( empty_file ) {
    dev::TransientDirectory tempDir;

    fs::path progressLogDir =
        fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR;
    fs::create_directories( progressLogDir );

    fs::path logFile = progressLogDir / StateProgressLog::PROGRESS_LOG_FILE;
    {
        std::ofstream file( logFile.string() );
    }

    StateProgressLog log( tempDir.path() );
    BOOST_CHECK( !log.isBlockCommitCompleted( 0 ) );
}

BOOST_AUTO_TEST_CASE( consecutive_block_numbers ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    for ( uint64_t block = 1; block <= 100; ++block ) {
        log.markBlockCommitStarted( block );
        BOOST_CHECK( !log.isBlockCommitCompleted( block ) );

        log.markBlockCommitCompleted( block );
        BOOST_CHECK( log.isBlockCommitCompleted( block ) );
    }

    BOOST_CHECK( log.isBlockCommitCompleted( 100 ) );
    BOOST_CHECK( !log.isBlockCommitCompleted( 99 ) );
}

BOOST_AUTO_TEST_CASE( crash_after_mark_started ) {
    dev::TransientDirectory tempDir;

    {
        StateProgressLog log( tempDir.path() );
        log.markBlockCommitStarted( 10 );
    }

    {
        StateProgressLog log( tempDir.path() );
        BOOST_CHECK( !log.isBlockCommitCompleted( 10 ) );

        log.markBlockCommitStarted( 10 );
        log.markBlockCommitCompleted( 10 );

        BOOST_CHECK( log.isBlockCommitCompleted( 10 ) );
    }
}

BOOST_AUTO_TEST_CASE( skip_already_committed_block ) {
    dev::TransientDirectory tempDir;

    {
        StateProgressLog log( tempDir.path() );
        log.markBlockCommitStarted( 40 );
        log.markBlockCommitCompleted( 40 );
    }

    {
        StateProgressLog log( tempDir.path() );
        BOOST_CHECK( log.isBlockCommitCompleted( 40 ) );
    }
}

BOOST_AUTO_TEST_CASE( reprocess_incomplete_block ) {
    dev::TransientDirectory tempDir;

    {
        StateProgressLog log( tempDir.path() );
        log.markBlockCommitStarted( 50 );
    }

    {
        StateProgressLog log( tempDir.path() );
        BOOST_CHECK( !log.isBlockCommitCompleted( 50 ) );

        log.markBlockCommitStarted( 50 );
        log.markBlockCommitCompleted( 50 );

        BOOST_CHECK( log.isBlockCommitCompleted( 50 ) );
    }
}

BOOST_AUTO_TEST_CASE( multiple_blocks_with_crash_in_middle ) {
    dev::TransientDirectory tempDir;

    {
        StateProgressLog log( tempDir.path() );
        for ( uint64_t block = 1; block <= 5; ++block ) {
            log.markBlockCommitStarted( block );
            log.markBlockCommitCompleted( block );
        }
        log.markBlockCommitStarted( 6 );
    }

    {
        StateProgressLog log( tempDir.path() );

        BOOST_CHECK( !log.isBlockCommitCompleted( 5 ) );
        BOOST_CHECK( !log.isBlockCommitCompleted( 6 ) );

        log.markBlockCommitStarted( 6 );
        log.markBlockCommitCompleted( 6 );

        BOOST_CHECK( log.isBlockCommitCompleted( 6 ) );
    }
}

BOOST_AUTO_TEST_CASE( restart_same_block_allowed ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitStarted( 70 );
    log.markBlockCommitStarted( 70 );

    log.markBlockCommitCompleted( 70 );
    BOOST_CHECK( log.isBlockCommitCompleted( 70 ) );
}

BOOST_AUTO_TEST_CASE( start_next_block_after_completion ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitStarted( 80 );
    log.markBlockCommitCompleted( 80 );

    log.markBlockCommitStarted( 81 );
    BOOST_CHECK( !log.isBlockCommitCompleted( 80 ) );
    BOOST_CHECK( !log.isBlockCommitCompleted( 81 ) );

    log.markBlockCommitCompleted( 81 );
    BOOST_CHECK( log.isBlockCommitCompleted( 81 ) );
}

BOOST_AUTO_TEST_SUITE_END()
