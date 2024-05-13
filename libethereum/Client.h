/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Client.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include <time.h>

#include <boost/filesystem/path.hpp>
#include <libconsensus/thirdparty/lru_ordered_memory_constrained_cache.hpp>

#include <libdevcore/Common.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/Guards.h>
#include <libdevcore/Worker.h>
#include <libethcore/SealEngine.h>
#include <libskale/State.h>

#include "Block.h"
#include "BlockChain.h"
#include "ClientBase.h"
#include "CommonNet.h"
#include "InstanceMonitor.h"
#include "SkaleHost.h"
#include "SnapshotAgent.h"
#include "StateImporter.h"
#include "ThreadSafeQueue.h"

#include <libhistoric/AlethStandardTrace.h>
#include <skutils/atomic_shared_ptr.h>
#include <skutils/multithreading.h>

class ConsensusHost;
class SnapshotManager;

namespace dev {
namespace eth {
class Client;
class DownloadMan;

enum ClientWorkState { Active = 0, Deleting, Deleted };

struct ActivityReport {
    unsigned ticks = 0;
    std::chrono::system_clock::time_point since = std::chrono::system_clock::now();
};

std::ostream& operator<<( std::ostream& _out, ActivityReport const& _r );


#ifdef HISTORIC_STATE
constexpr size_t MAX_BLOCK_TRACES_CACHE_SIZE = 64 * 1024 * 1024;
constexpr size_t MAX_BLOCK_TRACES_CACHE_ITEMS = 1024 * 1024;
#endif

/**
 * @brief Main API hub for interfacing with Ethereum.
 */
class Client : public ClientBase, protected Worker {
    friend class ::SkaleHost;

public:
    Client( ChainParams const& _params, int _networkID, std::shared_ptr< GasPricer > _gpForAdoption,
        std::shared_ptr< SnapshotManager > _snapshotManager,
        std::shared_ptr< InstanceMonitor > _instanceMonitor,
        boost::filesystem::path const& _dbPath = boost::filesystem::path(),
        WithExisting _forceAction = WithExisting::Trust,
        TransactionQueue::Limits const& _l = TransactionQueue::Limits{
            1024, 1024, 12322916, 24645833 } );
    /// Destructor.
    virtual ~Client();

    void stopWorking();

    void injectSkaleHost( std::shared_ptr< SkaleHost > _skaleHost = nullptr );

    /// Get information on this chain.
    ChainParams const& chainParams() const { return bc().chainParams(); }

    clock_t dbRotationPeriod() const { return bc().clockDbRotationPeriod_; }
    void dbRotationPeriod( clock_t clockPeriod ) { bc().clockDbRotationPeriod_ = clockPeriod; }

    /// Resets the gas pricer to some other object.
    void setGasPricer( std::shared_ptr< GasPricer > _gp ) { m_gp = _gp; }
    std::shared_ptr< GasPricer > gasPricer() const { return m_gp; }

    /// Submits the given transaction.
    /// @returns the new transaction's hash.
    h256 submitTransaction( TransactionSkeleton const& _t, Secret const& _secret ) override;

    /// Imports the given transaction into the transaction queue
    h256 importTransaction( Transaction const& _t ) override;

    /// Makes the given call. Nothing is recorded into the state.
    ExecutionResult call( Address const& _secret, u256 _value, Address _dest, bytes const& _data,
        u256 _gas, u256 _gasPrice,
#ifdef HISTORIC_STATE
        BlockNumber _blockNumber,
#endif
        FudgeFactor _ff = FudgeFactor::Strict ) override;

#ifdef HISTORIC_STATE
    Json::Value traceCall( Address const& _from, u256 _value, Address _to, bytes const& _data,
        u256 _gas, u256 _gasPrice, BlockNumber _blockNumber, Json::Value const& _jsonTraceConfig );
    Json::Value traceBlock( BlockNumber _blockNumber, Json::Value const& _jsonTraceConfig );
    Transaction createTransactionForCallOrTraceCall( const Address& _from, const u256& _value,
        const Address& _to, const bytes& _data, const u256& _gasLimit, const u256& _gasPrice,
        const u256& nonce ) const;
#endif


