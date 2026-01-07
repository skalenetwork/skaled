#include "StateProgressLog.h"

#include <fstream>
#include <sstream>
#include <string>

namespace fs = boost::filesystem;

namespace skale {

const std::string StateProgressLog::PROGRESS_LOG_DIR = "progress_log";
const std::string StateProgressLog::PROGRESS_LOG_FILE = "last_state_committed_block";
const std::string StateProgressLog::STATUS_STARTED = "started";
const std::string StateProgressLog::STATUS_COMPLETED = "completed";

StateProgressLog::StateProgressLog( const fs::path& _dataDir ) {
    fs::path progressLogDir = _dataDir / PROGRESS_LOG_DIR;
    fs::create_directories( progressLogDir );

    m_progressLogPath = progressLogDir / PROGRESS_LOG_FILE;
    m_tmpPath = progressLogDir / ( std::string( PROGRESS_LOG_FILE ) + ".tmp" );
}

void StateProgressLog::markBlockCommitStarted( uint64_t _blockNumber ) {
    writeStatus( _blockNumber, Status::Started );
}

void StateProgressLog::markBlockCommitCompleted( uint64_t _blockNumber ) {
    writeStatus( _blockNumber, Status::Completed );
}

void StateProgressLog::writeStatus( uint64_t _blockNumber, Status _status ) {
    std::ofstream tmpFile( m_tmpPath.string(), std::ios::trunc );
    if ( !tmpFile ) {
        BOOST_LOG( m_logger ) << "Failed to open tmp file for writing: " << m_tmpPath;
        return;
    }

    const std::string& statusStr =
        ( _status == Status::Completed ) ? STATUS_COMPLETED : STATUS_STARTED;
    tmpFile << _blockNumber << ": " << statusStr << "\n";
    tmpFile.close();

    if ( !tmpFile ) {
        BOOST_LOG( m_logger ) << "Failed to write to tmp file: " << m_tmpPath;
        return;
    }

    boost::system::error_code ec;
    fs::rename( m_tmpPath, m_progressLogPath, ec );
    if ( ec ) {
        BOOST_LOG( m_logger ) << "Failed to rename tmp file: " << ec.message();
    }
}

bool StateProgressLog::readStatus( uint64_t& _blockNumber, Status& _status ) const {
    std::ifstream file( m_progressLogPath.string() );
    if ( !file ) {
        return false;
    }

    std::string line;
    if ( !std::getline( file, line ) ) {
        return false;
    }

    std::istringstream iss( line );
    char colon;
    std::string statusStr;

    if ( !( iss >> _blockNumber >> colon >> statusStr ) || colon != ':' ) {
        return false;
    }

    _status = ( statusStr == STATUS_COMPLETED ) ? Status::Completed : Status::Started;
    return true;
}

bool StateProgressLog::isBlockCommitCompleted( uint64_t _blockNumber ) const {
    uint64_t storedBlockNumber = 0;
    Status storedStatus = Status::Started;

    if ( !readStatus( storedBlockNumber, storedStatus ) ) {
        return false;
    }

    return storedBlockNumber == _blockNumber && storedStatus == Status::Completed;
}

}  // namespace skale
