#pragma once

#include <libdevcore/Log.h>
#include <libethereum/TransactionReceipt.h>

#include <boost/filesystem.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace skale {

class StateProgressLog {
public:
    enum class Status : uint8_t { Started = 0, Completed = 1 };

    explicit StateProgressLog( const boost::filesystem::path& _dataDir );

    void markBlockCommitStarted( uint64_t _blockNumber );
    void markBlockCommitCompleted( uint64_t _blockNumber );

    void saveCommittedReceipts( const dev::eth::TransactionReceipts& _receipts );
    std::optional< dev::eth::TransactionReceipts > loadCommittedReceipts() const;

    bool isBlockCommitCompleted( uint64_t _blockNumber ) const;

    inline static const std::string PROGRESS_LOG_DIR = "progress_log";
    inline static const std::string PROGRESS_LOG_FILE = "last_state_committed_block";
    inline static const std::string RECEIPTS_FILE = "committed_transaction_receipts";

private:
    void writeStatus( uint64_t _blockNumber, Status _status );
    bool readStatus( uint64_t& _blockNumber, Status& _status ) const;

    inline static const std::string STATUS_STARTED = "started";
    inline static const std::string STATUS_COMPLETED = "completed";

    boost::filesystem::path m_progressLogPath;
    boost::filesystem::path m_tmpPath;
    boost::filesystem::path m_receiptsPath;
    boost::filesystem::path m_receiptsTmpPath;

    mutable dev::Logger m_logger{ dev::createLogger( dev::VerbosityWarning, "StateProgressLog" ) };
};

}  // namespace skale