    /// Blocks until all pending transactions have been processed.
    void flushTransactions() override;

    using ClientBase::blockDetails;
    using ClientBase::blockInfo;  // for another overload
    using ClientBase::uncleHashes;

    /// Retrieve pending transactions
    Transactions pending() const override;

    Transactions debugGetFutureTransactions() const { return m_tq.debugGetFutureTransactions(); }

    /// Queues a block for import.
    ImportResult queueBlock( bytes const& _block, bool _isSafe = false );

    /// Get the remaining gas limit in this block.
    u256 gasLimitRemaining() const override { return m_postSeal.gasLimitRemaining(); }
    /// Get the gas bid price
    u256 gasBidPrice( unsigned _blockNumber = dev::eth::LatestBlock ) const override {
        return m_gp->bid( _blockNumber );
    }

    // [PRIVATE API - only relevant for base clients, not available in general]
    /// Get the block.
    dev::eth::Block block( h256 const& _blockHash, PopulationStatistics* o_stats ) const;

    /// Get the object representing the current state of Ethereum.
    dev::eth::Block postState() const {
        ReadGuard l( x_postSeal );
        return m_postSeal;
    }
    /// Get the object representing the current canonical blockchain.
    BlockChain const& blockChain() const { return bc(); }
    /// Get some information on the block queue.
    BlockQueueStatus blockQueueStatus() const { return m_bq.status(); }
    /// Get some information on the block syncing.
    SyncStatus syncStatus() const override;
    /// Populate the uninitialized fields in the supplied transaction with default values
    TransactionSkeleton populateTransactionWithDefaults(
        TransactionSkeleton const& _t ) const override;
    /// Get the block queue.
    BlockQueue const& blockQueue() const { return m_bq; }
    /// Get the state database.
    skale::State const& state() const { return m_state; }
    /// Get some information on the transaction queue.
    TransactionQueue::Status transactionQueueStatus() const { return m_tq.status(); }
    TransactionQueue::Limits transactionQueueLimits() const { return m_tq.limits(); }
    TransactionQueue* debugGetTransactionQueue() { return &m_tq; }

    /// Freeze worker thread and sync some of the block queue.
    std::tuple< ImportRoute, bool, unsigned > syncQueue( unsigned _max = 1 );

    // Sealing stuff:
    // Note: "mining"/"miner" is deprecated. Use "sealing"/"sealer".

    Address author() const override {
        ReadGuard l( x_preSeal );
        return m_preSeal.author();
    }
    void setAuthor( Address const& _us ) override {
        DEV_WRITE_GUARDED( x_preSeal )
        m_preSeal.setAuthor( _us );
        restartMining();
    }

    /// Type of sealers available for this seal engine.
    strings sealers() const { return sealEngine()->sealers(); }
    /// Current sealer in use.
    std::string sealer() const { return sealEngine()->sealer(); }
    /// Change sealer.
    void setSealer( std::string const& _id ) {
        sealEngine()->setSealer( _id );
        if ( wouldSeal() )
            startSealing();
    }
    /// Review option for the sealer.
    bytes sealOption( std::string const& _name ) const { return sealEngine()->option( _name ); }
    /// Set option for the sealer.
    bool setSealOption( std::string const& _name, bytes const& _value ) {
        auto ret = sealEngine()->setOption( _name, _value );
        if ( wouldSeal() )
            startSealing();
        return ret;
    }

    /// Start sealing.
    void startSealing() override;
    /// Stop sealing.
    void stopSealing() override { m_wouldSeal = false; }
    /// Are we sealing now?
    bool wouldSeal() const override { return m_wouldSeal; }

    /// Are we updating the chain (syncing or importing a new block)?
    bool isSyncing() const override;
    /// Are we syncing the chain?
    bool isMajorSyncing() const override;

    /// Gets the network id.
    u256 networkId() const override;
    /// Sets the network id.
    void setNetworkId( u256 const& _n ) override;

    /// Get the seal engine.
    SealEngineFace* sealEngine() const override { return bc().sealEngine(); }

    // Debug stuff:

    DownloadMan const* downloadMan() const;
    /// Clears pending transactions. Just for debug use.
    void clearPending();
    /// Retries all blocks with unknown parents.
    void retryUnknown() { m_bq.retryAllUnknown(); }
    /// Get a report of activity.
    ActivityReport activityReport() {
        ActivityReport ret;
        std::swap( m_report, ret );
        return ret;
    }
    /// Set the extra data that goes into sealed blocks.
    void setExtraData( bytes const& _extraData ) { m_extraData = _extraData; }
    /// Rescue the chain.
    void rescue() { bc().rescue( m_state ); }

    std::unique_ptr< StateImporterFace > createStateImporter() {
        throw std::logic_error( "createStateImporter is not implemented" );
        //        return dev::eth::createStateImporter(m_state);
    }

    /// Queues a function to be executed in the main thread (that owns the blockchain, etc).
    void executeInMainThread( std::function< void() > const& _function );

    /// should be called after the constructor of the most derived class finishes.
    void startWorking() {
        assert( m_skaleHost );
        Worker::startWorking();  // these two lines are dependent!!
        m_skaleHost->startWorking();
    };

    /// Change the function that is called when a new block is imported
    Handler< BlockHeader const& > setOnBlockImport(
        std::function< void( BlockHeader const& ) > _handler ) {
        return m_onBlockImport.add( _handler );
    }
    /// Change the function that is called when a new block is sealed
    Handler< bytes const& > setOnBlockSealed( std::function< void( bytes const& ) > _handler ) {
        return m_onBlockSealed.add( _handler );
    }

    std::shared_ptr< SkaleHost > skaleHost() const { return m_skaleHost; }

    // main entry point after consensus
    size_t importTransactionsAsBlock( const Transactions& _transactions, u256 _gasPrice,
        uint64_t _timestamp = ( uint64_t ) utcTime() );

    boost::filesystem::path createSnapshotFile( unsigned _blockNumber ) {
        return m_snapshotAgent->createSnapshotFile( _blockNumber );
    }

    // set exiting time for node rotation
    void setSchainExitTime( uint64_t _timestamp ) const;

    dev::h256 getSnapshotHash( unsigned _blockNumber ) const {
        return m_snapshotAgent->getSnapshotHash( _blockNumber );
    }

    uint64_t getBlockTimestampFromSnapshot( unsigned _blockNumber ) const {
        return m_snapshotAgent->getBlockTimestampFromSnapshot( _blockNumber );
    }

    int64_t getLatestSnapshotBlockNumer() const {
        return m_snapshotAgent->getLatestSnapshotBlockNumer();
    }

    uint64_t getSnapshotCalculationTime() const {
        return m_snapshotAgent->getSnapshotCalculationTime();
    }

    uint64_t getSnapshotHashCalculationTime() const {
        return m_snapshotAgent->getSnapshotHashCalculationTime();
    }

    std::array< std::string, 4 > getIMABLSPublicKey() const {
        return chainParams().sChain.nodeGroups.at( historicGroupIndex ).blsPublicKey;
    }

    // get node id for historic node in chain
    std::string getHistoricNodeId( unsigned _id ) const {
        return chainParams().sChain.nodeGroups.at( historicGroupIndex ).nodes.at( _id ).id.str();
    }

    // get schain index for historic node in chain
    std::string getHistoricNodeIndex( unsigned _idx ) const {
        return chainParams()
            .sChain.nodeGroups.at( historicGroupIndex )
            .nodes.at( _idx )
            .schainIndex.str();
    }

    // get node owner for historic node in chain
    std::string getHistoricNodePublicKey( unsigned _idx ) const {
        return chainParams().sChain.nodeGroups.at( historicGroupIndex ).nodes.at( _idx ).publicKey;
    }

    void doStateDbCompaction() const { m_state.getOriginalDb()->doCompaction(); }

    void doBlocksDbCompaction() const { m_bc.doLevelDbCompaction(); }

    std::pair< uint64_t, uint64_t > getBlocksDbUsage() const;

