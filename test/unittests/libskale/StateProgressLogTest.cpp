#include <libskale/StateProgressLog.h>

#include <libdevcore/TransientDirectory.h>
#include <libethereum/TransactionReceipt.h>

#ifdef BITE2
#include <libethcore/BITECommon.h>
#include <libethereum/SchainPatch.h>
#include <libethereum/Transaction.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <fstream>

using namespace skale;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE( StateProgressLogSuite )

static const dev::eth::TransactionReceipts emptyReceipts;
#ifdef BITE2
static const std::deque< dev::eth::Transaction > emptyCtxs;
static const dev::eth::Transaction defaultCtx;
#endif

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

    auto data = log.loadProgressData();
    BOOST_REQUIRE( data.has_value() );
    BOOST_CHECK_EQUAL( data->blockNumber, 12345 );
    BOOST_CHECK_EQUAL( data->status, 0 );  // Started
}

BOOST_AUTO_TEST_CASE( mark_block_commit_completed ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );
    log.markBlockCommitCompleted( 67890, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    auto data = log.loadProgressData();
    BOOST_REQUIRE( data.has_value() );
    BOOST_CHECK_EQUAL( data->blockNumber, 67890 );
    BOOST_CHECK_EQUAL( data->status, 1 );  // Completed
}

BOOST_AUTO_TEST_CASE( is_block_commit_completed_true ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );
    log.markBlockCommitCompleted( 100, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

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
    log.markBlockCommitCompleted( 100, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

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
    log.markBlockCommitCompleted( 1, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );
    log.markBlockCommitStarted( 2 );

    auto data = log.loadProgressData();
    BOOST_REQUIRE( data.has_value() );
    BOOST_CHECK_EQUAL( data->blockNumber, 2 );
    BOOST_CHECK_EQUAL( data->status, 0 );  // Started
}

BOOST_AUTO_TEST_CASE( large_block_number ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    uint64_t largeBlockNumber = 18446744073709551615ULL;
    log.markBlockCommitCompleted( largeBlockNumber, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    BOOST_CHECK( log.isBlockCommitCompleted( largeBlockNumber ) );
}

BOOST_AUTO_TEST_CASE( zero_block_number ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitCompleted( 0, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    BOOST_CHECK( log.isBlockCommitCompleted( 0 ) );
}

BOOST_AUTO_TEST_CASE( started_then_completed_sequence ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitStarted( 42 );
    BOOST_CHECK( !log.isBlockCommitCompleted( 42 ) );

    log.markBlockCommitCompleted( 42, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );
    BOOST_CHECK( log.isBlockCommitCompleted( 42 ) );
}

BOOST_AUTO_TEST_CASE( persistence_across_instances ) {
    dev::TransientDirectory tempDir;

    {
        StateProgressLog log( tempDir.path() );
        log.markBlockCommitCompleted( 500, emptyReceipts, 0
#ifdef BITE2
            ,
            emptyCtxs
#endif
        );
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

    fs::path progressLogDir = fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR;
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

    fs::path progressLogDir = fs::path( tempDir.path() ) / StateProgressLog::PROGRESS_LOG_DIR;
    fs::create_directories( progressLogDir );

    fs::path logFile = progressLogDir / StateProgressLog::PROGRESS_LOG_FILE;
    { std::ofstream file( logFile.string() ); }

    StateProgressLog log( tempDir.path() );
    BOOST_CHECK( !log.isBlockCommitCompleted( 0 ) );
}

BOOST_AUTO_TEST_CASE( consecutive_block_numbers ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    for ( uint64_t block = 1; block <= 100; ++block ) {
        log.markBlockCommitStarted( block );
        BOOST_CHECK( !log.isBlockCommitCompleted( block ) );

        log.markBlockCommitCompleted( block, emptyReceipts, 0
#ifdef BITE2
            ,
            emptyCtxs
#endif
        );
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
        log.markBlockCommitCompleted( 10, emptyReceipts, 0
#ifdef BITE2
            ,
            emptyCtxs
#endif
        );

        BOOST_CHECK( log.isBlockCommitCompleted( 10 ) );
    }
}

BOOST_AUTO_TEST_CASE( skip_already_committed_block ) {
    dev::TransientDirectory tempDir;

    {
        StateProgressLog log( tempDir.path() );
        log.markBlockCommitStarted( 40 );
        log.markBlockCommitCompleted( 40, emptyReceipts, 0
#ifdef BITE2
            ,
            emptyCtxs
#endif
        );
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
        log.markBlockCommitCompleted( 50, emptyReceipts, 0
#ifdef BITE2
            ,
            emptyCtxs
#endif
        );

        BOOST_CHECK( log.isBlockCommitCompleted( 50 ) );
    }
}

BOOST_AUTO_TEST_CASE( multiple_blocks_with_crash_in_middle ) {
    dev::TransientDirectory tempDir;

    {
        StateProgressLog log( tempDir.path() );
        for ( uint64_t block = 1; block <= 5; ++block ) {
            log.markBlockCommitStarted( block );
            log.markBlockCommitCompleted( block, emptyReceipts, 0
#ifdef BITE2
                ,
                emptyCtxs
#endif
            );
        }
        log.markBlockCommitStarted( 6 );
    }

    {
        StateProgressLog log( tempDir.path() );

        BOOST_CHECK( !log.isBlockCommitCompleted( 5 ) );
        BOOST_CHECK( !log.isBlockCommitCompleted( 6 ) );

        log.markBlockCommitStarted( 6 );
        log.markBlockCommitCompleted( 6, emptyReceipts, 0
#ifdef BITE2
            ,
            emptyCtxs
#endif
        );

        BOOST_CHECK( log.isBlockCommitCompleted( 6 ) );
    }
}

BOOST_AUTO_TEST_CASE( restart_same_block_allowed ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitStarted( 70 );
    log.markBlockCommitStarted( 70 );

    log.markBlockCommitCompleted( 70, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );
    BOOST_CHECK( log.isBlockCommitCompleted( 70 ) );
}

BOOST_AUTO_TEST_CASE( start_next_block_after_completion ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitStarted( 80 );
    log.markBlockCommitCompleted( 80, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    log.markBlockCommitStarted( 81 );
    BOOST_CHECK( !log.isBlockCommitCompleted( 80 ) );
    BOOST_CHECK( !log.isBlockCommitCompleted( 81 ) );

    log.markBlockCommitCompleted( 81, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );
    BOOST_CHECK( log.isBlockCommitCompleted( 81 ) );
}

BOOST_AUTO_TEST_CASE( is_block_commit_started_but_not_completed ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    log.markBlockCommitStarted( 99 );
    BOOST_CHECK( log.isBlockCommitStartedButNotCompleted( 99 ) );
    BOOST_CHECK( !log.isBlockCommitStartedButNotCompleted( 100 ) );

    log.markBlockCommitCompleted( 99, emptyReceipts, 0
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );
    BOOST_CHECK( !log.isBlockCommitStartedButNotCompleted( 99 ) );
}

BOOST_AUTO_TEST_CASE( save_load_empty_receipts ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    uint64_t timestamp = 1700000000;
    log.markBlockCommitCompleted( 1, emptyReceipts, timestamp
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_CHECK_EQUAL( loaded->blockNumber, 1 );
    BOOST_CHECK_EQUAL( loaded->status, 1 );
    BOOST_CHECK( loaded->receipts.empty() );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );
}

BOOST_AUTO_TEST_CASE( save_load_single_receipt ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::eth::LogEntries logs;
    dev::eth::TransactionReceipt receipt( uint8_t( 1 ), dev::u256( 21000 ), logs );

    dev::eth::TransactionReceipts receipts;
    receipts.push_back( receipt );

    uint64_t timestamp = 1700000001;
    log.markBlockCommitCompleted( 2, receipts, timestamp
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_CHECK_EQUAL( loaded->blockNumber, 2 );
    BOOST_REQUIRE_EQUAL( loaded->receipts.size(), 1 );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );

    BOOST_CHECK( loaded->receipts[0].hasStatusCode() );
    BOOST_CHECK_EQUAL( loaded->receipts[0].statusCode(), 1 );
    BOOST_CHECK_EQUAL( loaded->receipts[0].cumulativeGasUsed(), dev::u256( 21000 ) );
    BOOST_CHECK( loaded->receipts[0].log().empty() );
}

BOOST_AUTO_TEST_CASE( save_load_multiple_receipts ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::eth::TransactionReceipts receipts;
    dev::eth::LogEntries emptyLogs;

    receipts.emplace_back( uint8_t( 1 ), dev::u256( 21000 ), emptyLogs );
    receipts.emplace_back( uint8_t( 1 ), dev::u256( 42000 ), emptyLogs );
    receipts.emplace_back( uint8_t( 0 ), dev::u256( 63000 ), emptyLogs );

    uint64_t timestamp = 1700000002;
    log.markBlockCommitCompleted( 3, receipts, timestamp
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_REQUIRE_EQUAL( loaded->receipts.size(), 3 );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );

    BOOST_CHECK_EQUAL( loaded->receipts[0].statusCode(), 1 );
    BOOST_CHECK_EQUAL( loaded->receipts[0].cumulativeGasUsed(), dev::u256( 21000 ) );

    BOOST_CHECK_EQUAL( loaded->receipts[1].statusCode(), 1 );
    BOOST_CHECK_EQUAL( loaded->receipts[1].cumulativeGasUsed(), dev::u256( 42000 ) );

    BOOST_CHECK_EQUAL( loaded->receipts[2].statusCode(), 0 );
    BOOST_CHECK_EQUAL( loaded->receipts[2].cumulativeGasUsed(), dev::u256( 63000 ) );
}

BOOST_AUTO_TEST_CASE( save_load_receipt_with_logs ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::Address contractAddress( "0x1234567890123456789012345678901234567890" );
    dev::h256 topic1( "0xabcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789" );
    dev::bytes logData = { 0x01, 0x02, 0x03, 0x04 };

    dev::eth::LogEntry logEntry( contractAddress, { topic1 }, logData );
    dev::eth::LogEntries logs = { logEntry };

    dev::eth::TransactionReceipt receipt( uint8_t( 1 ), dev::u256( 50000 ), logs );

    dev::eth::TransactionReceipts receipts;
    receipts.push_back( receipt );

    uint64_t timestamp = 1700000003;
    log.markBlockCommitCompleted( 4, receipts, timestamp
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_REQUIRE_EQUAL( loaded->receipts.size(), 1 );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );

    const auto& loadedReceipt = loaded->receipts[0];
    BOOST_CHECK_EQUAL( loadedReceipt.statusCode(), 1 );
    BOOST_CHECK_EQUAL( loadedReceipt.cumulativeGasUsed(), dev::u256( 50000 ) );

    BOOST_REQUIRE_EQUAL( loadedReceipt.log().size(), 1 );
    BOOST_CHECK_EQUAL( loadedReceipt.log()[0].address, contractAddress );
    BOOST_REQUIRE_EQUAL( loadedReceipt.log()[0].topics.size(), 1 );
    BOOST_CHECK_EQUAL( loadedReceipt.log()[0].topics[0], topic1 );
    BOOST_CHECK( loadedReceipt.log()[0].data == logData );
}

BOOST_AUTO_TEST_CASE( save_load_receipt_with_revert_reason ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::eth::LogEntries emptyLogs;
    std::string revertReason = "Insufficient balance";
    dev::eth::TransactionReceipt receipt(
        uint8_t( 0 ), dev::u256( 30000 ), emptyLogs, revertReason );

    dev::eth::TransactionReceipts receipts;
    receipts.push_back( receipt );

    uint64_t timestamp = 1700000004;
    log.markBlockCommitCompleted( 5, receipts, timestamp
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_REQUIRE_EQUAL( loaded->receipts.size(), 1 );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );

    BOOST_CHECK_EQUAL( loaded->receipts[0].statusCode(), 0 );
    BOOST_CHECK_EQUAL( loaded->receipts[0].cumulativeGasUsed(), dev::u256( 30000 ) );
    BOOST_CHECK_EQUAL( loaded->receipts[0].getRevertReason(), revertReason );
}

BOOST_AUTO_TEST_CASE( receipts_persistence_across_instances ) {
    dev::TransientDirectory tempDir;

    dev::eth::TransactionReceipts receipts;
    dev::eth::LogEntries emptyLogs;
    receipts.emplace_back( uint8_t( 1 ), dev::u256( 21000 ), emptyLogs );
    receipts.emplace_back( uint8_t( 1 ), dev::u256( 42000 ), emptyLogs );

    uint64_t timestamp = 1700000005;

    {
        StateProgressLog log( tempDir.path() );
        log.markBlockCommitCompleted( 6, receipts, timestamp
#ifdef BITE2
            ,
            emptyCtxs
#endif
        );
    }

    {
        StateProgressLog log( tempDir.path() );
        auto loaded = log.loadProgressData();
        BOOST_REQUIRE( loaded.has_value() );
        BOOST_REQUIRE_EQUAL( loaded->receipts.size(), 2 );
        BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );
        BOOST_CHECK_EQUAL( loaded->receipts[0].cumulativeGasUsed(), dev::u256( 21000 ) );
        BOOST_CHECK_EQUAL( loaded->receipts[1].cumulativeGasUsed(), dev::u256( 42000 ) );
    }
}

BOOST_AUTO_TEST_CASE( load_receipts_no_file ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    auto loaded = log.loadProgressData();
    BOOST_CHECK( !loaded.has_value() );
}

BOOST_AUTO_TEST_CASE( receipts_overwrite ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::eth::LogEntries emptyLogs;

    dev::eth::TransactionReceipts receipts1;
    receipts1.emplace_back( uint8_t( 1 ), dev::u256( 10000 ), emptyLogs );

    dev::eth::TransactionReceipts receipts2;
    receipts2.emplace_back( uint8_t( 0 ), dev::u256( 20000 ), emptyLogs );
    receipts2.emplace_back( uint8_t( 1 ), dev::u256( 40000 ), emptyLogs );

    uint64_t timestamp1 = 1700000006;
    uint64_t timestamp2 = 1700000007;
    log.markBlockCommitCompleted( 7, receipts1, timestamp1
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );
    log.markBlockCommitCompleted( 8, receipts2, timestamp2
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_CHECK_EQUAL( loaded->blockNumber, 8 );
    BOOST_REQUIRE_EQUAL( loaded->receipts.size(), 2 );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp2 );
    BOOST_CHECK_EQUAL( loaded->receipts[0].statusCode(), 0 );
    BOOST_CHECK_EQUAL( loaded->receipts[0].cumulativeGasUsed(), dev::u256( 20000 ) );
    BOOST_CHECK_EQUAL( loaded->receipts[1].statusCode(), 1 );
    BOOST_CHECK_EQUAL( loaded->receipts[1].cumulativeGasUsed(), dev::u256( 40000 ) );
}

BOOST_AUTO_TEST_CASE( save_load_receipt_bloom_preserved ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::Address contractAddress( "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
    dev::h256 topic( "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" );
    dev::eth::LogEntry logEntry( contractAddress, { topic }, {} );
    dev::eth::LogEntries logs = { logEntry };

    dev::eth::TransactionReceipt receipt( uint8_t( 1 ), dev::u256( 100000 ), logs );
    dev::eth::LogBloom originalBloom = receipt.bloom();

    dev::eth::TransactionReceipts receipts;
    receipts.push_back( receipt );

    uint64_t timestamp = 1700000008;
    log.markBlockCommitCompleted( 9, receipts, timestamp
#ifdef BITE2
        ,
        emptyCtxs
#endif
    );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_REQUIRE_EQUAL( loaded->receipts.size(), 1 );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );
    BOOST_CHECK( loaded->receipts[0].bloom() == originalBloom );
}

#ifdef BITE2

static dev::bytes makeCTXData( const dev::bytes& _payload = {} ) {
    dev::bytes data = dev::bite::ON_DECRYPT_FUNCTION_SELECTOR;
    data.insert( data.end(), _payload.begin(), _payload.end() );
    return data;
}

static dev::eth::Transaction makeSignedCTX( dev::u256 _value, dev::u256 _gasPrice, dev::u256 _gas,
    const dev::Address& _to, const dev::bytes& _payload, dev::u256 _nonce ) {
    dev::Secret secret( "0x1122334455667788112233445566778811223344556677881122334455667788" );
    return dev::eth::Transaction(
        _value, _gasPrice, _gas, _to, makeCTXData( _payload ), _nonce, secret );
}

BOOST_AUTO_TEST_CASE( save_load_single_ctx ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::Address dest( "0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" );
    dev::bytes payload = { 0x01, 0x02, 0x03 };
    auto ctx = makeSignedCTX( 1000, 20, 21000, dest, payload, 0 );

    std::deque< dev::eth::Transaction > ctxs = { ctx };

    uint64_t timestamp = 1700001000;
    log.markBlockCommitCompleted( 100, emptyReceipts, timestamp, ctxs );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_CHECK_EQUAL( loaded->blockNumber, 100 );
    BOOST_CHECK_EQUAL( loaded->status, 1 );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );
    BOOST_CHECK( loaded->receipts.empty() );
    BOOST_REQUIRE_EQUAL( loaded->ctxsCreatedInBlock.size(), 1 );

    const auto& loadedCtx = loaded->ctxsCreatedInBlock[0];
    BOOST_CHECK( loadedCtx.isCTX() );
    BOOST_CHECK_EQUAL( loadedCtx.value(), ctx.value() );
    BOOST_CHECK_EQUAL( loadedCtx.nonce(), ctx.nonce() );
    BOOST_CHECK( loadedCtx.data() == ctx.data() );
    BOOST_CHECK_EQUAL( loadedCtx.receiveAddress(), ctx.receiveAddress() );
}

BOOST_AUTO_TEST_CASE( save_load_multiple_ctxs ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::Address dest1( "0x1111111111111111111111111111111111111111" );
    dev::Address dest2( "0x2222222222222222222222222222222222222222" );
    dev::Address dest3( "0x3333333333333333333333333333333333333333" );

    auto ctx1 = makeSignedCTX( 100, 10, 21000, dest1, { 0xaa }, 0 );
    auto ctx2 = makeSignedCTX( 200, 20, 42000, dest2, { 0xbb, 0xcc }, 1 );
    auto ctx3 = makeSignedCTX( 300, 30, 63000, dest3, {}, 2 );

    std::deque< dev::eth::Transaction > ctxs = { ctx1, ctx2, ctx3 };

    uint64_t timestamp = 1700001001;
    log.markBlockCommitCompleted( 101, emptyReceipts, timestamp, ctxs );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_REQUIRE_EQUAL( loaded->ctxsCreatedInBlock.size(), 3 );

    BOOST_CHECK( loaded->ctxsCreatedInBlock[0].isCTX() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[0].value(), ctx1.value() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[0].receiveAddress(), dest1 );
    BOOST_CHECK( loaded->ctxsCreatedInBlock[0].data() == ctx1.data() );

    BOOST_CHECK( loaded->ctxsCreatedInBlock[1].isCTX() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[1].value(), ctx2.value() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[1].receiveAddress(), dest2 );
    BOOST_CHECK( loaded->ctxsCreatedInBlock[1].data() == ctx2.data() );

    BOOST_CHECK( loaded->ctxsCreatedInBlock[2].isCTX() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[2].value(), ctx3.value() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[2].receiveAddress(), dest3 );
    BOOST_CHECK( loaded->ctxsCreatedInBlock[2].data() == ctx3.data() );
}

BOOST_AUTO_TEST_CASE( save_load_ctxs_with_receipts ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    // Create receipts
    dev::eth::LogEntries emptyLogs;
    dev::eth::TransactionReceipts receipts;
    receipts.emplace_back( uint8_t( 1 ), dev::u256( 21000 ), emptyLogs );
    receipts.emplace_back( uint8_t( 0 ), dev::u256( 42000 ), emptyLogs );

    // Create CTXs
    dev::Address dest( "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
    auto ctx = makeSignedCTX( 500, 15, 30000, dest, { 0x01, 0x02 }, 5 );
    std::deque< dev::eth::Transaction > ctxs = { ctx };

    uint64_t timestamp = 1700001002;
    log.markBlockCommitCompleted( 102, receipts, timestamp, ctxs );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_CHECK_EQUAL( loaded->blockNumber, 102 );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );

    // Verify receipts
    BOOST_REQUIRE_EQUAL( loaded->receipts.size(), 2 );
    BOOST_CHECK_EQUAL( loaded->receipts[0].statusCode(), 1 );
    BOOST_CHECK_EQUAL( loaded->receipts[0].cumulativeGasUsed(), dev::u256( 21000 ) );
    BOOST_CHECK_EQUAL( loaded->receipts[1].statusCode(), 0 );
    BOOST_CHECK_EQUAL( loaded->receipts[1].cumulativeGasUsed(), dev::u256( 42000 ) );

    // Verify CTXs
    BOOST_REQUIRE_EQUAL( loaded->ctxsCreatedInBlock.size(), 1 );
    BOOST_CHECK( loaded->ctxsCreatedInBlock[0].isCTX() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[0].value(), ctx.value() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[0].receiveAddress(), dest );
}

BOOST_AUTO_TEST_CASE( save_load_empty_ctxs ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    uint64_t timestamp = 1700001003;
    log.markBlockCommitCompleted( 103, emptyReceipts, timestamp, emptyCtxs );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_CHECK_EQUAL( loaded->blockNumber, 103 );
    BOOST_CHECK( loaded->ctxsCreatedInBlock.empty() );
}

BOOST_AUTO_TEST_CASE( ctxs_persistence_across_instances ) {
    dev::TransientDirectory tempDir;

    dev::Address dest( "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" );
    auto ctx = makeSignedCTX( 777, 10, 21000, dest, { 0xde, 0xad }, 42 );
    std::deque< dev::eth::Transaction > ctxs = { ctx };

    uint64_t timestamp = 1700001004;

    {
        StateProgressLog log( tempDir.path() );
        log.markBlockCommitCompleted( 104, emptyReceipts, timestamp, ctxs );
    }

    {
        StateProgressLog log( tempDir.path() );
        auto loaded = log.loadProgressData();
        BOOST_REQUIRE( loaded.has_value() );
        BOOST_CHECK_EQUAL( loaded->blockNumber, 104 );
        BOOST_CHECK_EQUAL( loaded->timestamp, timestamp );
        BOOST_REQUIRE_EQUAL( loaded->ctxsCreatedInBlock.size(), 1 );
        BOOST_CHECK( loaded->ctxsCreatedInBlock[0].isCTX() );
        BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[0].value(), ctx.value() );
        BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[0].receiveAddress(), dest );
        BOOST_CHECK( loaded->ctxsCreatedInBlock[0].data() == ctx.data() );
    }
}

BOOST_AUTO_TEST_CASE( ctxs_overwrite ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::Address dest1( "0x1111111111111111111111111111111111111111" );
    dev::Address dest2( "0x2222222222222222222222222222222222222222" );

    auto ctx1 = makeSignedCTX( 100, 10, 21000, dest1, { 0x01 }, 0 );
    auto ctx2 = makeSignedCTX( 200, 20, 42000, dest2, { 0x02 }, 1 );

    uint64_t timestamp1 = 1700001005;
    uint64_t timestamp2 = 1700001006;

    log.markBlockCommitCompleted( 105, emptyReceipts, timestamp1, { ctx1 } );
    log.markBlockCommitCompleted( 106, emptyReceipts, timestamp2, { ctx2 } );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_CHECK_EQUAL( loaded->blockNumber, 106 );
    BOOST_CHECK_EQUAL( loaded->timestamp, timestamp2 );
    BOOST_REQUIRE_EQUAL( loaded->ctxsCreatedInBlock.size(), 1 );
    BOOST_CHECK( loaded->ctxsCreatedInBlock[0].isCTX() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[0].value(), ctx2.value() );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[0].receiveAddress(), dest2 );
}

