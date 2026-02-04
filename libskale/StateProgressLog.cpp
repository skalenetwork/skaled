#include "StateProgressLog.h"

#include <libdevcore/RLP.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = boost::filesystem;

namespace skale {

StateProgressLog::StateProgressLog( const fs::path& _dataDir ) {
    fs::path progressLogDir = _dataDir / PROGRESS_LOG_DIR;
    fs::create_directories( progressLogDir );

    m_progressLogPath = progressLogDir / PROGRESS_LOG_FILE;
    m_tmpPath = progressLogDir / ( std::string( PROGRESS_LOG_FILE ) + ".tmp" );
    m_progressDataPath = progressLogDir / PROGRESS_DATA_FILE;
    m_progressDataTmpPath = progressLogDir / ( std::string( PROGRESS_DATA_FILE ) + ".tmp" );
}

void StateProgressLog::markBlockCommitStarted( uint64_t _blockNumber ) {
    uint64_t storedBlockNumber = 0;
    Status storedStatus = Status::Started;
    if ( readStatus( storedBlockNumber, storedStatus ) ) {
        if ( storedStatus != Status::Completed && storedBlockNumber != _blockNumber ) {
            BOOST_THROW_EXCEPTION( std::runtime_error(
                "State progress inconsistency: previous block " +
                std::to_string( storedBlockNumber ) + " not completed, but trying to start block " +
                std::to_string( _blockNumber ) ) );
        }
    }
    writeStatus( _blockNumber, Status::Started );
}

void StateProgressLog::markBlockCommitCompleted( uint64_t _blockNumber ) {
    writeStatus( _blockNumber, Status::Completed );
}

void StateProgressLog::writeStatus( uint64_t _blockNumber, Status _status ) {
    {
        std::ofstream tmpFile( m_tmpPath, std::ios::out | std::ios::trunc );
        if ( !tmpFile ) {
            BOOST_LOG( m_logger ) << "Failed to open tmp file: " << m_tmpPath;
            return;
        }

        const std::string_view statusStr =
            ( _status == Status::Completed ) ? STATUS_COMPLETED : STATUS_STARTED;

        tmpFile << _blockNumber << ": " << statusStr << "\n";

        tmpFile.flush();
        if ( !tmpFile ) {
            BOOST_LOG( m_logger ) << "Write failure (disk full?): " << m_tmpPath;
            return;
        }
    }

    boost::system::error_code ec;
    fs::rename( m_tmpPath, m_progressLogPath, ec );

    if ( ec ) {
        BOOST_LOG( m_logger ) << "Rename error: " << ec.message();
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

void StateProgressLog::saveCommittedProgressData(
    const dev::eth::TransactionReceipts& _receipts, uint64_t _timestamp ) {
    dev::RLPStream rlpStream;
    rlpStream.appendList( 2 );
    rlpStream << _timestamp;

    dev::RLPStream receiptsStream;
    receiptsStream.appendList( _receipts.size() );
    for ( const auto& receipt : _receipts ) {
        receiptsStream.appendRaw( receipt.rlp() );
    }
    rlpStream.appendRaw( receiptsStream.out() );

    dev::bytes encoded = rlpStream.out();

    {
        std::ofstream tmpFile(
            m_progressDataTmpPath, std::ios::out | std::ios::binary | std::ios::trunc );
        if ( !tmpFile ) {
            BOOST_LOG( m_logger ) << "Failed to open receipts tmp file: " << m_progressDataTmpPath;
            return;
        }

        tmpFile.write( reinterpret_cast< const char* >( encoded.data() ), encoded.size() );

        tmpFile.flush();
        if ( !tmpFile ) {
            BOOST_LOG( m_logger ) << "Write failure for receipts (disk full?): "
                                  << m_progressDataTmpPath;
            return;
        }
    }

    boost::system::error_code ec;
    fs::rename( m_progressDataTmpPath, m_progressDataPath, ec );

    if ( ec ) {
        BOOST_LOG( m_logger ) << "Receipts rename error: " << ec.message();
    }
}

std::optional< CommittedProgressData > StateProgressLog::loadCommittedProgressData() const {
    std::ifstream file( m_progressDataPath, std::ios::in | std::ios::binary );
    if ( !file ) {
        return std::nullopt;
    }

    dev::bytes encoded(
        ( std::istreambuf_iterator< char >( file ) ), std::istreambuf_iterator< char >() );

    if ( encoded.empty() ) {
        return std::nullopt;
    }

    try {
        dev::RLP rlp( encoded );

        if ( !rlp.isList() || rlp.itemCount() != 2 ) {
            BOOST_LOG( m_logger ) << "Invalid receipts format: expected list of 2 items";
            return std::nullopt;
        }

        CommittedProgressData data;
        data.timestamp = rlp[0].toInt< uint64_t >();

        for ( auto const& item : rlp[1] ) {
            data.receipts.emplace_back( item.data() );
        }
        return data;
    } catch ( const std::exception& ex ) {
        BOOST_LOG( m_logger ) << "Failed to decode receipts: " << ex.what();
        return std::nullopt;
    }
}

}  // namespace skale