    std::pair< uint64_t, uint64_t > getStateDbUsage() const;

#ifdef HISTORIC_STATE
    uint64_t getHistoricStateDbUsage() const;
    uint64_t getHistoricRootsDbUsage() const;
#endif  // HISTORIC_STATE

    uint64_t submitOracleRequest( const string& _spec, string& _receipt, string& _errorMessage );
    uint64_t checkOracleResult( const string& _receipt, string& _result );

    SkaleDebugInterface::handler getDebugHandler() const { return m_debugHandler; }

#ifdef HISTORIC_STATE
    OverlayDB const& historicStateDB() const { return m_historicStateDB; }
    OverlayDB const& historicBlockToStateRootDB() const { return m_historicBlockToStateRootDB; }
#endif

protected:
    /// As syncTransactionQueue - but get list of transactions explicitly
    /// returns number of successfullty executed transactions
    /// thread unsafe!!
    size_t syncTransactions( const Transactions& _transactions, u256 _gasPrice,
        uint64_t _timestamp = ( uint64_t ) utcTime(),
        Transactions* vecMissing = nullptr  // it's non-null only for PARTIAL CATCHUP
    );

    /// As rejigSealing - but stub
    /// thread unsafe!!
    void sealUnconditionally( bool submitToBlockChain = true );

    /// thread unsafe!!
    void importWorkingBlock();

    /// Perform critical setup functions.
    /// Must be called in the constructor of the finally derived class.
    void init( WithExisting _forceAction, u256 _networkId );

    /// InterfaceStub methods
    BlockChain& bc() override { return m_bc; }
    BlockChain const& bc() const override { return m_bc; }

    /// Returns the state object for the full block (i.e. the terminal state) for index _h.
    /// Works properly with LatestBlock and PendingBlock.
    Block preSeal() const override {
        ReadGuard l( x_preSeal );
        return m_preSeal;
    }
    Block postSeal() const override {
        ReadGuard l( x_postSeal );
        return m_postSeal;
    }
    void prepareForTransaction() override;

    /// Collate the changed filters for the bloom filter of the given pending transaction.
    /// Insert any filters that are activated into @a o_changed.
    void appendFromNewPending(
        TransactionReceipt const& _receipt, h256Hash& io_changed, h256 _sha3 );

    /// Collate the changed filters for the hash of the given block.
    /// Insert any filters that are activated into @a o_changed.
    void appendFromBlock( h256 const& _blockHash, BlockPolarity _polarity, h256Hash& io_changed );

    /// Record that the set of filters @a _filters have changed.
    /// This doesn't actually make any callbacks, but increments some counters in m_watches.
    void noteChanged( h256Hash const& _filters );

    /// Submit
    virtual bool submitSealed( bytes const& _s );


#ifdef HISTORIC_STATE
    Block blockByNumber( BlockNumber _h ) const;
#endif

protected:
    /// Called when Worker is starting.
    void startedWorking() override;

    /// Do some work. Handles blockchain maintenance and sealing.
    void doWork( bool _doWait );
    void doWork() override { doWork( true ); }

    /// Called when Worker is exiting.
    void doneWorking() override;

    /// Called when wouldSeal(), pendingTransactions() have changed.
    void rejigSealing();

    /// Called on chain changes
    void onDeadBlocks( h256s const& _blocks, h256Hash& io_changed );

    /// Called on chain changes
    virtual void onNewBlocks( h256s const& _blocks, h256Hash& io_changed );

    /// Called after processing blocks by onChainChanged(_ir)
    void resyncStateFromChain();
    /// Update m_preSeal, m_working, m_postSeal blocks from the latest state of the chain
    void restartMining();

    /// Clear working state of transactions
    void resetState();

    /// Magically called when the chain has changed. An import route is provided.
    /// Called by either submitWork() or in our main thread through syncBlockQueue().
    void onChainChanged( ImportRoute const& _ir );

    /// Signal handler for when the block queue needs processing.
    void syncBlockQueue();

    /// Magically called when m_tq needs syncing. Be nice and don't block.
    void onTransactionQueueReady() {
        m_syncTransactionQueue = true;
        m_signalled.notify_all();
    }