BOOST_AUTO_TEST_CASE( save_load_ctx_with_larger_data ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    // Create transaction with a larger data payload
    dev::bytes largePayload( 1024, 0x42 );  // 1KB of 0x42
    dev::bytes expectedData = makeCTXData( largePayload );
    dev::Address dest( "0xcccccccccccccccccccccccccccccccccccccccc" );
    auto ctx = makeSignedCTX( 0, 100, 100000, dest, largePayload, 99 );

    std::deque< dev::eth::Transaction > ctxs = { ctx };

    uint64_t timestamp = 1700001007;
    log.markBlockCommitCompleted( 107, emptyReceipts, timestamp, ctxs );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_REQUIRE_EQUAL( loaded->ctxsCreatedInBlock.size(), 1 );
    BOOST_CHECK( loaded->ctxsCreatedInBlock[0].isCTX() );
    BOOST_CHECK( loaded->ctxsCreatedInBlock[0].data() == expectedData );
    BOOST_CHECK_EQUAL( loaded->ctxsCreatedInBlock[0].data().size(),
        1024 + dev::bite::ON_DECRYPT_FUNCTION_SELECTOR_SIZE_BYTES );
}

BOOST_AUTO_TEST_CASE( save_load_ctx_preserves_gas_fields ) {
    dev::TransientDirectory tempDir;

    StateProgressLog log( tempDir.path() );

    dev::Address dest( "0xdddddddddddddddddddddddddddddddddddddddd" );
    dev::u256 value = 12345;
    dev::u256 gasPrice = 5000000000;  // 5 gwei
    dev::u256 gas = 100000;
    dev::u256 nonce = 7;
    dev::bytes payload = { 0xa9, 0x05, 0x9c, 0xbb };
    dev::bytes expectedData = makeCTXData( payload );

    auto ctx = makeSignedCTX( value, gasPrice, gas, dest, payload, nonce );
    std::deque< dev::eth::Transaction > ctxs = { ctx };

    uint64_t timestamp = 1700001008;
    log.markBlockCommitCompleted( 108, emptyReceipts, timestamp, ctxs );

    auto loaded = log.loadProgressData();
    BOOST_REQUIRE( loaded.has_value() );
    BOOST_REQUIRE_EQUAL( loaded->ctxsCreatedInBlock.size(), 1 );

    const auto& loadedCtx = loaded->ctxsCreatedInBlock[0];
    BOOST_CHECK( loadedCtx.isCTX() );
    BOOST_CHECK_EQUAL( loadedCtx.value(), value );
    BOOST_CHECK_EQUAL( loadedCtx.nonce(), nonce );
    BOOST_CHECK( loadedCtx.data() == expectedData );
    BOOST_CHECK_EQUAL( loadedCtx.receiveAddress(), dest );
}

#endif

BOOST_AUTO_TEST_SUITE_END()
