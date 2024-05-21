#include "SnapshotAgent.h"

#include <libskale/SnapshotManager.h>

#include <libdevcore/Common.h>

#include <boost/chrono.hpp>

using namespace dev;

SnapshotAgent::SnapshotAgent( int64_t _snapshotIntervalSec,
    std::shared_ptr< SnapshotManager > _snapshotManager, SkaleDebugTracer& _debugTracer )
    : m_snapshotIntervalSec( _snapshotIntervalSec ),
      m_snapshotManager( _snapshotManager ),
      m_debugTracer( _debugTracer ) {
    if ( m_snapshotIntervalSec > 0 ) {
        LOG( m_logger ) << "Snapshots enabled, snapshotIntervalSec is: " << m_snapshotIntervalSec;
    }
}

void SnapshotAgent::init( unsigned _currentBlockNumber, int64_t _timestampOfBlock1 ) {
    if ( m_snapshotIntervalSec <= 0 )
        return;

    if ( _currentBlockNumber == 0 )
        doSnapshotAndComputeHash( 0 );

    auto latest_snapshots = this->m_snapshotManager->getLatestSnapshots();

    // if two
    if ( latest_snapshots.first ) {
        assert( latest_snapshots.first != 1 );  // 1 can never be snapshotted

        this->last_snapshoted_block_with_hash = latest_snapshots.first;

        // ignore second as it was "in hash computation"
        // check that both are imported!!
        // h256 h2 = this->hashFromNumber( latest_snapshots.second );
        // assert( h2 != h256() );
        // last_snapshot_creation_time = blockInfo( h2 ).timestamp();

        last_snapshot_creation_time =
            this->m_snapshotManager->getBlockTimestamp( latest_snapshots.second );

        if ( !m_snapshotManager->isSnapshotHashPresent( latest_snapshots.second ) )
            startHashComputingThread();

        // one snapshot
    } else if ( latest_snapshots.second ) {
        assert( latest_snapshots.second != 1 );  // 1 can never be snapshotted
        assert( _timestampOfBlock1 > 0 );        // we created snapshot somehow

        // whether it is local or downloaded - we shall ignore it's hash but use it's time
        // see also how last_snapshoted_block_with_hash is updated in importTransactionsAsBlock
        // h256 h2 = this->hashFromNumber( latest_snapshots.second );
        // uint64_t time_of_second = blockInfo( h2 ).timestamp();

        this->last_snapshoted_block_with_hash = -1;
        // last_snapshot_creation_time = time_of_second;

        last_snapshot_creation_time =
            this->m_snapshotManager->getBlockTimestamp( latest_snapshots.second );

        if ( !m_snapshotManager->isSnapshotHashPresent( latest_snapshots.second ) )
            startHashComputingThread();

        // no snapshots yet
    } else {
        this->last_snapshoted_block_with_hash = -1;
        // init last block creation time with only robust time source - timestamp of 1st block!
        last_snapshot_creation_time = _timestampOfBlock1;
    }

    LOG( m_logger ) << "Latest snapshots init: " << latest_snapshots.first << " "
                    << latest_snapshots.second << " -> " << this->last_snapshoted_block_with_hash;

    LOG( m_logger ) << "Init last snapshot creation time: " << this->last_snapshot_creation_time;
}

void SnapshotAgent::finishHashComputingAndUpdateHashesIfNeeded( int64_t _timestamp ) {
    if ( m_snapshotIntervalSec > 0 && this->isTimeToDoSnapshot( _timestamp ) ) {
        LOG( m_logger ) << "Last snapshot creation time: " << this->last_snapshot_creation_time;

        if ( m_snapshotHashComputing != nullptr && m_snapshotHashComputing->joinable() )
            m_snapshotHashComputing->join();

        // TODO Make this number configurable
        // thread can be absent - if hash was already there
        // snapshot can be absent too
        // but hash cannot be absent
        auto latest_snapshots = this->m_snapshotManager->getLatestSnapshots();
        if ( latest_snapshots.second ) {
            assert( m_snapshotManager->isSnapshotHashPresent( latest_snapshots.second ) );
            this->last_snapshoted_block_with_hash = latest_snapshots.second;
            m_snapshotManager->leaveNLastSnapshots( 2 );
        }
    }
}

void SnapshotAgent::doSnapshotIfNeeded( unsigned _currentBlockNumber, int64_t _timestamp ) {
    if ( m_snapshotIntervalSec <= 0 )
        return;

    LOG( m_loggerDetail ) << "Block timestamp: " << _timestamp;

    if ( this->isTimeToDoSnapshot( _timestamp ) ) {
        try {
            boost::chrono::high_resolution_clock::time_point t1;
            boost::chrono::high_resolution_clock::time_point t2;
            LOG( m_logger ) << "DOING SNAPSHOT: " << _currentBlockNumber;
            m_debugTracer.tracepoint( "doing_snapshot" );

            t1 = boost::chrono::high_resolution_clock::now();
            m_snapshotManager->doSnapshot( _currentBlockNumber );
            t2 = boost::chrono::high_resolution_clock::now();
            this->snapshot_calculation_time_ms =
                boost::chrono::duration_cast< boost::chrono::milliseconds >( t2 - t1 ).count();
        } catch ( SnapshotManager::SnapshotPresent& ex ) {
            cerror << "WARNING " << dev::nested_exception_what( ex );
        }

        this->last_snapshot_creation_time = _timestamp;

        LOG( m_logger ) << "New snapshot creation time: " << this->last_snapshot_creation_time;
    }

    // snapshots without hash can appear either from start, from downloading or from just
    // creation
    auto latest_snapshots = this->m_snapshotManager->getLatestSnapshots();

    // start if thread is free and there is work
    if ( ( m_snapshotHashComputing == nullptr || !m_snapshotHashComputing->joinable() ) &&
         latest_snapshots.second &&
         !m_snapshotManager->isSnapshotHashPresent( latest_snapshots.second ) ) {
        startHashComputingThread();

    }  // if thread
}