    /// Magically called when m_bq needs syncing. Be nice and don't block.
    void onBlockQueueReady() {
        m_syncBlockQueue = true;
        m_signalled.notify_all();
    }

    /// Called when the post state has changed (i.e. when more transactions are in it or we're
    /// sealing on a new block). This updates m_sealingInfo.
    void onPostStateChanged();

    /// Does garbage collection on watches.
    void checkWatchGarbage();

    /// Ticks various system-level objects.
    void tick();

    /// Called when we have attempted to import a bad block.
    /// @warning May be called from any thread.
    void onBadBlock( Exception& _ex ) const;

    /// Executes the pending functions in m_functionQueue
    void callQueuedFunctions();


    BlockChain m_bc;  ///< Maintains block database and owns the seal engine.
    BlockQueue m_bq;  ///< Maintains a list of incoming blocks not yet on the blockchain (to be
                      ///< imported).
    TransactionQueue m_tq;  ///< Maintains a list of incoming transactions not yet in a block on the
                            ///< blockchain.


#ifdef HISTORIC_STATE
    OverlayDB m_historicStateDB;  ///< Acts as the central point for the state database, so multiple
                                  ///< States can share it.
    OverlayDB m_historicBlockToStateRootDB;  /// Maps hashes of block IDs to state roots
#endif

    std::shared_ptr< GasPricer > m_gp;  ///< The gas pricer.

    skale::State m_state;            ///< Acts as the central point for the state.
    mutable SharedMutex x_preSeal;   ///< Lock on m_preSeal.
    Block m_preSeal;                 ///< The present state of the client.
    mutable SharedMutex x_postSeal;  ///< Lock on m_postSeal.
    Block m_postSeal;  ///< The state of the client which we're sealing (i.e. it'll have all the
                       ///< rewards added).
    mutable SharedMutex x_working;  ///< Lock on m_working.
    Block m_working;  ///< The state of the client which we're sealing (i.e. it'll have all the
                      ///< rewards added), while we're actually working on it.
    BlockHeader m_sealingInfo;  ///< The header we're attempting to seal on (derived from
                                ///< m_postSeal).

    mutable Mutex m_blockImportMutex;  /// synchronize state and latest block update

    bool remoteActive() const;     ///< Is there an active and valid remote worker?
    bool m_remoteWorking = false;  ///< Has the remote worker recently been reset?
    std::atomic< bool > m_needStateReset = { false };     ///< Need reset working state to premin on
                                                          ///< next sync
    std::chrono::system_clock::time_point m_lastGetWork;  ///< Is there an active and valid remote
                                                          ///< worker?

    dev::u256 m_networkId;  // TODO delegate this to someone? (like m_host)
    // TODO Add here m_isSyncing or use special BlockchainSync class (ask Stan)

    std::condition_variable m_signalled;
    Mutex x_signalled;

    Handler<> m_tqReady;
    Handler< h256 const& > m_tqReplaced;
    Handler<> m_bqReady;

    bool m_wouldSeal = false;          ///< True if we /should/ be sealing.
    bool m_wouldButShouldnot = false;  ///< True if the last time we called rejigSealing wouldSeal()
                                       ///< was true but sealer's shouldSeal() was false.

    mutable std::chrono::system_clock::time_point m_lastGarbageCollection;
    ///< When did we last both doing GC on the watches?
    mutable std::chrono::system_clock::time_point m_lastTick = std::chrono::system_clock::now();
    ///< When did we last tick()?

    unsigned m_syncAmount = 50;  ///< Number of blocks to sync in each go.

    ActivityReport m_report;

    SharedMutex x_functionQueue;
    std::queue< std::function< void() > > m_functionQueue;  ///< Functions waiting to be executed in
                                                            ///< the main thread.

    std::atomic< bool > m_syncTransactionQueue = { false };
    std::atomic< bool > m_syncBlockQueue = { false };

    bytes m_extraData;

    Signal< BlockHeader const& > m_onBlockImport;  ///< Called if we have imported a new block into
                                                   ///< the DB
    Signal< bytes const& > m_onBlockSealed;        ///< Called if we have sealed a new block

