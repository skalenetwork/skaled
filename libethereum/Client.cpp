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
#include <chrono>
#include <memory>
#include <thread>

#include <libdevcore/microprofile.h>

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
    std::shared_ptr< GasPricer > _gpForAdoption, fs::path const& _dbPath,
    fs::path const& _snapshotPath, WithExisting _forceAction, TransactionQueue::Limits const& _l )
    : Worker( "Client", 0 ),
      m_bc( _params, _dbPath, _forceAction,
          []( unsigned d, unsigned t ) {
              std::cerr << "REVISING BLOCKCHAIN: Processed " << d << " of " << t << "...\r";
          } ),
      m_tq( _l ),
      m_gp( _gpForAdoption ? _gpForAdoption : make_shared< TrivialGasPricer >() ),
      m_preSeal( chainParams().accountStartNonce ),
      m_postSeal( chainParams().accountStartNonce ),
      m_working( chainParams().accountStartNonce ) {
    init( _dbPath, _snapshotPath, _forceAction, _networkID );
}

Client::~Client() {
    m_new_block_watch.uninstallAll();
    m_new_pending_transaction_watch.uninstallAll();

    if ( m_skaleHost )
        m_skaleHost->stopWorking();  // TODO Find and document a systematic way to sart/stop all
                                     // workers
    else
        cerror << "Instance of SkaleHost was not properly created.";

    m_signalled.notify_all();  // to wake up the thread from Client::doWork()
    stopWorking();

    m_tq.HandleDestruction();  // l_sergiy: destroy transaction queue earlier
    m_bq.stop();               // l_sergiy: added to stop block queue processing

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

void Client::init( fs::path const& _dbPath, fs::path const& _snapshotDownloadPath,
    WithExisting _forceAction, u256 _networkId ) {
    DEV_TIMED_FUNCTION_ABOVE( 500 );
    m_networkId = _networkId;

    // Cannot be opened until after blockchain is open, since BlockChain may upgrade the database.
    // TODO: consider returning the upgrade mechanism here. will delaying the opening of the
    // blockchain database until after the construction.
    m_state = State( chainParams().accountStartNonce, _dbPath, bc().genesisHash(),
        BaseState::PreExisting, chainParams().accountInitialFunds );

    if ( m_state.empty() ) {
        m_state.startWrite().populateFrom( bc().chainParams().genesisState );
    };
    // LAZY. TODO: move genesis state construction/commiting to stateDB openning and have this
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

    // create Ethereum capability only if we're not downloading the snapshot
    if ( _snapshotDownloadPath.empty() ) {
        //        auto ethHostCapability =
        //            make_shared<EthereumHost>(_extNet, bc(), m_stateDB, m_tq, m_bq, _networkId);
        //        _extNet.registerCapability(ethHostCapability);
        //        if(!m_skaleHost)
        //            m_skaleHost = make_shared< SkaleHost >( *this, m_tq );
        //        m_skaleHost->startWorking();
    }

    // create Warp capability if we either download snapshot or can give out snapshot
    auto const importedSnapshot = importedSnapshotPath( _dbPath, bc().genesisHash() );
    bool const importedSnapshotExists = fs::exists( importedSnapshot );
    if ( !_snapshotDownloadPath.empty() || importedSnapshotExists ) {
        std::shared_ptr< SnapshotStorageFace > snapshotStorage(
            importedSnapshotExists ? createSnapshotStorage( importedSnapshot ) : nullptr );
        //        auto warpHostCapability = make_shared<WarpHostCapability>(
        //            _extNet, bc(), _networkId, _snapshotDownloadPath, snapshotStorage);
        //        _extNet.registerCapability(warpHostCapability);
        //        m_warpHost = warpHostCapability;
    }

    if ( _dbPath.size() )
        Defaults::setDBPath( _dbPath );
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
    stopWorking();
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
    LOG( m_loggerDetail ) << "startedWorking()";

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

void Client::reopenChain( WithExisting _we ) {
    reopenChain( bc().chainParams(), _we );
}

void Client::reopenChain( ChainParams const& _p, WithExisting _we ) {
    m_signalled.notify_all();  // to wake up the thread from Client::doWork()
    bool wasSealing = wouldSeal();
    if ( wasSealing )
        stopSealing();
    stopWorking();

    m_tq.clear();
    m_bq.clear();
    sealEngine()->cancelGeneration();

    {
        WriteGuard l( x_postSeal );
        WriteGuard l2( x_preSeal );
        WriteGuard l3( x_working );

        m_preSeal = Block( chainParams().accountStartNonce );
        m_postSeal = Block( chainParams().accountStartNonce );
        m_working = Block( chainParams().accountStartNonce );

        bc().reopen( _p, _we );
        if ( !m_state.connected() ) {
            m_state = State( Invalid256, Defaults::dbPath(), bc().genesisHash() );
        }
        if ( _we == WithExisting::Kill ) {
            State writer = m_state.startWrite();
            writer.clearAll();
            writer.populateFrom( bc().chainParams().genesisState );
        }

        m_preSeal = bc().genesisBlock( m_state );
        m_preSeal.setAuthor( _p.author );
        m_postSeal = m_preSeal;
        m_working = Block( chainParams().accountStartNonce );
    }

    // SKALE    m_consensusHost->reset();

    startedWorking();
    doWork();

    startWorking();
    if ( wasSealing )
        startSealing();
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

size_t Client::importTransactionsAsBlock( const Transactions& _transactions, uint64_t _timestamp ) {
    DEV_GUARDED( m_blockImportMutex ) {
        size_t n_succeeded = syncTransactions( _transactions, _timestamp );
        sealUnconditionally( false );
        importWorkingBlock();
        return n_succeeded;
    }
    assert( false );
    return 0;
}

size_t Client::syncTransactions( const Transactions& _transactions, uint64_t _timestamp ) {
    assert( m_skaleHost );

    // HACK remove block verification and put it directly in blockchain!!
    // TODO remove block verification and put it directly in blockchain!!
    while ( m_working.isSealed() )
        usleep( 1000 );

    resyncStateFromChain();

    Timer timer;

    h256Hash changeds;
    TransactionReceipts newPendingReceipts;
    unsigned goodReceipts;

    DEV_WRITE_GUARDED( x_working ) {
        assert( !m_working.isSealed() );

        //        assert(m_state.m_db_write_lock.has_value());
        tie( newPendingReceipts, goodReceipts ) =
            m_working.syncEveryone( bc(), _transactions, _timestamp );
        m_state.updateToLatestVersion();
    }

    DEV_READ_GUARDED( x_working )
    DEV_WRITE_GUARDED( x_postSeal )
    m_postSeal = m_working;

    DEV_READ_GUARDED( x_postSeal )
    for ( size_t i = 0; i < newPendingReceipts.size(); i++ )
        appendFromNewPending( newPendingReceipts[i], changeds, m_postSeal.pending()[i].sha3() );

    // Tell farm about new transaction (i.e. restart mining).
    onPostStateChanged();

    // Tell watches about the new transactions.
    noteChanged( changeds );

    // Tell network about the new transactions.
    m_skaleHost->noteNewTransactions();

    ctrace << "Processed " << newPendingReceipts.size() << " transactions in"
           << ( timer.elapsed() * 1000 ) << "(" << ( bool ) m_syncTransactionQueue << ")";

    return goodReceipts;
}

void Client::onDeadBlocks( h256s const& _blocks, h256Hash& io_changed ) {
    // insert transactions that we are declaring the dead part of the chain
    for ( auto const& h : _blocks ) {
        LOG( m_loggerDetail ) << "Dead block: " << h;
        for ( auto const& t : bc().transactions( h ) ) {
            LOG( m_loggerDetail ) << "Resubmitting dead-block transaction "
                                  << Transaction( t, CheckTransaction::None );
            ctrace << "Resubmitting dead-block transaction "
                   << Transaction( t, CheckTransaction::None );
            m_tq.import( t, IfDropped::Retry );
        }
    }

    for ( auto const& h : _blocks )
        appendFromBlock( h, BlockPolarity::Dead, io_changed );
}

void Client::onNewBlocks( h256s const& _blocks, h256Hash& io_changed ) {
    assert( m_skaleHost );

    // remove transactions from m_tq nicely rather than relying on out of date nonce later on.
    for ( auto const& h : _blocks )
        LOG( m_loggerDetail ) << "Live block: " << h;

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
    m_state.updateToLatestVersion();
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

void Client::onChainChanged( ImportRoute const& _ir ) {
    //  ctrace << "onChainChanged()";
    h256Hash changeds;
    onDeadBlocks( _ir.deadBlocks, changeds );
    for ( auto const& t : _ir.goodTranactions ) {
        LOG( m_loggerDetail ) << "Safely dropping transaction " << t.sha3();
        m_tq.dropGood( t );
    }
    onNewBlocks( _ir.liveBlocks, changeds );
    if ( !isMajorSyncing() )
        resyncStateFromChain();
    noteChanged( changeds );
}

bool Client::remoteActive() const {
    return chrono::system_clock::now() - m_lastGetWork < chrono::seconds( 30 );
}

void Client::onPostStateChanged() {
    LOG( m_loggerDetail ) << "Post state changed.";
    m_signalled.notify_all();
    m_remoteWorking = false;
}

void Client::startSealing() {
    if ( m_wouldSeal == true )
        return;
    LOG( m_logger ) << "Mining Beneficiary: " << author();
    if ( author() ) {
        m_wouldSeal = true;
        m_signalled.notify_all();
    } else
        LOG( m_logger ) << "You need to set an author in order to seal!";
}

void Client::rejigSealing() {
    if ( ( wouldSeal() || remoteActive() ) && !isMajorSyncing() ) {
        if ( sealEngine()->shouldSeal( this ) ) {
            m_wouldButShouldnot = false;

            LOG( m_loggerDetail ) << "Rejigging seal engine...";
            DEV_WRITE_GUARDED( x_working ) {
                if ( m_working.isSealed() ) {
                    LOG( m_logger ) << "Tried to seal sealed block...";
                    return;
                }
                // TODO is that needed? we have "Generating seal on" below
                LOG( m_loggerDetail ) << "Starting to seal block #" << m_working.info().number();
                m_working.commitToSeal( bc(), m_extraData );
            }
            DEV_READ_GUARDED( x_working ) {
                DEV_WRITE_GUARDED( x_postSeal )
                m_postSeal = m_working;
                m_sealingInfo = m_working.info();
            }

            if ( wouldSeal() ) {
                sealEngine()->onSealGenerated( [=]( bytes const& _header ) {
                    LOG( m_logger )
                        << "Block sealed #" << BlockHeader( _header, HeaderData ).number();
                    if ( this->submitSealed( _header ) )
                        m_onBlockSealed( _header );
                    else
                        LOG( m_logger ) << "Submitting block failed...";
                } );
                ctrace << "Generating seal on " << m_sealingInfo.hash( WithoutSeal ) << " #"
                       << m_sealingInfo.number();
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

    LOG( m_loggerDetail ) << "Rejigging seal engine...";
    DEV_WRITE_GUARDED( x_working ) {
        if ( m_working.isSealed() ) {
            LOG( m_logger ) << "Tried to seal sealed block...";
            return;
        }
        // TODO is that needed? we have "Generating seal on" below
        LOG( m_loggerDetail ) << "Starting to seal block #" << m_working.info().number();
        m_working.commitToSeal( bc(), m_extraData );
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
    LOG( m_logger ) << "Block sealed #" << BlockHeader( header, HeaderData ).number();
    if ( submitToBlockChain ) {
        if ( this->submitSealed( header ) )
            m_onBlockSealed( header );
        else
            LOG( m_logger ) << "Submitting block failed...";
    } else {
        UpgradableGuard l( x_working );
        {
            UpgradeGuard l2( l );
            if ( m_working.sealBlock( header ) ) {
                m_onBlockSealed( header );
            } else {
                LOG( m_logger ) << "Sealing block failed...";
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
        LOG( m_loggerWatch ) << "noteChanged: " << filtersToString( _filters );
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

    tick();

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
            if ( m_watches[key].lastPoll != chrono::system_clock::time_point::max() &&
                 chrono::system_clock::now() - m_watches[key].lastPoll >
                     chrono::seconds( 200 ) )  // HACK Changed from 20 to 200 for debugging!
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
    // TODO implement whis when syncing will be needed
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

h256 Client::importTransaction( Transaction const& _t ) {
    prepareForTransaction();

    // Use the Executive to perform basic validation of the transaction
    // (e.g. transaction signature, account balance) using the state of
    // the latest block in the client's blockchain. This can throw but
    // we'll catch the exception at the RPC level.

    const_cast< Transaction& >( _t ).checkOutExternalGas( chainParams().externalGasDifficulty );

    // throws in case of error
    Executive::verifyTransaction( _t,
        bc().number() ? this->blockInfo( bc().currentHash() ) : bc().genesis(),
        this->state().startRead(), *bc().sealEngine(), 0 );

    ImportResult res = m_tq.import( _t );
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
    u256 _gas, u256 _gasPrice, FudgeFactor _ff ) {
    ExecutionResult ret;
    try {
        Block temp = latestBlock();
        u256 nonce = max< u256 >( temp.transactionsFrom( _from ), m_tq.maxNonce( _from ) );
        u256 gas = _gas == Invalid256 ? gasLimitRemaining() : _gas;
        u256 gasPrice = _gasPrice == Invalid256 ? gasBidPrice() : _gasPrice;
        Transaction t( _value, gasPrice, gas, _dest, _data, nonce );
        t.forceSender( _from );
        t.checkOutExternalGas( ~u256( 0 ) );
        if ( _ff == FudgeFactor::Lenient )
            temp.mutableState().addBalance( _from, ( u256 )( t.gas() * t.gasPrice() + t.value() ) );
        ret = temp.execute( bc().lastBlockHashes(), t, Permanence::Reverted );
    } catch ( InvalidNonce const& in ) {
        std::cout << "exception in client call(1):"
                  << boost::current_exception_diagnostic_information() << std::endl;
        throw std::runtime_error( "call with invalid nonce" );
    } catch ( ... ) {
        std::cout << "exception in client call(2):"
                  << boost::current_exception_diagnostic_information() << std::endl;
        throw;
    }
    return ret;
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