boost::filesystem::path SnapshotAgent::createSnapshotFile( unsigned _blockNumber ) {
    if ( _blockNumber > this->getLatestSnapshotBlockNumer() && _blockNumber != 0 )
        throw std::invalid_argument( "Too new snapshot requested" );
    boost::filesystem::path path = m_snapshotManager->makeOrGetDiff( _blockNumber );
    // TODO Make constant 2 configurable
    m_snapshotManager->leaveNLastDiffs( 2 );
    return path;
}

void SnapshotAgent::terminate() {
    if ( m_snapshotHashComputing != nullptr ) {
        try {
            if ( m_snapshotHashComputing->joinable() )
                m_snapshotHashComputing->detach();
        } catch ( ... ) {
        }
    }
}

dev::h256 SnapshotAgent::getSnapshotHash( unsigned _blockNumber, bool _forArchiveNode ) const {
    if ( _blockNumber > this->last_snapshoted_block_with_hash && _blockNumber != 0 )
        return dev::h256();

    try {
        dev::h256 res = this->m_snapshotManager->getSnapshotHash( _blockNumber, _forArchiveNode );
        return res;
    } catch ( const SnapshotManager::SnapshotAbsent& ) {
        return dev::h256();
    }

    // fall through other exceptions
}

uint64_t SnapshotAgent::getBlockTimestampFromSnapshot( unsigned _blockNumber ) const {
    return this->m_snapshotManager->getBlockTimestamp( _blockNumber );
}

bool SnapshotAgent::isTimeToDoSnapshot( uint64_t _timestamp ) const {
    return _timestamp / uint64_t( m_snapshotIntervalSec ) >
           this->last_snapshot_creation_time / uint64_t( m_snapshotIntervalSec );
}

void SnapshotAgent::startHashComputingThread() {
    auto latest_snapshots = this->m_snapshotManager->getLatestSnapshots();

    m_snapshotHashComputing.reset( new std::thread( [this, latest_snapshots]() {
        m_debugTracer.tracepoint( "computeSnapshotHash_start" );
        try {
            boost::chrono::high_resolution_clock::time_point t1;
            boost::chrono::high_resolution_clock::time_point t2;

            t1 = boost::chrono::high_resolution_clock::now();
            this->m_snapshotManager->computeSnapshotHash( latest_snapshots.second );
            t2 = boost::chrono::high_resolution_clock::now();
            this->snapshot_hash_calculation_time_ms =
                boost::chrono::duration_cast< boost::chrono::milliseconds >( t2 - t1 ).count();
            LOG( m_logger ) << "Computed hash for snapshot " << latest_snapshots.second << ": "
                            << m_snapshotManager->getSnapshotHash( latest_snapshots.second );
            m_debugTracer.tracepoint( "computeSnapshotHash_end" );

        } catch ( const std::exception& ex ) {
            cerror << cc::fatal( "CRITICAL" ) << " " << cc::warn( dev::nested_exception_what( ex ) )
                   << cc::error( " in computeSnapshotHash(). Exiting..." );
            cerror << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
            ExitHandler::exitHandler( -1, ExitHandler::ec_compute_snapshot_error );
        } catch ( ... ) {
            cerror << cc::fatal( "CRITICAL" )
                   << cc::error(
                          " unknown exception in computeSnapshotHash(). "
                          "Exiting..." );
            cerror << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
            ExitHandler::exitHandler( -1, ExitHandler::ec_compute_snapshot_error );
        }
    } ) );
}

void SnapshotAgent::doSnapshotAndComputeHash( unsigned _blockNumber ) {
    LOG( m_logger ) << "DOING SNAPSHOT: " << _blockNumber;
    m_debugTracer.tracepoint( "doing_snapshot" );

    try {
        m_snapshotManager->doSnapshot( _blockNumber );
    } catch ( SnapshotManager::SnapshotPresent& ex ) {
        LOG( m_logger ) << "0 block snapshot is already present. Skipping.";
        return;
    }

    m_snapshotManager->computeSnapshotHash( _blockNumber );
    LOG( m_logger ) << "Computed hash for snapshot " << _blockNumber << ": "
                    << m_snapshotManager->getSnapshotHash( _blockNumber );
    m_debugTracer.tracepoint( "computeSnapshotHash_end" );
}
