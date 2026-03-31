#include "StateProgressLog.h"

#include <libdevcore/RLP.h>
#include <libethereum/SchainPatch.h>
#include <libethereum/Transaction.h>

#include <fstream>
#include <stdexcept>

namespace fs = boost::filesystem;

namespace skale {

StateProgressLog::StateProgressLog( const fs::path& _dataDir ) {
    fs::path progressLogDir = _dataDir / PROGRESS_LOG_DIR;
    fs::create_directories( progressLogDir );

    m_progressLogPath = progressLogDir / PROGRESS_LOG_FILE;
    m_tmpPath = progressLogDir / ( std::string( PROGRESS_LOG_FILE ) + ".tmp" );
}

void StateProgressLog::markBlockCommitStarted( uint64_t _blockNumber ) {
    auto existing = loadProgressData();
    if ( existing ) {
        if ( existing->status != static_cast< uint8_t >( Status::Completed ) &&
             existing->blockNumber != _blockNumber ) {
            BOOST_THROW_EXCEPTION( std::runtime_error(
                "State progress inconsistency: previous block " +
                std::to_string( existing->blockNumber ) +
                " not completed, but trying to start block " + std::to_string( _blockNumber ) ) );
        }
    }

    CommittedProgressData data;
    data.blockNumber = _blockNumber;
    data.status = static_cast< uint8_t >( Status::Started );
    data.timestamp = 0;
    writeProgressData( data );
}

void StateProgressLog::markBlockCommitCompleted(
    uint64_t _blockNumber, const dev::eth::TransactionReceipts& _receipts, uint64_t _timestamp
#ifdef BITE
    ,
    const std::deque< dev::eth::Transaction >& _ctxsCreatedInBlock
#endif
) {
    CommittedProgressData data;
    data.blockNumber = _blockNumber;
    data.status = static_cast< uint8_t >( Status::Completed );
    data.timestamp = _timestamp;
    data.receipts = _receipts;
#ifdef BITE
    data.ctxsCreatedInBlock = _ctxsCreatedInBlock;
#endif
    writeProgressData( data );
}

void StateProgressLog::writeProgressData( const CommittedProgressData& _data ) {
    dev::RLPStream rlpStream;
    rlpStream.appendList( rlpItemsCount );
    rlpStream << _data.blockNumber;
    rlpStream << _data.status;
    rlpStream << _data.timestamp;

    dev::RLPStream receiptsStream;
    receiptsStream.appendList( _data.receipts.size() );
    for ( const auto& receipt : _data.receipts ) {
        receiptsStream.appendRaw( receipt.rlp() );
    }
    rlpStream.appendRaw( receiptsStream.out() );

#ifdef BITE
    dev::RLPStream ctxsStream;
    ctxsStream.appendList( _data.ctxsCreatedInBlock.size() );
    for ( const auto& ctx : _data.ctxsCreatedInBlock ) {
        dev::RLPStream ctxEntry;
        ctxEntry.appendList( 2 );
        ctxEntry.appendRaw( ctx.toBytes() );
        ctxEntry << ctx.getCTXOrigin();
        ctxsStream.appendRaw( ctxEntry.out() );
    }
    rlpStream.appendRaw( ctxsStream.out() );
#endif

    dev::bytes encoded = rlpStream.out();

    {
        std::ofstream tmpFile( m_tmpPath, std::ios::out | std::ios::binary | std::ios::trunc );
        if ( !tmpFile ) {
            BOOST_LOG( m_logger ) << "Failed to open tmp file: " << m_tmpPath;
            return;
        }

        tmpFile.write( reinterpret_cast< const char* >( encoded.data() ), encoded.size() );

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

std::optional< CommittedProgressData > StateProgressLog::loadProgressData() const {
    std::ifstream file( m_progressLogPath, std::ios::in | std::ios::binary );
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

        if ( !rlp.isList() || rlp.itemCount() != rlpItemsCount ) {
            BOOST_LOG( m_logger ) << "Invalid progress data format: expected list of "
                                  << rlpItemsCount << " items";
            return std::nullopt;
        }

        CommittedProgressData data;
        data.blockNumber = rlp[0].toInt< uint64_t >();
        data.status = rlp[1].toInt< uint8_t >();
        data.timestamp = rlp[2].toInt< uint64_t >();

        for ( auto const& item : rlp[3] ) {
            data.receipts.emplace_back( item.data() );
        }

#ifdef BITE
        for ( auto const& item : rlp[4] ) {
            CHECK_EXPRESSION( item.isList() && item.itemCount() == 2 );
            dev::eth::Transaction tx( item[0].data(), dev::eth::CheckTransaction::None, true,
                EIP1559TransactionsPatch::isEnabledInWorkingBlock(),
                InvalidTransactionFormatPatch::isEnabledInWorkingBlock(),
                Bite2Patch::isEnabledInWorkingBlock() );
            tx.setCTXOrigin( item[1].toHash< dev::h256 >() );
            data.ctxsCreatedInBlock.push_back( std::move( tx ) );
        }
#endif
        return data;
    } catch ( const std::exception& ex ) {
        BOOST_LOG( m_logger ) << "Failed to decode progress data: " << ex.what();
        return std::nullopt;
    }
}

bool StateProgressLog::isBlockCommitCompleted( uint64_t _blockNumber ) const {
    auto data = loadProgressData();
    if ( !data ) {
        return false;
    }
    return data->blockNumber == _blockNumber &&
           data->status == static_cast< uint8_t >( Status::Completed );
}

bool StateProgressLog::isBlockCommitStartedButNotCompleted( uint64_t _blockNumber ) const {
    auto data = loadProgressData();
    if ( !data ) {
        return false;
    }
    return data->blockNumber == _blockNumber &&
           data->status == static_cast< uint8_t >( Status::Started );
}

}  // namespace skale
