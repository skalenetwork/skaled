#ifndef SNAPSHOTAGENT_H
#define SNAPSHOTAGENT_H

#include <libdevcore/Log.h>
#include <libskale/SkaleDebug.h>

#include <boost/filesystem.hpp>

#include <memory>
#include <thread>

class SnapshotManager;

// Knows all about snapshots and maintains all dynamic behavior related to them:
//  - keeping time of snapshot creation
//  - hash computation
//  - serialization
class SnapshotAgent {
public:
    SnapshotAgent( int64_t _snapshotIntervalSec,
        std::shared_ptr< SnapshotManager > _snapshotManager, SkaleDebugTracer& _debugTracer );

    // timestamp of 1st block is the only robust time source
    void init( unsigned _currentBlockNumber, int64_t _timestampOfBlock1 );

    void finishHashComputingAndUpdateHashesIfNeeded( int64_t _timestamp );
    void doSnapshotIfNeeded( unsigned _currentBlockNumber, int64_t _timestamp );

    boost::filesystem::path createSnapshotFile( unsigned _blockNumber );

    void terminate();

    dev::h256 getSnapshotHash( unsigned _blockNumber, bool _forArchiveNode ) const;
    uint64_t getBlockTimestampFromSnapshot( unsigned _blockNumber ) const;
    int64_t getLatestSnapshotBlockNumer() const { return this->last_snapshoted_block_with_hash; }
    uint64_t getSnapshotCalculationTime() const { return this->snapshot_calculation_time_ms; }
    uint64_t getSnapshotHashCalculationTime() const {
        return this->snapshot_hash_calculation_time_ms;
    }

private:
    // time of last physical snapshot
    int64_t last_snapshot_creation_time = 0;
    // usually this is snapshot before last!
    int64_t last_snapshoted_block_with_hash = -1;

    int64_t m_snapshotIntervalSec;
    std::shared_ptr< SnapshotManager > m_snapshotManager;

    inline bool isTimeToDoSnapshot( uint64_t _timestamp ) const;
    void doSnapshotAndComputeHash( unsigned _blockNumber );
    void startHashComputingThread();

    std::unique_ptr< std::thread > m_snapshotHashComputing;

    uint64_t snapshot_calculation_time_ms;
    uint64_t snapshot_hash_calculation_time_ms;

    dev::Logger m_logger{ createLogger( dev::VerbosityInfo, "SnapshotAgent" ) };
    dev::Logger m_loggerDetail{ createLogger( dev::VerbosityTrace, "SnapshotAgent" ) };

    SkaleDebugTracer& m_debugTracer;
};

#endif  // SNAPSHOTAGENT_H
