#pragma once

#include <libdevcore/Log.h>

#include <boost/filesystem.hpp>
#include <cstdint>
#include <string>

namespace skale {

class StateProgressLog {
public:
    enum class Status : uint8_t { Started = 0, Completed = 1 };

    explicit StateProgressLog( const boost::filesystem::path& _dataDir );

    void markBlockCommitStarted( uint64_t _blockNumber );
    void markBlockCommitCompleted( uint64_t _blockNumber );

    bool isBlockCommitCompleted( uint64_t _blockNumber ) const;

    static const std::string PROGRESS_LOG_DIR;
    static const std::string PROGRESS_LOG_FILE;

private:
    void writeStatus( uint64_t _blockNumber, Status _status );
    bool readStatus( uint64_t& _blockNumber, Status& _status ) const;

    static const std::string STATUS_STARTED;
    static const std::string STATUS_COMPLETED;

    boost::filesystem::path m_progressLogPath;
    boost::filesystem::path m_tmpPath;

    dev::Logger m_logger{ dev::createLogger( dev::VerbosityWarning, "StateProgressLog" ) };
};

}  // namespace skale