    Logger m_logger{ createLogger( VerbosityInfo, "client" ) };
    Logger m_loggerDetail{ createLogger( VerbosityTrace, "client" ) };

    SkaleDebugTracer m_debugTracer;
    SkaleDebugInterface::handler m_debugHandler;

    /// skale
    std::shared_ptr< SkaleHost > m_skaleHost;
    std::shared_ptr< SnapshotAgent > m_snapshotAgent;
    bool m_snapshotAgentInited = false;
    const static dev::h256 empty_str_hash;
    std::shared_ptr< InstanceMonitor > m_instanceMonitor;
    fs::path m_dbPath;
#ifdef HISTORIC_STATE
    cache::lru_ordered_memory_constrained_cache< std::string, Json::Value > m_blockTraceCache;
#endif

private:
    void initHistoricGroupIndex();
    void updateHistoricGroupIndex();

    // which group corresponds to the current block timestamp on this node
    unsigned historicGroupIndex = 0;

public:
    FILE* performance_fd;

protected:
    // generic watch
    template < typename parameter_type, typename key_type = unsigned >
    class genericWatch {
    public:
        typedef std::function< void( const unsigned&, const parameter_type& ) > handler_type;

    private:
        typedef skutils::multithreading::recursive_mutex_type mutex_type;
        typedef std::lock_guard< mutex_type > lock_type;
        static mutex_type& mtx() { return skutils::get_ref_mtx(); }

        typedef std::map< key_type, handler_type > map_type;
        map_type map_;

        std::atomic< key_type > subscription_counter_ = 0;

    public:
        genericWatch() {}
        virtual ~genericWatch() { uninstallAll(); }
        key_type create_subscription_id() { return subscription_counter_++; }
        virtual bool is_installed( const key_type& k ) const {
            lock_type lock( mtx() );
            if ( map_.find( k ) == map_.end() )
                return false;
            return true;
        }
        virtual key_type install( handler_type& h ) {
            if ( !h )
                return false;  // not call-able
            lock_type lock( mtx() );
            key_type k = create_subscription_id();
            map_[k] = h;
            return k;
        }
        virtual bool uninstall( const key_type& k ) {
            lock_type lock( mtx() );
            auto itFind = map_.find( k );
            if ( itFind == map_.end() )
                return false;
            map_.erase( itFind );
            return true;
        }
        virtual void uninstallAll() {
            lock_type lock( mtx() );
            map_.clear();
        }
        virtual void invoke( const parameter_type& p ) {
            map_type map2;
            {  // block
                lock_type lock( mtx() );
                map2 = map_;
            }  // block
            auto itWalk = map2.begin();
            for ( ; itWalk != map2.end(); ++itWalk ) {
                try {
                    itWalk->second( itWalk->first, p );
                } catch ( ... ) {
                }
            }
        }
    };
    // new block watch
    typedef genericWatch< Block > blockWatch;
    blockWatch m_new_block_watch;
    // new pending transation watch
    typedef genericWatch< Transaction > transactionWatch;
    transactionWatch m_new_pending_transaction_watch;

public:
    // new block watch
    virtual unsigned installNewBlockWatch(
        std::function< void( const unsigned&, const Block& ) >& ) override;
    virtual bool uninstallNewBlockWatch( const unsigned& ) override;

    // new pending transation watch
    virtual unsigned installNewPendingTransactionWatch(
        std::function< void( const unsigned&, const Transaction& ) >& ) override;
    virtual bool uninstallNewPendingTransactionWatch( const unsigned& ) override;


#ifdef HISTORIC_STATE
    u256 historicStateBalanceAt( Address _a, BlockNumber _block ) const override;
    u256 historicStateCountAt( Address _a, BlockNumber _block ) const override;
    u256 historicStateAt( Address _a, u256 _l, BlockNumber _block ) const override;
    h256 historicStateRootAt( Address _a, BlockNumber _block ) const override;
    bytes historicStateCodeAt( Address _a, BlockNumber _block ) const override;
#endif
    void initStateFromDiskOrGenesis();
    void populateNewChainStateFromGenesis();
};

}  // namespace eth
}  // namespace dev
