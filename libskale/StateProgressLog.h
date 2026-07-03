#pragma once

#include <libdevcore/Log.h>
#include <libethereum/TransactionReceipt.h>

#include <boost/filesystem.hpp>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace skale {

struct CommittedProgressData {
    uint64_t blockNumber;
    uint8_t status;  // 0 = started, 1 = completed
    uint64_t timestamp;
    dev::eth::TransactionReceipts receipts;
#ifdef BITE
    std::deque< dev::eth::Transaction > ctxsCreatedInBlock;
#endif
};

class StateProgressLog {
public:
    enum class Status : uint8_t { Started = 0, Completed = 1 };

    explicit StateProgressLog( const boost::filesystem::path& _dataDir );

    void markBlockCommitStarted( uint64_t _blockNumber );
    void markBlockCommitCompleted(
        uint64_t _blockNumber, const dev::eth::TransactionReceipts& _receipts, uint64_t _timestamp
#ifdef BITE
        ,
        const std::deque< dev::eth::Transaction >& _ctxsCreatedInBlock
#endif
    );

    bool isBlockCommitCompleted( uint64_t _blockNumber ) const;
    bool isBlockCommitStartedButNotCompleted( uint64_t _blockNumber ) const;

    std::optional< CommittedProgressData > loadProgressData() const;

    inline static const std::string PROGRESS_LOG_DIR = "progress_log";
    inline static const std::string PROGRESS_LOG_FILE = "state_progress";

private:
    // RLP format: [blockNumber, status, timestamp, [receipt0_rlp, receipt1_rlp, ...], [ctx0_rlp,
    // ctx1_rlp, ...]]
    void writeProgressData( const CommittedProgressData& _data );

    boost::filesystem::path m_progressLogPath;
    boost::filesystem::path m_tmpPath;

    mutable dev::Logger m_logger{ dev::createLogger( dev::VerbosityWarning, "StateProgressLog" ) };
#ifdef BITE
    static constexpr size_t rlpItemsCount = 5;
#else
    static constexpr size_t rlpItemsCount = 4;
#endif
};

}  // namespace skale
