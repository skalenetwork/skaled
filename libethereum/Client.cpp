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
/** @file Client.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Client.h"
#include "Block.h"
#include "Defaults.h"
#include "Executive.h"
#include "SkaleHost.h"
#include "SnapshotStorage.h"
#include "TransactionQueue.h"
#include <libdevcore/Log.h>
#include <boost/filesystem.hpp>

#include <libskale/AmsterdamFixPatch.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>

#include <libdevcore/microprofile.h>

#include <libdevcore/FileSystem.h>
#include <libdevcore/system_usage.h>
#ifndef NO_ALETH_STATE
#include <libethereum/State.h>
#endif
#include <libskale/ContractStorageLimitPatch.h>
#include <libskale/State.h>
#include <libskale/TotalStorageUsedPatch.h>
#include <libskale/UnsafeRegion.h>
#include <skutils/console_colors.h>
#include <json.hpp>

using namespace std;
using namespace dev;
using namespace dev::eth;
namespace fs = boost::filesystem;
using skale::BaseState;
using skale::Permanence;
using skale::State;
using namespace skale::error;

static_assert( BOOST_VERSION >= 106400, "Wrong boost headers version" );

namespace {
std::string filtersToString( h256Hash const& _fs ) {
    std::stringstream str;
    str << "{";
    unsigned i = 0;
    for ( h256 const& f : _fs ) {
        str << ( i++ ? ", " : "" );
        if ( f == PendingChangedFilter )
            str << "pending";
        else if ( f == ChainChangedFilter )
            str << "chain";
        else
            str << f;
    }
    str << "}";
    return str.str();
}

#if ( defined __HAVE_SKALED_LOCK_FILE_INDICATING_CRITICAL_STOP__ )
void create_lock_file_or_fail( const fs::path& dir ) {
    fs::path p = dir / "skaled.lock";
    if ( fs::exists( p ) )
        throw runtime_error( string( "Data dir unclean! Remove " ) + p.string() +
                             " and continue at your own risk!" );
    FILE* fp = fopen( p.c_str(), "w" );
    if ( !fp )
        throw runtime_error(
            string( "Cannot create lock file " ) + p.string() + ": " + strerror( errno ) );
    fclose( fp );
}
void delete_lock_file( const fs::path& dir ) {
    fs::path p = dir / "skaled.lock";
    if ( fs::exists( p ) )
        fs::remove( p );
}
#endif  /// (defined __HAVE_SKALED_LOCK_FILE_INDICATING_CRITICAL_STOP__)

}  // namespace

std::ostream& dev::eth::operator<<( std::ostream& _out, ActivityReport const& _r ) {
    _out << "Since " << toString( _r.since ) << " ("
         << std::chrono::duration_cast< std::chrono::seconds >(
                std::chrono::system_clock::now() - _r.since )
                .count();
    _out << "): " << _r.ticks << "ticks";
    return _out;
}

Client::Client( ChainParams const& _params, int _networkID,
    std::shared_ptr< GasPricer > _gpForAdoption,
    std::shared_ptr< SnapshotManager > _snapshotManager,
    std::shared_ptr< InstanceMonitor > _instanceMonitor, fs::path const& _dbPath,
    WithExisting _forceAction, TransactionQueue::Limits const& _l )
    : Worker( "Client", 0 ),
      m_bc( _params, _dbPath, true, _forceAction ),
      m_tq( _l ),
      m_gp( _gpForAdoption ? _gpForAdoption : make_shared< TrivialGasPricer >() ),
      m_preSeal( chainParams().accountStartNonce ),
      m_postSeal( chainParams().accountStartNonce ),
      m_working( chainParams().accountStartNonce ),
      m_snapshotManager( _snapshotManager ),
      m_instanceMonitor( _instanceMonitor ),
      m_dbPath( _dbPath ) {
#if ( defined __HAVE_SKALED_LOCK_FILE_INDICATING_CRITICAL_STOP__ )
    create_lock_file_or_fail( m_dbPath );
#endif  /// (defined __HAVE_SKALED_LOCK_FILE_INDICATING_CRITICAL_STOP__)

    m_debugTracer.call_on_tracepoint( [this]( const std::string& name ) {
        clog( VerbosityTrace, "client" )
            << "TRACEPOINT " << name << " " << m_debugTracer.get_tracepoint_count( name );
    } );


    m_debugHandler = [this]( const std::string& arg ) -> std::string {
        return DebugTracer_handler( arg, this->m_debugTracer );
    };

    init( _forceAction, _networkID );

    TotalStorageUsedPatch::g_client = this;
    ContractStorageLimitPatch::contractStoragePatchTimestamp =
        chainParams().sChain.contractStoragePatchTimestamp;
}

Client::~Client() {
    stopWorking();
}

void Client::stopWorking() {
    // TODO Try this in develop. For hotfix we will keep as is
    //    if ( !Worker::isWorking() )
    //        return;

    Worker::stopWorking();

    if ( m_skaleHost )
        m_skaleHost->stopWorking();  // TODO Find and document a systematic way to start/stop all
                                     // workers
    else
        cerror << "Instance of SkaleHost was not properly created.";

    if ( m_snapshotHashComputing != nullptr ) {
        try {
            if ( m_snapshotHashComputing->joinable() )
                m_snapshotHashComputing->join();
        } catch ( ... ) {
        }
    }

    m_new_block_watch.uninstallAll();
    m_new_pending_transaction_watch.uninstallAll();

    m_signalled.notify_all();  // to wake up the thread from Client::doWork()

    m_tq.HandleDestruction();  // l_sergiy: destroy transaction queue earlier
    m_bq.stop();               // l_sergiy: added to stop block queue processing

    m_bc.close();
    LOG( m_logger ) << cc::success( "Blockchain is closed" );

#if ( defined __HAVE_SKALED_LOCK_FILE_INDICATING_CRITICAL_STOP__ )
    bool isForcefulExit =
        ( !m_skaleHost || m_skaleHost->exitedForcefully() == false ) ? false : true;
    if ( !isForcefulExit ) {
        delete_lock_file( m_dbPath );
        LOG( m_logger ) << cc::success( "Deleted lock file " )
                        << cc::p( boost::filesystem::canonical( m_dbPath ).string() +
                                  std::string( "/skaled.lock" ) );
    } else {
        LOG( m_logger ) << cc::fatal( "ATTENTION:" ) << " " << cc::error( "Deleted lock file " )
                        << cc::p( boost::filesystem::canonical( m_dbPath ).string() +
                                  std::string( "/skaled.lock" ) )
                        << cc::error( " after forceful exit" );
    }
    LOG( m_logger ).flush();
#endif  /// (defined __HAVE_SKALED_LOCK_FILE_INDICATING_CRITICAL_STOP__)

    terminate();
}


void Client::injectSkaleHost( std::shared_ptr< SkaleHost > _skaleHost ) {
    assert( !m_skaleHost );

    m_skaleHost = _skaleHost;

    if ( !m_skaleHost )
        m_skaleHost = make_shared< SkaleHost >( *this );
    if ( Worker::isWorking() )
        m_skaleHost->startWorking();
}

void Client::init( WithExisting _forceAction, u256 _networkId ) {
    DEV_TIMED_FUNCTION_ABOVE( 500 );
    m_networkId = _networkId;

    // Cannot be opened until after blockchain is open, since BlockChain may upgrade the database.
    // TODO: consider returning the upgrade mechanism here. will delaying the opening of the
    // blockchain database until after the construction.
    m_state = skale::State( chainParams().accountStartNonce, m_dbPath, bc().genesisHash(),
        skale::BaseState::PreExisting, chainParams().accountInitialFunds,
        chainParams().sChain.contractStorageLimit );

    if ( m_state.empty() ) {
        m_state.startWrite().populateFrom( bc().chainParams().genesisState );
        m_state = m_state.startNew();
    };
    // LAZY. TODO: move genesis state construction/commiting to stateDB opening and have this
    // just take the root from the genesis block.


    m_preSeal = bc().genesisBlock( m_state );
    m_postSeal = m_preSeal;

    m_bq.setChain( bc() );

    m_lastGetWork = std::chrono::system_clock::now() - chrono::seconds( 30 );
    m_tqReady = m_tq.onReady( [=]() {
        this->onTransactionQueueReady();
    } );  // TODO: should read m_tq->onReady(thisThread, syncTransactionQueue);
    m_tqReplaced = m_tq.onReplaced( [=]( h256 const& ) { m_needStateReset = true; } );
    m_bqReady = m_bq.onReady( [=]() {
        this->onBlockQueueReady();
    } );  // TODO: should read m_bq->onReady(thisThread, syncBlockQueue);
    m_bq.setOnBad( [=]( Exception& ex ) { this->onBadBlock( ex ); } );
    bc().setOnBad( [=]( Exception& ex ) { this->onBadBlock( ex ); } );
    bc().setOnBlockImport( [=]( BlockHeader const& _info ) {
        if ( m_skaleHost )
            m_skaleHost->onBlockImported( _info );
        m_onBlockImport( _info );
    } );

    if ( _forceAction == WithExisting::Rescue )
        bc().rescue( m_state );

    m_gp->update( bc() );

    if ( m_dbPath.size() )
        Defaults::setDBPath( m_dbPath );

    if ( chainParams().sChain.snapshotIntervalSec > 0 ) {
        LOG( m_logger ) << "Snapshots enabled, snapshotIntervalSec is: "
                        << chainParams().sChain.snapshotIntervalSec;
        this->initHashes();
    }

    if ( ChainParams().sChain.nodeGroups.size() > 0 )
        initIMABLSPublicKey();

    // HACK Needed to set env var for consensus
    AmsterdamFixPatch::isEnabled( *this );

    doWork( false );
}

ImportResult Client::queueBlock( bytes const& _block, bool _isSafe ) {
    if ( m_bq.status().verified + m_bq.status().verifying + m_bq.status().unverified > 10000 ) {
        MICROPROFILE_SCOPEI( "Client", "queueBlock sleep 500", MP_DIMGRAY );
        this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
    }
    return m_bq.import( &_block, _isSafe );
}

tuple< ImportRoute, bool, unsigned > Client::syncQueue( unsigned _max ) {
    Worker::stopWorking();
    return bc().sync( m_bq, m_state, _max );
}

void Client::onBadBlock( Exception& _ex ) const {
    // BAD BLOCK!!!
    bytes const* block = boost::get_error_info< errinfo_block >( _ex );
    if ( !block ) {
        cwarn << "ODD: onBadBlock called but exception (" << _ex.what() << ") has no block in it.";
        cwarn << boost::diagnostic_information( _ex );
        return;
    }

    badBlock( *block, _ex.what() );
}

void Client::callQueuedFunctions() {
    while ( true ) {
        function< void() > f;
        DEV_WRITE_GUARDED( x_functionQueue )
        if ( !m_functionQueue.empty() ) {
            f = m_functionQueue.front();
            m_functionQueue.pop();
        }
        if ( f )
            f();
        else
            break;
    }
}

u256 Client::networkId() const {
    return m_networkId;
}

void Client::setNetworkId( u256 const& _n ) {
    m_networkId = _n;
    // TODO Notify host somehow
}

bool Client::isSyncing() const {
    return false;
    // TODO Ask here some special sync component
}

bool Client::isMajorSyncing() const {
    return false;
    // TODO Ask here some special sync component
}

// TODO make Client not-Worker, remove all this stuff! (doWork, etc..)
void Client::startedWorking() {
    // Synchronise the state according to the head of the block chain.
    // TODO: currently it contains keys for *all* blocks. Make it remove old ones.
    LOG( m_loggerDetail ) << cc::debug( "startedWorking()" );

    DEV_GUARDED( m_blockImportMutex ) {
        DEV_WRITE_GUARDED( x_preSeal )
        m_preSeal.sync( bc() );
        DEV_READ_GUARDED( x_preSeal ) {
            DEV_WRITE_GUARDED( x_working )
            m_working = m_preSeal;
            DEV_WRITE_GUARDED( x_postSeal )
            m_postSeal = m_preSeal;
        }
    }
}

void Client::doneWorking() {
    // Synchronise the state according to the head of the block chain.
    // TODO: currently it contains keys for *all* blocks. Make it remove old ones.

    DEV_GUARDED( m_blockImportMutex ) {
        DEV_WRITE_GUARDED( x_preSeal )
        m_preSeal.sync( bc() );
        DEV_READ_GUARDED( x_preSeal ) {
            DEV_WRITE_GUARDED( x_working )
            m_working = m_preSeal;
            DEV_WRITE_GUARDED( x_postSeal )
            m_postSeal = m_preSeal;
        }
    }
}

void Client::executeInMainThread( function< void() > const& _function ) {
    DEV_WRITE_GUARDED( x_functionQueue )
    m_functionQueue.push( _function );
    m_signalled.notify_all();
}

void Client::clearPending() {
    DEV_WRITE_GUARDED( x_postSeal ) {
        if ( !m_postSeal.pending().size() )
            return;
        m_tq.clear();
        DEV_READ_GUARDED( x_preSeal )
        m_postSeal = m_preSeal;
    }

    startSealing();
    h256Hash changeds;
    noteChanged( changeds );
}

void Client::appendFromNewPending(
    TransactionReceipt const& _receipt, h256Hash& io_changed, h256 _sha3 ) {
    Guard l( x_filtersWatches );
    io_changed.insert( PendingChangedFilter );
    m_specialFilters.at( PendingChangedFilter ).push_back( _sha3 );
    for ( pair< h256 const, InstalledFilter >& i : m_filters ) {
        // acceptable number.
        auto m = i.second.filter.matches( _receipt );
        if ( m.size() ) {
            // filter catches them
            for ( LogEntry const& l : m )
                i.second.changes_.push_back( LocalisedLogEntry( l ) );
            io_changed.insert( i.first );
        }
    }
}

void Client::appendFromBlock( h256 const& _block, BlockPolarity _polarity, h256Hash& io_changed ) {
    // TODO: more precise check on whether the txs match.
    auto receipts = bc().receipts( _block ).receipts;

    Guard l( x_filtersWatches );
    io_changed.insert( ChainChangedFilter );
    m_specialFilters.at( ChainChangedFilter ).push_back( _block );
    for ( pair< h256 const, InstalledFilter >& i : m_filters ) {
        // acceptable number & looks like block may contain a matching log entry.
        for ( size_t j = 0; j < receipts.size(); j++ ) {
            auto tr = receipts[j];
            auto m = i.second.filter.matches( tr );
            if ( m.size() ) {
                auto transactionHash = transaction( _block, j ).sha3();
                // filter catches them
                for ( LogEntry const& l : m )
                    i.second.changes_.push_back( LocalisedLogEntry( l, _block,
                        ( BlockNumber ) bc().number( _block ), transactionHash, j, 0, _polarity ) );
                io_changed.insert( i.first );
            }
        }
    }
}

unsigned static const c_syncMin = 1;
unsigned static const c_syncMax = 1000;
double static const c_targetDuration = 1;

void Client::syncBlockQueue() {
    //  cdebug << "syncBlockQueue()";

    ImportRoute ir;
    unsigned count;
    Timer t;
    tie( ir, m_syncBlockQueue, count ) = bc().sync( m_bq, m_state, m_syncAmount );
    double elapsed = t.elapsed();

    if ( count ) {
        LOG( m_logger ) << count << " blocks imported in " << unsigned( elapsed * 1000 ) << " ms ("
                        << ( count / elapsed ) << " blocks/s) in #" << bc().number();
    }

    if ( elapsed > c_targetDuration * 1.1 && count > c_syncMin )
        m_syncAmount = max( c_syncMin, count * 9 / 10 );
    else if ( count == m_syncAmount && elapsed < c_targetDuration * 0.9 &&
              m_syncAmount < c_syncMax )
        m_syncAmount = min( c_syncMax, m_syncAmount * 11 / 10 + 1 );
    if ( ir.liveBlocks.empty() )
        return;
    onChainChanged( ir );
}

static std::string stat_transactions2str(
    const Transactions& _transactions, const std::string& strPrefix ) {
    size_t cnt = _transactions.size();
    std::string s;
    if ( !strPrefix.empty() )
        s += strPrefix;
    s += cc::size10( cnt ) + " " +
         cc::debug(
             ( cnt > 1 ) ? "transactions: " : ( ( cnt == 1 ) ? "transaction: " : "transactions" ) );
    size_t i = 0;
    for ( const Transactions::value_type& tx : _transactions ) {
        if ( i > 0 )
            s += cc::normal( ", " );
        s += cc::debug( "#" ) + cc::size10( i ) + cc::debug( "/" ) + cc::info( tx.sha3().hex() );
        ++i;
    }
    return s;
}

size_t Client::importTransactionsAsBlock(
    const Transactions& _transactions, u256 _gasPrice, uint64_t _timestamp ) {
    // HACK here was m_blockImportMutex - but now it is acquired in SkaleHost!!!
    // TODO decouple Client and SkaleHost
    int64_t snapshotIntervalSec = chainParams().sChain.snapshotIntervalSec;

    // init last block creation time with only robust time source - timestamp of 1st block!
    if ( number() == 0 ) {
        last_snapshot_creation_time = _timestamp;
        LOG( m_logger ) << "Init last snapshot creation time: "
                        << this->last_snapshot_creation_time;
    } else if ( snapshotIntervalSec > 0 && this->isTimeToDoSnapshot( _timestamp ) ) {
        LOG( m_logger ) << "Last snapshot creation time: " << this->last_snapshot_creation_time;

        if ( m_snapshotHashComputing != nullptr && m_snapshotHashComputing->joinable() )
            m_snapshotHashComputing->join();

        // TODO Make this number configurable
        // thread can be absent - if hash was already there
        // snapshot can be absent too
        // but hash cannot be absent
        auto latest_snapshots = this->m_snapshotManager->getLatestSnasphots();
        if ( latest_snapshots.second ) {
            assert( m_snapshotManager->isSnapshotHashPresent( latest_snapshots.second ) );
            this->last_snapshoted_block_with_hash = latest_snapshots.second;
            m_snapshotManager->leaveNLastSnapshots( 2 );
        }

        // also there might be snapshot on disk
        // and it's time to make it "last with hash"
        if ( last_snapshoted_block_with_hash == 0 ) {
            auto latest_snapshots = this->m_snapshotManager->getLatestSnasphots();
            if ( latest_snapshots.second ) {
                uint64_t time_of_second =
                    blockInfo( this->hashFromNumber( latest_snapshots.second ) ).timestamp();
                if ( time_of_second == ( ( uint64_t ) last_snapshot_creation_time ) )
                    this->last_snapshoted_block_with_hash = latest_snapshots.second;
            }  // if second
        }      // if == 0
    }

    //
    // begin, detect partially executed block
    bool bIsPartial = false;
    dev::h256 shaLastTx = m_state.safeLastExecutedTransactionHash();

    auto iterFound = std::find_if( _transactions.begin(), _transactions.end(),
        [&shaLastTx]( const Transaction& txWalk ) { return txWalk.sha3() == shaLastTx; } );

    // detect partial ONLY if this transaction is not known!
    bIsPartial = iterFound != _transactions.end() && !isKnownTransaction( shaLastTx );

    Transactions vecPassed, vecMissing;
    if ( bIsPartial ) {
        vecPassed.insert( vecPassed.end(), _transactions.begin(), iterFound + 1 );
        vecMissing.insert( vecMissing.end(), iterFound + 1, _transactions.end() );
    }

    size_t cntAll = _transactions.size();
    size_t cntPassed = vecPassed.size();
    size_t cntMissing = vecMissing.size();
    size_t cntExpected = cntMissing;
    if ( bIsPartial ) {
        LOG( m_logger ) << cc::fatal( "PARTIAL CATCHUP DETECTED:" )
                        << cc::warn( " found partially executed block, have " )
                        << cc::size10( cntAll ) << cc::warn( " transaction(s), " )
                        << cc::size10( cntPassed ) << cc::warn( " passed, " )
                        << cc::size10( cntMissing ) << cc::warn( " missing" );
        LOG( m_logger ).flush();
        LOG( m_logger ) << cc::info( "PARTIAL CATCHUP:" )
                        << stat_transactions2str( _transactions, cc::notice( " All " ) );
        LOG( m_logger ).flush();
        LOG( m_logger ) << cc::info( "PARTIAL CATCHUP:" )
                        << stat_transactions2str( vecPassed, cc::notice( " Passed " ) );
        LOG( m_logger ).flush();
        LOG( m_logger ) << cc::info( "PARTIAL CATCHUP:" )
                        << stat_transactions2str( vecMissing, cc::notice( " Missing " ) );
        //        LOG( m_logger ) << cc::info( "PARTIAL CATCHUP:" ) << cc::attention( " Found " )
        //                        << cc::size10( partialTransactionReceipts.size() )
        //                        << cc::attention( " partial transaction receipt(s) inside " )
        //                        << cc::notice( "SAFETY CACHE" );
        LOG( m_logger ).flush();
    }
    // end, detect partially executed block
    //
    size_t cntSucceeded = 0;
    cntSucceeded = syncTransactions(
        _transactions, _gasPrice, _timestamp, bIsPartial ? &vecMissing : nullptr );
    sealUnconditionally( false );
    importWorkingBlock();

    LOG( m_loggerDetail ) << "Total unsafe time so far = "
                          << std::chrono::duration_cast< std::chrono::seconds >(
                                 UnsafeRegion::getTotalTime() )
                                 .count()
                          << " seconds";

    if ( bIsPartial )
        cntSucceeded += cntPassed;
    if ( cntSucceeded != cntAll ) {
        LOG( m_logger ) << cc::fatal( "TX EXECUTION WARNING:" ) << cc::warn( " expected " )
                        << cc::size10( cntAll ) << cc::warn( " transaction(s) to pass, when " )
                        << cc::size10( cntSucceeded ) << cc::warn( " passed with success," )
                        << cc::size10( cntExpected ) << cc::warn( " expected to run and pass" );
        LOG( m_logger ).flush();
    }
    if ( bIsPartial ) {
        LOG( m_logger ) << cc::success( "PARTIAL CATCHUP SUCCESS: with " ) << cc::size10( cntAll )
                        << cc::success( " transaction(s), " ) << cc::size10( cntPassed )
                        << cc::success( " passed, " ) << cc::size10( cntMissing )
                        << cc::success( " missing" );
        LOG( m_logger ).flush();
    }

    if ( chainParams().sChain.nodeGroups.size() > 0 )
        updateIMABLSPublicKey();

    if ( snapshotIntervalSec > 0 ) {
        unsigned block_number = this->number();

        LOG( m_loggerDetail ) << "Block timestamp: " << _timestamp;

        if ( this->isTimeToDoSnapshot( _timestamp ) ) {
            try {
                boost::chrono::high_resolution_clock::time_point t1;
                boost::chrono::high_resolution_clock::time_point t2;
                LOG( m_logger ) << "DOING SNAPSHOT: " << block_number;
                m_debugTracer.tracepoint( "doing_snapshot" );

                t1 = boost::chrono::high_resolution_clock::now();
                m_snapshotManager->doSnapshot( block_number );
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
        auto latest_snapshots = this->m_snapshotManager->getLatestSnasphots();

        // start if thread is free and there is work
        if ( ( m_snapshotHashComputing == nullptr || !m_snapshotHashComputing->joinable() ) &&
             latest_snapshots.second &&
             !m_snapshotManager->isSnapshotHashPresent( latest_snapshots.second ) ) {
            m_snapshotHashComputing.reset( new std::thread( [this, latest_snapshots]() {
                m_debugTracer.tracepoint( "computeSnapshotHash_start" );
                try {
                    boost::chrono::high_resolution_clock::time_point t1;
                    boost::chrono::high_resolution_clock::time_point t2;

                    t1 = boost::chrono::high_resolution_clock::now();
                    this->m_snapshotManager->computeSnapshotHash( latest_snapshots.second );
                    t2 = boost::chrono::high_resolution_clock::now();
                    this->snapshot_hash_calculation_time_ms =
                        boost::chrono::duration_cast< boost::chrono::milliseconds >( t2 - t1 )
                            .count();
                    LOG( m_logger )
                        << "Computed hash for snapshot " << latest_snapshots.second << ": "
                        << m_snapshotManager->getSnapshotHash( latest_snapshots.second );
                    m_debugTracer.tracepoint( "computeSnapshotHash_end" );

                } catch ( const std::exception& ex ) {
                    cerror << cc::fatal( "CRITICAL" ) << " "
                           << cc::warn( dev::nested_exception_what( ex ) )
                           << cc::error( " in computeSnapshotHash(). Exiting..." );
                    cerror << DETAILED_ERROR;
                    cerror << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
                    ExitHandler::exitHandler( SIGABRT, ExitHandler::ec_compute_snapshot_error );
                } catch ( ... ) {
                    cerror << cc::fatal( "CRITICAL" )
                           << cc::error(
                                  " unknown exception in computeSnapshotHash(). "
                                  "Exiting..." );
                    cerror << DETAILED_ERROR;
                    cerror << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
                    ExitHandler::exitHandler( SIGABRT, ExitHandler::ec_compute_snapshot_error );
                }
            } ) );
        }  // if thread
    }      // if snapshots enabled

    // TEMPRORARY FIX!
    // TODO: REVIEW
    tick();

    return cntSucceeded;
    assert( false );
    return 0;
}

size_t Client::syncTransactions(
    const Transactions& _transactions, u256 _gasPrice, uint64_t _timestamp,
    Transactions* vecMissing  // it's non-null only for PARTIAL CATCHUP
) {
    assert( m_skaleHost );

    // HACK remove block verification and put it directly in blockchain!!
    // TODO remove block verification and put it directly in blockchain!!
    while ( m_working.isSealed() ) {
        cnote << "m_working.isSealed. sleeping" << endl;
        usleep( 1000 );
    }

    resyncStateFromChain();

    Timer timer;

    TransactionReceipts newPendingReceipts;
    unsigned goodReceipts;

    ContractStorageLimitPatch::lastBlockTimestamp = blockChain().info().timestamp();

    DEV_WRITE_GUARDED( x_working ) {
        assert( !m_working.isSealed() );

        // assert(m_state.m_db_write_lock.has_value());
        tie( newPendingReceipts, goodReceipts ) =
            m_working.syncEveryone( bc(), _transactions, _timestamp, _gasPrice, vecMissing );
        m_state = m_state.startNew();
    }

    DEV_READ_GUARDED( x_working )
    DEV_WRITE_GUARDED( x_postSeal )
    m_postSeal = m_working;

    // Tell farm about new transaction (i.e. restart mining).
    onPostStateChanged();

    // Tell network about the new transactions.
    m_skaleHost->noteNewTransactions();

    ctrace << cc::debug( "Processed " ) << cc::size10( newPendingReceipts.size() )
           << cc::debug( " transactions in " ) << cc::size10( timer.elapsed() * 1000 )
           << cc::debug( "(" ) << ( bool ) m_syncTransactionQueue << cc::debug( ")" );

    return goodReceipts;
}

void Client::onDeadBlocks( h256s const& _blocks, h256Hash& io_changed ) {
    // insert transactions that we are declaring the dead part of the chain
    for ( auto const& h : _blocks ) {
        LOG( m_loggerDetail ) << cc::warn( "Dead block: " ) << h;
        for ( auto const& t : bc().transactions( h ) ) {
            LOG( m_loggerDetail ) << cc::debug( "Resubmitting dead-block transaction " )
                                  << Transaction( t, CheckTransaction::None );
            ctrace << cc::debug( "Resubmitting dead-block transaction " )
                   << Transaction( t, CheckTransaction::None );
            m_tq.import( t, IfDropped::Retry );
        }
    }

    for ( auto const& h : _blocks )
        appendFromBlock( h, BlockPolarity::Dead, io_changed );
}

void Client::onNewBlocks( h256s const& _blocks, h256Hash& io_changed ) {
    assert( m_skaleHost );

    m_skaleHost->noteNewBlocks();

    for ( auto const& h : _blocks )
        appendFromBlock( h, BlockPolarity::Live, io_changed );
}

void Client::resyncStateFromChain() {
    DEV_READ_GUARDED( x_working )
    if ( bc().currentHash() == m_working.info().parentHash() )
        return;

    restartMining();
}

void Client::restartMining() {
    bool preChanged = false;
    Block newPreMine( chainParams().accountStartNonce );
    DEV_READ_GUARDED( x_preSeal )
    newPreMine = m_preSeal;

    // TODO: use m_postSeal to avoid re-evaluating our own blocks.
    m_state = m_state.startNew();
    preChanged = newPreMine.sync( bc(), m_state );

    if ( preChanged || m_postSeal.author() != m_preSeal.author() ) {
        DEV_WRITE_GUARDED( x_preSeal )
        m_preSeal = newPreMine;
        DEV_WRITE_GUARDED( x_working )
        m_working = newPreMine;
        DEV_READ_GUARDED( x_postSeal )
        if ( !m_postSeal.isSealed() || m_postSeal.info().hash() != newPreMine.info().parentHash() )
            for ( auto const& t : m_postSeal.pending() ) {
                LOG( m_loggerDetail ) << "Resubmitting post-seal transaction " << t;
                //                      ctrace << "Resubmitting post-seal transaction " << t;
                auto ir = m_tq.import( t, IfDropped::Retry );
                if ( ir != ImportResult::Success )
                    onTransactionQueueReady();
            }
        DEV_READ_GUARDED( x_working ) DEV_WRITE_GUARDED( x_postSeal ) m_postSeal = m_working;

        onPostStateChanged();
    }

    // Quick hack for now - the TQ at this point already has the prior pending transactions in it;
    // we should resync with it manually until we are stricter about what constitutes "knowing".
    onTransactionQueueReady();
}

void Client::resetState() {
    Block newPreMine( chainParams().accountStartNonce );
    DEV_READ_GUARDED( x_preSeal )
    newPreMine = m_preSeal;

    DEV_WRITE_GUARDED( x_working )
    m_working = newPreMine;
    DEV_READ_GUARDED( x_working ) DEV_WRITE_GUARDED( x_postSeal ) m_postSeal = m_working;

    onPostStateChanged();
    onTransactionQueueReady();
}

bool Client::isTimeToDoSnapshot( uint64_t _timestamp ) const {
    int snapshotIntervalSec = chainParams().sChain.snapshotIntervalSec;
    return _timestamp / uint64_t( snapshotIntervalSec ) >
           this->last_snapshot_creation_time / uint64_t( snapshotIntervalSec );
}

void Client::setSchainExitTime( uint64_t _timestamp ) const {
    m_instanceMonitor->initRotationParams( _timestamp );
}

void Client::onChainChanged( ImportRoute const& _ir ) {
    //  ctrace << "onChainChanged()";
    h256Hash changeds;
    onDeadBlocks( _ir.deadBlocks, changeds );

    // this should be already done in SkaleHost::createBlock()
    //    for ( auto const& t : _ir.goodTranactions ) {
    //        LOG( m_loggerDetail ) << "Safely dropping transaction " << t.sha3();
    //        m_tq.dropGood( t );
    //    }

    onNewBlocks( _ir.liveBlocks, changeds );
    if ( !isMajorSyncing() )
        resyncStateFromChain();
    noteChanged( changeds );
}

bool Client::remoteActive() const {
    return chrono::system_clock::now() - m_lastGetWork < chrono::seconds( 30 );
}

void Client::onPostStateChanged() {
    LOG( m_loggerDetail ) << cc::notice( "Post state changed." );
    m_signalled.notify_all();
    m_remoteWorking = false;
}

void Client::startSealing() {
    if ( m_wouldSeal == true )
        return;
    LOG( m_logger ) << cc::notice( "Client::startSealing: " ) << author();
    if ( author() ) {
        m_wouldSeal = true;
        m_signalled.notify_all();
    } else
        LOG( m_logger ) << cc::warn( "You need to set an author in order to seal!" );
}

void Client::rejigSealing() {
    if ( ( wouldSeal() || remoteActive() ) && !isMajorSyncing() ) {
        if ( sealEngine()->shouldSeal( this ) ) {
            m_wouldButShouldnot = false;

            LOG( m_loggerDetail ) << cc::notice( "Rejigging seal engine..." );
            DEV_WRITE_GUARDED( x_working ) {
                if ( m_working.isSealed() ) {
                    LOG( m_logger ) << cc::notice( "Tried to seal sealed block..." );
                    return;
                }
                // TODO is that needed? we have "Generating seal on" below
                LOG( m_loggerDetail ) << cc::notice( "Starting to seal block" ) << " "
                                      << cc::warn( "#" ) << cc::num10( m_working.info().number() );

                // TODO Deduplicate code!
                dev::h256 stateRootToSet;
                if ( this->last_snapshoted_block_with_hash > 0 ) {
                    dev::h256 state_root_hash =
                        this->m_snapshotManager->getSnapshotHash( last_snapshoted_block_with_hash );
                    stateRootToSet = state_root_hash;
                }
                // propagate current!
                else if ( this->number() > 0 ) {
                    stateRootToSet =
                        blockInfo( this->hashFromNumber( this->number() ) ).stateRoot();
                } else {
                    stateRootToSet = Client::empty_str_hash;
                }

                m_working.commitToSeal( bc(), m_extraData, stateRootToSet );
            }
            DEV_READ_GUARDED( x_working ) {
                DEV_WRITE_GUARDED( x_postSeal )
                m_postSeal = m_working;
                m_sealingInfo = m_working.info();
            }

            if ( wouldSeal() ) {
                sealEngine()->onSealGenerated( [=]( bytes const& _header ) {
                    LOG( m_logger ) << cc::success( "Block sealed" ) << " " << cc::warn( "#" )
                                    << cc::num10( BlockHeader( _header, HeaderData ).number() );
                    if ( this->submitSealed( _header ) )
                        m_onBlockSealed( _header );
                    else
                        LOG( m_logger ) << cc::error( "Submitting block failed..." );
                } );
                ctrace << cc::notice( "Generating seal on " ) << m_sealingInfo.hash( WithoutSeal )
                       << " " << cc::warn( "#" ) << cc::num10( m_sealingInfo.number() );
                sealEngine()->generateSeal( m_sealingInfo );
            }
        } else
            m_wouldButShouldnot = true;
    }
    if ( !m_wouldSeal )
        sealEngine()->cancelGeneration();
}

void Client::sealUnconditionally( bool submitToBlockChain ) {
    m_wouldButShouldnot = false;

    LOG( m_loggerDetail ) << cc::notice( "Rejigging seal engine..." );
    DEV_WRITE_GUARDED( x_working ) {
        if ( m_working.isSealed() ) {
            LOG( m_logger ) << cc::notice( "Tried to seal sealed block..." );
            return;
        }
        // TODO is that needed? we have "Generating seal on" below
        LOG( m_loggerDetail ) << cc::notice( "Starting to seal block" ) << " " << cc::warn( "#" )
                              << cc::num10( m_working.info().number() );
        // latest hash is really updated after NEXT snapshot already started hash computation!
        // TODO Deduplicate code!
        dev::h256 stateRootToSet;
        if ( this->last_snapshoted_block_with_hash > 0 ) {
            dev::h256 state_root_hash =
                this->m_snapshotManager->getSnapshotHash( last_snapshoted_block_with_hash );
            stateRootToSet = state_root_hash;
        }
        // propagate current!
        else if ( this->number() > 0 ) {
            stateRootToSet = blockInfo( this->hashFromNumber( this->number() ) ).stateRoot();
        } else {
            stateRootToSet = Client::empty_str_hash;
        }

        stateRootToSet = AmsterdamFixPatch::overrideStateRoot( *this ) != dev::h256() ?
                             AmsterdamFixPatch::overrideStateRoot( *this ) :
                             stateRootToSet;

        m_working.commitToSeal( bc(), m_extraData, stateRootToSet );
    }
    DEV_READ_GUARDED( x_working ) {
        DEV_WRITE_GUARDED( x_postSeal )
        m_postSeal = m_working;
        m_sealingInfo = m_working.info();
    }


    // TODO Remove unnecessary de/serializations
    RLPStream headerRlp;
    m_sealingInfo.streamRLP( headerRlp );
    const bytes& header = headerRlp.out();
    BlockHeader header_struct( header, HeaderData );
    LOG( m_logger ) << cc::success( "Block sealed" ) << " #" << cc::num10( header_struct.number() )
                    << " (" << header_struct.hash() << ")";
    std::stringstream ssBlockStats;
    ssBlockStats << cc::success( "Block stats:" ) << "BN:" << number()
                 << ":BTS:" << bc().info().timestamp() << ":TXS:" << TransactionBase::howMany()
                 << ":HDRS:" << BlockHeader::howMany() << ":LOGS:" << LogEntry::howMany()
                 << ":SENGS:" << SealEngineBase::howMany()
                 << ":TXRS:" << TransactionReceipt::howMany() << ":BLCKS:" << Block::howMany()
                 << ":ACCS:" << Account::howMany() << ":BQS:" << BlockQueue::howMany()
                 << ":BDS:" << BlockDetails::howMany() << ":TSS:" << TransactionSkeleton::howMany()
                 << ":UTX:" << TransactionQueue::UnverifiedTransaction::howMany()
                 << ":VTX:" << TransactionQueue::VerifiedTransaction::howMany()
                 << ":CMM:" << bc().getTotalCacheMemory();
    if ( number() % 1000 == 0 ) {
        ssBlockStats << ":RAM:" << getRAMUsage();
        ssBlockStats << ":CPU:" << getCPUUsage();
    }
    LOG( m_logger ) << ssBlockStats.str();


    if ( submitToBlockChain ) {
        if ( this->submitSealed( header ) )
            m_onBlockSealed( header );
        else
            LOG( m_logger ) << cc::error( "Submitting block failed..." );
    } else {
        UpgradableGuard l( x_working );
        {
            UpgradeGuard l2( l );
            if ( m_working.sealBlock( header ) ) {
                m_onBlockSealed( header );
            } else {
                LOG( m_logger ) << cc::error( "Sealing block failed..." );
            }
        }
        DEV_WRITE_GUARDED( x_postSeal )
        m_postSeal = m_working;
    }
}

void Client::importWorkingBlock() {
    DEV_READ_GUARDED( x_working );
    ImportRoute importRoute = bc().import( m_working );
    m_new_block_watch.invoke( m_working );
    onChainChanged( importRoute );
}

void Client::noteChanged( h256Hash const& _filters ) {
    Guard l( x_filtersWatches );
    if ( _filters.size() )
        LOG( m_loggerWatch ) << cc::notice( "noteChanged: " ) << filtersToString( _filters );
    // accrue all changes left in each filter into the watches.
    for ( auto& w : m_watches )
        if ( _filters.count( w.second.id ) ) {
            if ( m_filters.count( w.second.id ) ) {
                LOG( m_loggerWatch ) << "!!! " << w.first << " " << w.second.id.abridged();
                w.second.append_changes( m_filters.at( w.second.id ).changes_ );
            } else if ( m_specialFilters.count( w.second.id ) )
                for ( h256 const& hash : m_specialFilters.at( w.second.id ) ) {
                    LOG( m_loggerWatch )
                        << "!!! " << w.first << " "
                        << ( w.second.id == PendingChangedFilter ?
                                   "pending" :
                                   w.second.id == ChainChangedFilter ? "chain" : "???" );
                    w.second.append_changes( LocalisedLogEntry( SpecialLogEntry, hash ) );
                }
        }
    // clear the filters now.
    for ( auto& i : m_filters )
        i.second.changes_.clear();
    for ( auto& i : m_specialFilters )
        i.second.clear();
}

void Client::doWork( bool _doWait ) {
    bool t = true;
    if ( m_syncBlockQueue.compare_exchange_strong( t, false ) )
        syncBlockQueue();

    if ( m_needStateReset ) {
        resetState();
        m_needStateReset = false;
    }

    t = true;
    bool isSealed = false;
    DEV_READ_GUARDED( x_working )
    isSealed = m_working.isSealed();
    //    if (!isSealed && !isMajorSyncing() && !m_remoteWorking &&
    //    m_syncTransactionQueue.compare_exchange_strong(t, false))
    //        syncTransactionQueue();

    // TEMPRORARY FIX!
    // TODO: REVIEW
    // tick();

    // SKALE Mine only empty blocks! (for tests passing/account balancing)
    rejigSealing();

    callQueuedFunctions();

    DEV_READ_GUARDED( x_working )
    isSealed = m_working.isSealed();
    // If the block is sealed, we have to wait for it to tickle through the block queue
    // (which only signals as wanting to be synced if it is ready).
    if ( !m_syncBlockQueue && ( _doWait || isSealed ) && isWorking() ) {
        MICROPROFILE_SCOPEI( "Client", "m_signalled.wait_for", MP_DIMGRAY );
        std::unique_lock< std::mutex > l( x_signalled );
        m_signalled.wait_for( l, chrono::seconds( 1 ) );
    }
}

void Client::tick() {
    if ( chrono::system_clock::now() - m_lastTick > chrono::seconds( 1 ) ) {
        m_report.ticks++;
        checkWatchGarbage();
        m_bq.tick();
        m_lastTick = chrono::system_clock::now();
        if ( m_report.ticks == 15 )
            LOG( m_loggerDetail ) << activityReport();
    }
}

void Client::checkWatchGarbage() {
    if ( chrono::system_clock::now() - m_lastGarbageCollection > chrono::seconds( 5 ) ) {
        // watches garbage collection
        vector< unsigned > toUninstall;
        DEV_GUARDED( x_filtersWatches )
        for ( auto key : keysOf( m_watches ) )
            if ( ( !m_watches[key].isWS() ) &&
                 m_watches[key].lastPoll != chrono::system_clock::time_point::max() &&
                 chrono::system_clock::now() - m_watches[key].lastPoll >
                     chrono::seconds( 20 ) )  // NB Was 200 for debugging. Normal value is 20!
            {
                toUninstall.push_back( key );
                LOG( m_loggerDetail ) << "GC: Uninstall " << key << " ("
                                      << chrono::duration_cast< chrono::seconds >(
                                             chrono::system_clock::now() - m_watches[key].lastPoll )
                                             .count()
                                      << " s old)";
            }
        for ( auto i : toUninstall )
            uninstallWatch( i );

        // blockchain GC
        bc().garbageCollect();

        m_lastGarbageCollection = chrono::system_clock::now();
    }
}

void Client::prepareForTransaction() {
    startWorking();
}


#ifndef NO_ALETH_STATE
Block Client::blockByNumber(BlockNumber _h) const
{
    try {
        auto hash = ClientBase::hashFromNumber(_h);
        DEV_GUARDED( m_blockImportMutex ) { return Block( bc(), hash, m_state ); }
        assert( false );
        return Block( bc() );
    } catch ( Exception& ex ) {
        ex << errinfo_block( bc().block( bc().currentHash() ) );
        onBadBlock( ex );
        return Block( bc() );
    }
}
#endif

Block Client::latestBlock() const {
    // TODO Why it returns not-filled block??! (see Block ctor)
    try {
        DEV_GUARDED( m_blockImportMutex ) { return Block( bc(), bc().currentHash(), m_state ); }
        assert( false );
        return Block( bc() );
    } catch ( Exception& ex ) {
        ex << errinfo_block( bc().block( bc().currentHash() ) );
        onBadBlock( ex );
        return Block( bc() );
    }
}

void Client::flushTransactions() {
    doWork();
}

Transactions Client::pending() const {
    return m_tq.topTransactions( m_tq.status().current );
}

SyncStatus Client::syncStatus() const {
    // TODO implement this when syncing will be needed
    SyncStatus s;
    s.startBlockNumber = s.currentBlockNumber = s.highestBlockNumber = 0;
    return s;
}

TransactionSkeleton Client::populateTransactionWithDefaults( TransactionSkeleton const& _t ) const {
    TransactionSkeleton ret( _t );

    // Default gas value meets the intrinsic gas requirements of both
    // send value and create contract transactions and is the same default
    // value used by geth and testrpc.
    const u256 defaultTransactionGas = 90000;
    if ( ret.nonce == Invalid256 ) {
        Block block = postSeal();
        block.startReadState();
        ret.nonce = max< u256 >( block.transactionsFrom( ret.from ), m_tq.maxNonce( ret.from ) );
    }
    if ( ret.gasPrice == Invalid256 )
        ret.gasPrice = gasBidPrice();
    if ( ret.gas == Invalid256 )
        ret.gas = defaultTransactionGas;

    return ret;
}

bool Client::submitSealed( bytes const& _header ) {
    bytes newBlock;
    {
        UpgradableGuard l( x_working );
        {
            UpgradeGuard l2( l );
            if ( !m_working.sealBlock( _header ) )
                return false;
        }
        DEV_WRITE_GUARDED( x_postSeal )
        m_postSeal = m_working;
        newBlock = m_working.blockData();
    }

    // OPTIMISE: very inefficient to not utilise the existing OverlayDB in m_postSeal that contains
    // all trie changes.
    return queueBlock( newBlock, true ) == ImportResult::Success;
}

h256 Client::submitTransaction( TransactionSkeleton const& _t, Secret const& _secret ) {
    TransactionSkeleton ts = populateTransactionWithDefaults( _t );
    ts.from = toAddress( _secret );
    Transaction t( ts, _secret );
    return importTransaction( t );
}

// TODO: Check whether multiTransactionMode enabled on contracts
h256 Client::importTransaction( Transaction const& _t ) {
    prepareForTransaction();

    // Use the Executive to perform basic validation of the transaction
    // (e.g. transaction signature, account balance) using the state of
    // the latest block in the client's blockchain. This can throw but
    // we'll catch the exception at the RPC level.

    const_cast< Transaction& >( _t ).checkOutExternalGas( chainParams().externalGasDifficulty );

    // throws in case of error
    skale::State state;
    u256 gasBidPrice;

    DEV_GUARDED( m_blockImportMutex ) {
        state = this->state().startRead();
        gasBidPrice = this->gasBidPrice();
    }

    Executive::verifyTransaction( _t,
        bc().number() ? this->blockInfo( bc().currentHash() ) : bc().genesis(), state,
        *bc().sealEngine(), 0, gasBidPrice, chainParams().sChain.multiTransactionMode );

    ImportResult res;
    if ( chainParams().sChain.multiTransactionMode && state.getNonce( _t.sender() ) < _t.nonce() &&
         m_tq.maxCurrentNonce( _t.sender() ) != _t.nonce() ) {
        res = m_tq.import( _t, IfDropped::Ignore, true );
    } else {
        res = m_tq.import( _t );
    }

    switch ( res ) {
    case ImportResult::Success:
        break;
    case ImportResult::ZeroSignature:
        BOOST_THROW_EXCEPTION( ZeroSignatureTransaction() );
    case ImportResult::SameNonceAlreadyInQueue:
        BOOST_THROW_EXCEPTION( SameNonceAlreadyInQueue() );
    case ImportResult::AlreadyKnown:
        BOOST_THROW_EXCEPTION( PendingTransactionAlreadyExists() );
    case ImportResult::AlreadyInChain:
        BOOST_THROW_EXCEPTION( TransactionAlreadyInChain() );
    default:
        BOOST_THROW_EXCEPTION( UnknownTransactionValidationError() );
    }

    m_new_pending_transaction_watch.invoke( _t );

    return _t.sha3();
}

// TODO: remove try/catch, allow exceptions




ExecutionResult Client::call( Address const& _from, u256 _value, Address _dest, bytes const& _data,
    u256 _gas, u256 _gasPrice,
#ifndef NO_ALETH_STATE
                              BlockNumber _blockNumber,
#endif
                              FudgeFactor _ff ) {
    ExecutionResult ret;
    try {


        Block temp = latestBlock();

#ifndef NO_ALETH_STATE
        Block historicBlock = blockByNumber(_blockNumber);
        if (_blockNumber < bc().number()) {
            // historic state
            try
            {
                u256 nonce = max<u256>(historicBlock.transactionsFrom(_from), m_tq.maxNonce(_from));
                u256 gas = _gas == Invalid256 ? gasLimitRemaining() : _gas;
                u256 gasPrice = _gasPrice == Invalid256 ? gasBidPrice() : _gasPrice;
                Transaction t(_value, gasPrice, gas, _dest, _data, nonce);
                t.forceSender(_from);
                if (_ff == FudgeFactor::Lenient)
                    temp.mutableState().addBalance(_from, (u256)(t.gas() * t.gasPrice() + t.value()));
                ret = temp.executeAlethCall(bc().lastBlockHashes(), t);
            }
            catch (...)
            {
                cwarn << boost::current_exception_diagnostic_information();
            }
            return ret;

        }
#endif

        // TODO there can be race conditions between prev and next line!
        skale::State readStateForLock = temp.mutableState().startRead();
        u256 nonce = max< u256 >( temp.transactionsFrom( _from ), m_tq.maxNonce( _from ) );
        u256 gas = _gas == Invalid256 ? gasLimitRemaining() : _gas;
        u256 gasPrice = _gasPrice == Invalid256 ? gasBidPrice() : _gasPrice;
        Transaction t( _value, gasPrice, gas, _dest, _data, nonce );
        t.forceSender( _from );
        t.forceChainId( chainParams().chainID );
        t.checkOutExternalGas( ~u256( 0 ) );
        if ( _ff == FudgeFactor::Lenient )
            temp.mutableState().addBalance( _from, ( u256 )( t.gas() * t.gasPrice() + t.value() ) );
        ret = temp.execute( bc().lastBlockHashes(), t, skale::Permanence::Reverted );
    } catch ( InvalidNonce const& in ) {
        LOG( m_logger ) << "exception in client call(1):"
                        << boost::current_exception_diagnostic_information() << std::endl;
        throw std::runtime_error( "call with invalid nonce" );
    } catch ( ... ) {
        LOG( m_logger ) << "exception in client call(2):"
                        << boost::current_exception_diagnostic_information() << std::endl;
        throw;
    }
    return ret;
}

void Client::initHashes() {
    int snapshotIntervalSec = chainParams().sChain.snapshotIntervalSec;
    assert( snapshotIntervalSec > 0 );
    ( void ) snapshotIntervalSec;

    auto latest_snapshots = this->m_snapshotManager->getLatestSnasphots();

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
            this->m_snapshotManager->getBlockTimestamp( latest_snapshots.second, chainParams() );

        // one snapshot
    } else if ( latest_snapshots.second ) {
        assert( latest_snapshots.second != 1 );  // 1 can never be snapshotted
        assert( this->number() > 0 );            // we created snapshot somehow

        // whether it is local or downloaded - we shall ignore it's hash but use it's time
        // see also how last_snapshoted_block_with_hash is updated in importTransactionsAsBlock
        // h256 h2 = this->hashFromNumber( latest_snapshots.second );
        // uint64_t time_of_second = blockInfo( h2 ).timestamp();

        this->last_snapshoted_block_with_hash = -1;
        // last_snapshot_creation_time = time_of_second;

        last_snapshot_creation_time =
            this->m_snapshotManager->getBlockTimestamp( latest_snapshots.second, chainParams() );

        // no snapshots yet
    } else {
        this->last_snapshoted_block_with_hash = -1;

        if ( this->number() >= 1 )
            last_snapshot_creation_time = blockInfo( this->hashFromNumber( 1 ) ).timestamp();
        else
            this->last_snapshot_creation_time = 0;
    }

    LOG( m_logger ) << "Latest snapshots init: " << latest_snapshots.first << " "
                    << latest_snapshots.second << " -> " << this->last_snapshoted_block_with_hash;
    LOG( m_logger ) << "Fake Last snapshot creation time: " << last_snapshot_creation_time;
}

void Client::initIMABLSPublicKey() {
    if ( number() == 0 ) {
        imaBLSPublicKeyGroupIndex = 0;
        return;
    }

    uint64_t currentBlockTimestamp = blockInfo( hashFromNumber( number() ) ).timestamp();
    uint64_t previousBlockTimestamp = blockInfo( hashFromNumber( number() - 1 ) ).timestamp();

    // always returns it != end() because current finish ts equals to uint64_t(-1)
    auto it = std::find_if( chainParams().sChain.nodeGroups.begin(),
        chainParams().sChain.nodeGroups.end(),
        [&currentBlockTimestamp](
            const dev::eth::NodeGroup& ng ) { return currentBlockTimestamp <= ng.finishTs; } );
    assert( it != chainParams().sChain.nodeGroups.end() );

    if ( it != chainParams().sChain.nodeGroups.begin() ) {
        auto prevIt = std::prev( it );
        if ( currentBlockTimestamp >= prevIt->finishTs &&
             previousBlockTimestamp < prevIt->finishTs )
            it = prevIt;
    }

    imaBLSPublicKeyGroupIndex = std::distance( chainParams().sChain.nodeGroups.begin(), it );
}

void Client::updateIMABLSPublicKey() {
    uint64_t blockTimestamp = blockInfo( hashFromNumber( number() ) ).timestamp();
    uint64_t currentFinishTs = chainParams().sChain.nodeGroups[imaBLSPublicKeyGroupIndex].finishTs;
    if ( blockTimestamp >= currentFinishTs )
        ++imaBLSPublicKeyGroupIndex;
    assert( imaBLSPublicKeyGroupIndex < chainParams().sChain.nodeGroups.size() );
}

// new block watch
unsigned Client::installNewBlockWatch(
    std::function< void( const unsigned&, const Block& ) >& fn ) {
    return m_new_block_watch.install( fn );
}
bool Client::uninstallNewBlockWatch( const unsigned& k ) {
    return m_new_block_watch.uninstall( k );
}

// new pending transation watch
unsigned Client::installNewPendingTransactionWatch(
    std::function< void( const unsigned&, const Transaction& ) >& fn ) {
    return m_new_pending_transaction_watch.install( fn );
}
bool Client::uninstallNewPendingTransactionWatch( const unsigned& k ) {
    return m_new_pending_transaction_watch.uninstall( k );
}

uint64_t Client::submitOracleRequest( const string& _spec, string& _receipt ) {
    assert( m_skaleHost );
    uint64_t status = -1;
    if ( m_skaleHost )
        status = m_skaleHost->submitOracleRequest( _spec, _receipt );
    else
        throw runtime_error( "Instance of SkaleHost was not properly created." );
    return status;
}

uint64_t Client::checkOracleResult( const string& _receipt, string& _result ) {
    assert( m_skaleHost );
    uint64_t status = -1;
    if ( m_skaleHost )
        status = m_skaleHost->checkOracleResult( _receipt, _result );
    else
        throw runtime_error( "Instance of SkaleHost was not properly created." );
    return status;
}

const dev::h256 Client::empty_str_hash =
    dev::h256( "66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925" );


#ifndef NO_ALETH_STATE
u256 Client::alethStateBalanceAt(Address _a, BlockNumber _block) const
{
    return blockByNumber(_block).mutableState().mutableAlethState().balance(_a);
}

u256 Client::alethStateCountAt(Address _a, BlockNumber _block) const
{
    return blockByNumber(_block).mutableState().mutableAlethState().getNonce(_a);
}

u256 Client::alethStateAt(Address _a, u256 _l, BlockNumber _block) const
{
    return blockByNumber(_block).mutableState().mutableAlethState().storage(_a, _l);
}

h256 Client::alethStateRootAt(Address _a, BlockNumber _block) const
{
    return blockByNumber(_block).mutableState().mutableAlethState().storageRoot(_a);
}

bytes Client::alethStateCodeAt(Address _a, BlockNumber _block) const
{
    return blockByNumber(_block).mutableState().mutableAlethState().code(_a);
}


#endif
