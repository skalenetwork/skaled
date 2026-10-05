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
/** @file Block.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Block.h"

#include "BlockChain.h"
#include "Defaults.h"
#include "Executive.h"
#include "ExtVM.h"
#include "GenesisInfo.h"
#include "SchainPatch.h"
#include "TransactionQueue.h"
#include <libdevcore/Assertions.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/TrieHash.h>
#include <libethcore/Exceptions.h>
#include <libethcore/SealEngine.h>
#include <libevm/VMFactory.h>
#include <libskale/SkipInvalidTransactionsPatch.h>
#include <libskale/StateProgressLog.h>
#include <boost/filesystem.hpp>
#include <ctime>
#include <memory>

#include <libdevcore/microprofile.h>

#include <skutils/console_colors.h>

#ifdef BITE
#include <libethereum/Precompiled.h>
#include <libethereum/SkaleHost.h>
#endif

using namespace std;
using namespace dev;
using namespace dev::eth;
namespace fs = boost::filesystem;
using skale::BaseState;
using namespace skale::error;
using skale::Permanence;
using skale::State;

#define ETH_TIMED_ENACTMENTS 1

static const unsigned c_maxSyncTransactions = 1024;

namespace {
class DummyLastBlockHashes : public eth::LastBlockHashesFace {
public:
    h256s precedingHashes( h256 const& /* _mostRecentHash */ ) const override { return {}; }
    void clear() override {}
};

bool shouldKeepRejectedTransactionQueued( Transaction const& _tx, State const& _state ) {
    if ( _tx.isInvalid() || !_tx.hasSignature() || _tx.hasZeroSignature() )
        return false;

    return _tx.nonce() > _state.getNonce( _tx.safeSender() );
}

bool transactionNeedsQueueCleanup(
    Transaction const& _tx, ExecutionResult const& _res, State const& _state ) {
    if ( _res.excepted != TransactionException::WouldNotBeInBlock )
        return true;

    return !shouldKeepRejectedTransactionQueued( _tx, _state );
}

bool receiptAdvancedGas( TransactionReceipt const& _receipt, u256 const& _previousCumulativeGas ) {
    return _receipt.cumulativeGasUsed() != _previousCumulativeGas;
}

bool notifyConsumedTransactions( Block::OnTransactionConsumed const& _onTransactionConsumed,
    Transactions const& _transactions ) {
    if ( !_onTransactionConsumed )
        return false;

    bool needsQueueReadyNotification = false;
    for ( auto const& tx : _transactions )
        needsQueueReadyNotification = _onTransactionConsumed( tx ) || needsQueueReadyNotification;
    return needsQueueReadyNotification;
}

}  // namespace

Block::Block( BlockChain const& _bc, boost::filesystem::path const& _dbPath,
    dev::h256 const& _genesis, BaseState _bs, Address const& _author )
    : m_state( Invalid256, _dbPath, _genesis, _bs ),
      m_precommit( Invalid256 ),
      m_author( _author ) {
    noteChain( _bc );
    m_previousBlock.clear();
    m_currentBlock.clear();
}

Block::Block( const BlockChain& _bc, h256 const& _hash, const State& _state, BaseState /*_bs*/,
    const Address& _author )
    : m_state( _state ), m_precommit( Invalid256 ), m_author( _author ) {
    noteChain( _bc );
    m_previousBlock.clear();
    m_currentBlock.clear();

    if ( !_bc.isKnown( _hash ) ) {
        // Might be worth throwing here.
        BOOST_LOG( m_loggerWarning ) << "Invalid block given for state population: " << _hash;
        BOOST_THROW_EXCEPTION( BlockNotFound() << errinfo_target( _hash ) );
    }

#ifndef HISTORIC_STATE

    if ( _bc.currentHash() != _hash && _bc.genesisHash() != _hash ) {
        throw std::logic_error(
            "Can't populate block with historical state because it is not supported" );
    }

#endif

    auto b = _bc.block( _hash );
    BlockHeader bi( b );  // No need to check - it's already in the DB.
    m_currentBlock = bi;
    if ( !bi.number() ) {
        // Genesis required:
        // We know there are no transactions, so just populate directly.
        sync( _bc, _hash, bi );
    } else {
        auto parentHash = bi.parentHash();
        if ( !_bc.isKnown( parentHash ) ) {
            // Might be worth throwing here.
            BOOST_LOG( m_loggerWarning )
                << "Invalid parent hash given for population " << parentHash;
            BOOST_THROW_EXCEPTION( BlockNotFound() << errinfo_target( parentHash ) );
        }
        auto pb = _bc.block( parentHash );
        m_previousBlock = BlockHeader( pb );
    }
}

Block::Block( Block const& _s )
    : m_state( _s.m_state ),
      m_transactions( _s.m_transactions ),
      m_receipts( _s.m_receipts ),
      m_transactionSet( _s.m_transactionSet ),
      m_precommit( _s.m_state ),
      m_previousBlock( _s.m_previousBlock ),
      m_currentBlock( _s.m_currentBlock ),
      m_currentBytes( _s.m_currentBytes ),
      m_author( _s.m_author ),
      m_sealEngine( _s.m_sealEngine )
#ifdef BITE
      ,
      m_ctxHashesLists( _s.m_ctxHashesLists )
#endif
{
    m_committedToSeal = false;
}

Block& Block::operator=( Block const& _s ) {
    if ( &_s == this )
        return *this;

    m_state = _s.m_state;
    m_transactions = _s.m_transactions;
    m_receipts = _s.m_receipts;
    m_transactionSet = _s.m_transactionSet;
    m_previousBlock = _s.m_previousBlock;
    m_currentBlock = _s.m_currentBlock;
    m_currentBytes = _s.m_currentBytes;
    m_author = _s.m_author;
    m_sealEngine = _s.m_sealEngine;

#ifdef BITE
    m_ctxHashesLists = _s.m_ctxHashesLists;
#endif

    m_precommit = m_state;
    m_committedToSeal = false;
    return *this;
}


// make a lightweight read only copy
// we only copy the fields we need for eth_call
// in particular we do not copy receipts and transactions
// as well as raw bytes
Block Block::getReadOnlyCopy() const {
    Block copy( Null );
    copy.m_state = m_state.createReadOnlySnapBasedCopy();
    copy.m_author = m_author;
    copy.m_sealEngine = m_sealEngine;
    copy.m_committedToSeal = false;
    copy.m_precommit = m_state;
    copy.m_currentBlock = m_currentBlock;
    copy.m_previousBlock = m_previousBlock;
    return copy;
};


void Block::resetCurrent( int64_t _timestamp ) {
    m_transactions.clear();
    m_receipts.clear();
    m_transactionSet.clear();
#ifdef BITE
    m_ctxHashesLists.clear();
#endif
    m_currentBlock = BlockHeader();
    m_currentBlock.setAuthor( m_author );
    m_currentBlock.setTimestamp( _timestamp );  // max( m_previousBlock.timestamp() + 1, _timestamp
                                                // ) );
    m_currentBytes.clear();
    sealEngine()->populateFromParent( m_currentBlock, m_previousBlock );

    // TODO: check.

    m_committedToSeal = false;

    performIrregularModifications();
    updateBlockhashContract();


    m_state = m_state.createStateCopyAndClearCaches();
}

SealEngineFace* Block::sealEngine() const {
    if ( !m_sealEngine )
        BOOST_THROW_EXCEPTION( ChainOperationWithUnknownBlockChain() );
    return m_sealEngine;
}

void Block::noteChain( BlockChain const& _bc ) {
    if ( !m_sealEngine ) {
        m_state.noteAccountStartNonce( _bc.chainParams().getAccountStartNonce() );
        m_precommit.noteAccountStartNonce( _bc.chainParams().getAccountStartNonce() );
        m_sealEngine = _bc.sealEngine();
    }
}

PopulationStatistics Block::populateFromChain(
    BlockChain const& _bc, h256 const& _h, ImportRequirements::value _ir ) {
    noteChain( _bc );

    PopulationStatistics ret{ 0.0, 0.0 };

    if ( !_bc.isKnown( _h ) ) {
        // Might be worth throwing here.
        BOOST_LOG( m_loggerWarning ) << "Invalid block given for state population: " << _h;
        BOOST_THROW_EXCEPTION( BlockNotFound() << errinfo_target( _h ) );
    }

    if ( _bc.currentHash() != _h && _bc.genesisHash() != _h ) {
        throw std::logic_error(
            "Can't populate block with historical state because it is not supported" );
    } else {
        throw std::logic_error( "Not implemented. And maybe should not." );
    }

    auto b = _bc.block( _h );
    BlockHeader bi( b );  // No need to check - it's already in the DB.
    if ( bi.number() ) {
        // Non-genesis:

        // 1. Start at parent's end state (state root).
        BlockHeader bip( _bc.block( bi.parentHash() ) );
        sync( _bc, bi.parentHash(), bip );

        // 2. Enact the block's transactions onto this state.
        m_author = bi.author();
        Timer t;
        auto vb = _bc.verifyBlock(
            &b, function< void( Exception& ) >(), _ir | ImportRequirements::TransactionBasic );
        ret.verify = t.elapsed();
        t.restart();
        enact( vb, _bc );
        ret.enact = t.elapsed();
    } else {
        // Genesis required:
        // We know there are no transactions, so just populate directly.
        std::logic_error( "Not implemented" );
        sync( _bc, _h, bi );
    }

    return ret;
}

bool Block::sync( BlockChain const& _bc ) {
    return sync( _bc, _bc.currentHash() );
}

bool Block::sync( BlockChain const& _bc, State const& _state ) {
    m_state = _state;
    m_precommit = _state;
    return sync( _bc );
}

bool Block::sync( BlockChain const& _bc, h256 const& _block, BlockHeader const& _bi ) {
    noteChain( _bc );

    bool ret = false;
    // BLOCK
    BlockHeader bi = _bi ? _bi : _bc.info( _block );
#if ETH_PARANOIA
    if ( !bi )
        while ( 1 ) {
            try {
                auto b = _bc.block( _block );
                bi.populate( b );
                break;
            } catch ( Exception const& _e ) {
                // TODO: Slightly nicer handling? :-)
                BOOST_LOG( m_loggerError )
                    << "ERROR: Corrupt block-chain! Delete your block-chain DB and restart.";
                BOOST_LOG( m_loggerError ) << diagnostic_information( _e );
            } catch ( std::exception const& _e ) {
                // TODO: Slightly nicer handling? :-)
                BOOST_LOG( m_loggerError )
                    << "ERROR: Corrupt block-chain! Delete your block-chain DB and restart.";
                BOOST_LOG( m_loggerError ) << _e.what();
            }
        }
#endif
    if ( bi == m_currentBlock ) {
        // We mined the last block.
        // Our state is good - we just need to move on to next.
        m_previousBlock = m_currentBlock;
        // see at end        resetCurrent();
        ret = true;
    } else if ( bi == m_previousBlock ) {
        // No change since last sync.
        // Carry on as we were.
    } else {
        // New blocks available, or we've switched to a different branch. All change.
        // Find most recent state dump and replay what's left.
        // (Most recent state dump might end up being genesis.)

        m_previousBlock = bi;
        resetCurrent();
        ret = true;
    }
#if ALLOW_REBUILD
    else {
        // New blocks available, or we've switched to a different branch. All change.
        // Find most recent state dump and replay what's left.
        // (Most recent state dump might end up being genesis.)

        std::vector< h256 > chain;
        while ( bi.number() != 0 && m_db.lookup( bi.stateRoot() ).empty() )  // while we don't have
                                                                             // the state root of
                                                                             // the latest block...
        {
            chain.push_back( bi.hash() );                 // push back for later replay.
            bi.populate( _bc.block( bi.parentHash() ) );  // move to parent.
        }

        m_previousBlock = bi;
        resetCurrent();

        // Iterate through in reverse, playing back each of the blocks.
        try {
            for ( auto it = chain.rbegin(); it != chain.rend(); ++it ) {
                auto b = _bc.block( *it );
                enact( &b, _bc, _ir );
                cleanup( true );
            }
        } catch ( ... ) {
            // TODO: Slightly nicer handling? :-)
            BOOST_LOG( m_loggerError )
                << "ERROR: Corrupt block-chain! Delete your block-chain DB and restart.";
            BOOST_LOG( m_loggerError ) << boost::current_exception_diagnostic_information();
            exit( 1 );
        }

        resetCurrent();
        ret = true;
    }
#endif
    resetCurrent( m_currentBlock.timestamp() );
    return ret;
}


// Note - this function is only used in tests
pair< TransactionReceipts, bool > Block::sync(
    BlockChain const& _bc, TransactionQueue& _tq, GasPricer const& _gp, unsigned msTimeout ) {
    MICROPROFILE_SCOPEI( "Block", "sync tq", MP_BURLYWOOD );

    if ( isSealed() )
        BOOST_THROW_EXCEPTION( InvalidOperationOnSealedBlock() );

    noteChain( _bc );

    // TRANSACTIONS
    pair< TransactionReceipts, bool > ret;

    Transactions transactions = _tq.topTransactions( c_maxSyncTransactions, m_transactionSet );
    ret.second = ( transactions.size() == c_maxSyncTransactions );  // say there's more to the
                                                                    // caller if we hit the limit

#ifndef FAIR
    for ( Transaction& transaction : transactions ) {
        transaction.checkOutExternalGas( _bc.chainParams(), _bc.info().timestamp(), _bc.number() );
    }
#endif

    assert( _bc.currentHash() == m_currentBlock.parentHash() );
    auto deadline = chrono::steady_clock::now() + chrono::milliseconds( msTimeout );

    for ( int goodTxs = max( 0, ( int ) transactions.size() - 1 );
          goodTxs < ( int ) transactions.size(); ) {
        goodTxs = 0;
        for ( auto const& t : transactions )
            if ( !m_transactionSet.count( t.sha3() ) ) {
                try {
                    if ( t.gasPrice() >= _gp.ask( *this ) ) {
                        execute( _bc.lastBlockHashes(), t, Permanence::Uncommitted );
#ifdef FAIR
                        ret.first = m_receipts;
#else
                        ret.first.push_back( m_receipts.back() );
#endif
                        ++goodTxs;
                    } else if ( t.gasPrice() < _gp.ask( *this ) * 9 / 10 ) {
                        BOOST_LOG( m_loggerDebug )
                            << t.sha3() << " Dropping El Cheapo transaction (<90% of ask price)";
                        _tq.drop( t.sha3() );
                    }
                } catch ( InvalidNonce const& in ) {
                    bigint const& req = *boost::get_error_info< errinfo_required >( in );
                    bigint const& got = *boost::get_error_info< errinfo_got >( in );

                    if ( req > got ) {
                        // too old
                        BOOST_LOG( m_loggerDebug )
                            << t.sha3() << " Dropping old transaction (nonce too low)";
                        _tq.drop( t.sha3() );
                    } else if ( got > req + _tq.waiting( t.sender() ) ) {
                        // too new
                        BOOST_LOG( m_loggerDebug )
                            << t.sha3() << " Dropping new transaction (too many nonces ahead)";
                        _tq.drop( t.sha3() );
                    } else
                        _tq.setFuture( t.sha3() );
                } catch ( BlockGasLimitReached const& e ) {
                    bigint const& got = *boost::get_error_info< errinfo_got >( e );
                    if ( got > m_currentBlock.gasLimit() ) {
                        BOOST_LOG( m_loggerDebug )
                            << t.sha3()
                            << " Dropping over-gassy transaction (gas > block's gas limit)";
                        BOOST_LOG( m_loggerDebug )
                            << "got: " << got << " required: " << m_currentBlock.gasLimit();
                        _tq.drop( t.sha3() );
                    } else {
                        BOOST_LOG( m_loggerDebug )
                            << t.sha3()
                            << " Temporarily no gas left in current block (txs gas > "
                               "block's gas limit)";
                        // Temporarily no gas left in current block.
                        // OPTIMISE: could note this and then we don't evaluate until a block that
                        // does have the gas left. for now, just leave alone.
                    }
                } catch ( Exception const& _e ) {
                    // Something else went wrong - drop it.
                    BOOST_LOG( m_loggerDebug )
                        << t.sha3()
                        << " Dropping invalid transaction: " << diagnostic_information( _e );
                    _tq.drop( t.sha3() );
                } catch ( std::exception const& ) {
                    // Something else went wrong - drop it.
                    _tq.drop( t.sha3() );
                    BOOST_LOG( m_loggerWarning )
                        << t.sha3() << "Transaction caused low-level exception :(";
                }
            }
        if ( chrono::steady_clock::now() > deadline ) {
            ret.second =
                true;  // say there's more to the caller if we ended up crossing the deadline.
            break;
        }
    }


    return ret;
}

inline void Block::doPartialCatchupTestIfRequested( unsigned i ) {
    static const char* FAIL_AT_TX_NUM = std::getenv( "TEST_FAIL_AT_TX_NUM" );
    static int64_t transactionCount = 0;

    if ( FAIL_AT_TX_NUM ) {
        if ( transactionCount == std::stoi( FAIL_AT_TX_NUM ) ) {
            // fail hard for test
            cerror << "Test: crashing skaled on purpose after processing  " << i
                   << " transactions in block";
            exit( -1 );
        }

        transactionCount++;
    }
}

void Block::sanityCheckPartialTransactionReceipts( std::optional< BlockNumber > blockNumber ) {
    // do a simple sanity check from time to time
    static uint64_t sanityCheckCounter = 0;
    if ( sanityCheckCounter++ % 10000 == 0 ) {
        if ( blockNumber.has_value() ) {
            LDB_CHECK( m_state.safePartialTransactionReceipts( blockNumber.value() ).empty() );
        } else {
            LDB_CHECK( m_state.safeLegacyPartialTransactionReceipts().empty() );
        }
    }
}

tuple< TransactionReceipts, unsigned, bool > Block::syncEveryone( BlockChain const& _bc,
    const Transactions& _transactions, uint64_t _timestamp, u256 _gasPrice, u256 _baseFeePerGas,
    OnTransactionConsumed const& _onTransactionConsumed, u256 _prevRandao ) {
    if ( isSealed() )
        BOOST_THROW_EXCEPTION( InvalidOperationOnSealedBlock() );

    noteChain( _bc );
    assert( _bc.currentHash() == m_currentBlock.parentHash() );

    SyncContext context;
    context.singleCommitEnabled = SingleStateCommitPerBlockPatch::isEnabledInWorkingBlock();

    if ( context.singleCommitEnabled && isCurrentBlockCommitted() ) {
        auto recovered =
            recoverFromReceipts( _transactions, _timestamp, _baseFeePerGas, _prevRandao );
        bool needsQueueReadyNotification = false;
        Transactions queueCleanupTransactions;
        u256 cumulativeGas = 0;
        for ( unsigned i = 0; i < recovered.first.size() && i < _transactions.size(); ++i ) {
            if ( receiptAdvancedGas( recovered.first[i], cumulativeGas ) ) {
#ifdef BITE
                // recoverFromReceipts() restored the post-block BITE queue from the progress log,
                // so consumed CTXs are already absent. Only clean regular queues here.
                if ( !_transactions[i].isCTX() )
#endif
                    queueCleanupTransactions.push_back( _transactions[i] );
            }
            cumulativeGas = recovered.first[i].cumulativeGasUsed();
        }
        needsQueueReadyNotification =
            notifyConsumedTransactions( _onTransactionConsumed, queueCleanupTransactions );
#ifdef BITE
        m_pendingCtxs = g_skaleHost->pendingBITE2Transactions();
#endif
        return make_tuple( recovered.first, recovered.second, needsQueueReadyNotification );
    }

    prepareStateForSync( _timestamp, _baseFeePerGas, _prevRandao, context );
    executeTransactions( _bc, _transactions, _gasPrice, context, _onTransactionConsumed );

    if ( !context.singleCommitEnabled || !isCurrentBlockCommitted() ) {
        saveStateChanges( _bc, _transactions, context );
    }

    return make_tuple( context.receipts, context.receipts.size() - context.badCount,
        context.needsQueueReadyNotification );
}

std::pair< TransactionReceipts, unsigned > Block::recoverFromReceipts(
    const Transactions& _transactions, uint64_t, u256 _baseFeePerGas, u256 _prevRandao ) {
    if ( !SingleStateCommitPerBlockPatch::isEnabledInWorkingBlock() ) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error( "recoverFromReceipts called outside single commit mode" ) );
    }

    auto progressLog = m_state.getProgressLog();
    if ( !progressLog ) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error( "Progress log is not available during recovery" ) );
    }

    auto savedData = progressLog->loadProgressData();
    if ( !savedData || savedData->receipts.size() != _transactions.size() ) {
        BOOST_THROW_EXCEPTION( std::runtime_error(
            "Saved receipts missing or count mismatch during recovery for block " +
            std::to_string( m_currentBlock.number() ) ) );
    }

    BOOST_LOG( m_loggerWarning ) << "Recovering block " << m_currentBlock.number()
                                 << " from saved receipts with timestamp " << savedData->timestamp;

    resetCurrent( savedData->timestamp );
    if ( _baseFeePerGas != 0 )
        m_currentBlock.setBaseFeePerGas( _baseFeePerGas );
    // Nonzero only post-Paris (gated in SkaleHost::createBlock); recovery must rebuild the
    // exact header the pre-crash execution was producing.
    applyPrevRandao( _prevRandao );

    for ( const auto& tx : _transactions ) {
        m_transactions.push_back( tx );
        m_transactionSet.insert( tx.sha3() );
    }
    m_receipts = std::move( savedData->receipts );
#ifdef BITE
    // set CTXs from previous block to BITE2 queue
    g_skaleHost->setBITE2QueueOnInit( std::move( savedData->ctxsCreatedInBlock ) );
    m_pendingCtxs = g_skaleHost->pendingBITE2Transactions();
#endif

    unsigned badCount = 0;
    u256 cumulativeGas = 0;
    for ( const auto& receipt : m_receipts ) {
        if ( receipt.cumulativeGasUsed() == cumulativeGas ) {
            badCount++;
        }
        cumulativeGas = receipt.cumulativeGasUsed();
    }

    return std::make_pair( m_receipts, m_receipts.size() - badCount );
}

void Block::applyPrevRandao( u256 _prevRandao ) {
    if ( _prevRandao != 0 && m_currentBlock.sealFieldCount() == 2 )
        m_currentBlock.setPrevRandao( h256( _prevRandao ) );
}

void Block::prepareStateForSync(
    uint64_t _timestamp, u256 _baseFeePerGas, u256 _prevRandao, SyncContext& _context ) {
    resetCurrent( _timestamp );
    if ( _baseFeePerGas != 0 )
        m_currentBlock.setBaseFeePerGas( _baseFeePerGas );
    // Nonzero only post-Paris (gated in SkaleHost::createBlock). resetCurrent() wrote the
    // Paris zero seal fields via Ethash::populateFromParent; this overrides the value the
    // same way baseFee is set.
    applyPrevRandao( _prevRandao );
    m_state = m_state.createStateCopyAndClearCaches();

#ifndef FAIR
    if ( _context.singleCommitEnabled ) {
        bool isCacheEnabled = RevertableFSPatch::isEnabledWhen( m_previousBlock.timestamp() );
        m_state.resetOverlayFS( isCacheEnabled );
    }
#endif

    if ( _context.singleCommitEnabled ) {
        // Recovery from saved receipts is handled in recoverFromReceipts() before reaching here.
        m_receipts.clear();
    } else {
        m_receipts = m_state.safePartialTransactionReceipts( info().number() );
        if ( !m_receipts.empty() ) {
            cwarn << "Recovering from a previous crash while processing TRANSACTION:"
                  << m_receipts.size() << ":BLOCK:" << info().number();
            u256 cumulativeGas = 0;
            for ( auto const& receipt : m_receipts ) {
                if ( receipt.cumulativeGasUsed() == cumulativeGas ) {
                    _context.badCount++;
                }
                cumulativeGas = receipt.cumulativeGasUsed();
            }
        }
    }
    _context.receipts = m_receipts;
}

void Block::executeTransactions( BlockChain const& _bc, const Transactions& _transactions,
    u256 _gasPrice, SyncContext& _context, OnTransactionConsumed const& _onTransactionConsumed ) {
    const Permanence permanence =
        _context.singleCommitEnabled ? Permanence::BlockCommitted : Permanence::Committed;

    TransactionReceipts savedReceipts = m_receipts;

#ifdef BITE
    m_ctxHashesLists.resize( _transactions.size() );
#endif

    for ( unsigned i = 0; i < _transactions.size(); ++i ) {
        Transaction const& tr = _transactions[i];
        try {
            if ( !_context.singleCommitEnabled && i < savedReceipts.size() ) {
                // this transaction has already been executed and we have a
                // receipt for it. We do not need to execute it again. Only applicable for legacy
                // multiple commits mode
                m_transactions.push_back( tr );
                m_transactionSet.insert( tr.sha3() );
                u256 previousCumulativeGas = i == 0 ? 0 : savedReceipts[i - 1].cumulativeGasUsed();
                if ( receiptAdvancedGas( savedReceipts[i], previousCumulativeGas ) )
                    _context.queueCleanupTransactions.push_back( tr );
                continue;
            }

            // Tell skaled to fail in a middle of blog processing
            // this is used in partial catchup tests
            doPartialCatchupTestIfRequested( i );

            auto receipt = executeSingleTransaction( _bc, tr, i, _gasPrice, permanence, _context );
            if ( receipt ) {
                _context.receipts.push_back( *receipt );
            }
        } catch ( Exception& ex ) {
            ex << errinfo_transactionIndex( i );
            // just ignore invalid transactions
            BOOST_LOG( m_loggerError ) << "FAILED transaction after consensus! " << ex.what();
        }
    }

    _context.needsQueueReadyNotification =
        notifyConsumedTransactions( _onTransactionConsumed, _context.queueCleanupTransactions ) ||
        _context.needsQueueReadyNotification;
#ifdef BITE
    // finalize BITE2 queue after executing all txns from current block
    m_pendingCtxs = g_skaleHost->pendingBITE2Transactions();
#endif
}

std::optional< TransactionReceipt > Block::executeSingleTransaction( BlockChain const& _bc,
    Transaction const& _tx, unsigned _txIndex, u256 _gasPrice, skale::Permanence _permanence,
    SyncContext& _context ) {
    if ( !_tx.isInvalid() &&
#ifndef FAIR
         !_tx.hasExternalGas() &&
#endif
         _tx.gasPrice() < _gasPrice ) {
        BOOST_LOG( m_loggerDebug )
            << "Transaction " << _tx.sha3() << " WouldNotBeInBlock: gasPrice " << _tx.gasPrice()
            << " < " << _gasPrice;

        if ( SkipInvalidTransactionsPatch::isEnabledInWorkingBlock() ) {
            m_transactions.push_back( _tx );
            m_transactionSet.insert( _tx.sha3() );

            TransactionReceipt const nullReceipt =
                info().number() >= sealEngine()->chainParams().getByzantiumForkBlock() ?
                    TransactionReceipt( 0, info().gasUsed(), LogEntries() ) :
                    TransactionReceipt( EmptyTrie, info().gasUsed(), LogEntries() );

            m_receipts.push_back( nullReceipt );
            if ( !_context.singleCommitEnabled ) {
                // we need to record the receipt in case we crash
                m_state.safeSetAndCommitPartialTransactionReceipt(
                    nullReceipt.rlp(), info().number(), _txIndex );
            }
            ++_context.badCount;
            if ( !shouldKeepRejectedTransactionQueued( _tx, m_state ) )
                _context.queueCleanupTransactions.push_back( _tx );
            return nullReceipt;
        }
        if ( !shouldKeepRejectedTransactionQueued( _tx, m_state ) )
            _context.queueCleanupTransactions.push_back( _tx );
        return std::nullopt;
    }

    ExecutionResult res = execute( _bc.lastBlockHashes(), _tx, _permanence, OnOpFunc(), _txIndex );

#ifdef BITE
    if ( res.excepted != TransactionException::None ) {
        // clear all CTXs that were added by last tx
        // because it was reverted
        g_skaleHost->clearTempBITE2Transactions();
    } else {
        // get list of CTX hashes created by current txn
        m_ctxHashesLists[_txIndex] = g_skaleHost->getBITE2HashesForCurrentTxn();
        // commit CTXs from temporary to permanent
        g_skaleHost->commitTempBITE2Transactions();
    }
#endif

    if ( transactionNeedsQueueCleanup( _tx, res, m_state ) )
        _context.queueCleanupTransactions.push_back( _tx );

    if ( !_context.singleCommitEnabled && !m_receipts.empty() &&
         !ClearPartialReceiptsPatch::isEnabledWhen( m_previousBlock.timestamp() ) ) {
        _context.receiptsOfCommitted.push_back( m_receipts.back() );
    }

    if ( !SkipInvalidTransactionsPatch::isEnabledInWorkingBlock() ||
         res.excepted != TransactionException::WouldNotBeInBlock ) {
        if ( res.excepted == TransactionException::WouldNotBeInBlock )
            ++_context.badCount;
        return m_receipts.back();
    }

    return std::nullopt;
}

bool Block::isCurrentBlockCommitted() {
    // This check only works for single commit mode
    auto progressLog = m_state.getProgressLog();
    if ( !progressLog ) {
        return false;
    }

    if ( progressLog->isBlockCommitCompleted( m_currentBlock.number() ) ) {
        BOOST_LOG( m_loggerDebug ) << "Skipping state commit - block " << m_currentBlock.number()
                                   << " already completed according to progress log";
        return true;
    }
    return false;
}

void Block::saveStateChanges(
    BlockChain const& _bc, const Transactions& _transactions, const SyncContext& _context ) {
    auto progressLog = m_state.getProgressLog();
    if ( progressLog && _context.singleCommitEnabled ) {
        progressLog->markBlockCommitStarted( m_currentBlock.number() );
    }

#ifdef FAIR
    auto lastRewardedBlockNumber = m_state.getLastRewardedBlockNumber();
    if ( lastRewardedBlockNumber < m_currentBlock.number() ) {
        auto blockTimestamp = m_previousBlock.timestamp();
        auto reward = _bc.sealEngine()->blockReward( blockTimestamp, m_currentBlock.number() );
        rewardAllForNonDefaultBlock( _bc.chainParams().getStakingContractAddress(), reward );
    }
    m_state.safeSetLastRewardedBlockNumber( m_currentBlock.number() );
#endif

    if ( !_transactions.empty() ) {
        m_state.safeSetLastExecutedTransactionHash( _transactions.back().sha3() );
    }

    const bool stateWritable = m_state.connected() && !m_state.isReadOnlySnapBasedState();
    if ( !stateWritable )
        return;

    runCommit( _bc, _context );

    LDB_CHECK( _context.receipts.size() >= _context.badCount );

    createBlockSnapshot();

    if ( progressLog && _context.singleCommitEnabled ) {
#ifdef BITE
        CHECK_EXPRESSION( m_pendingCtxs );
#endif
        progressLog->markBlockCommitCompleted(
            m_currentBlock.number(), _context.receipts, m_currentBlock.timestamp()
#ifdef BITE
                                                            ,
            *m_pendingCtxs
#endif
        );
    }

    if ( !_context.singleCommitEnabled ) {
        clearPartialReceipts();
    }
    // for backward compatibility still have to run this check for singleBlockCommit
    handleLegacyPartialReceipts( _bc, _context );
}

void Block::runCommit( BlockChain const& _bc, const SyncContext& _context ) {
    bool removeEmptyAccounts = m_currentBlock.number() >= _bc.chainParams().getEIP158ForkBlock();
    m_state.commit( removeEmptyAccounts ? dev::eth::CommitBehaviour::RemoveEmptyAccounts :
                                          dev::eth::CommitBehaviour::KeepEmptyAccounts,
        m_currentBlock.number() );

#ifndef FAIR
    if ( _context.singleCommitEnabled ) {
        m_state.fs()->commit();
    }
#endif
}

void Block::createBlockSnapshot() {
    m_state.createStateCopyAndClearCaches();
    LDB_CHECK( m_state.getOriginalDb() );
    m_state.getOriginalDb()->createBlockSnap( info().number() );

#ifdef HISTORIC_STATE
    m_state.mutableHistoricState().saveRootForBlockNumber( m_currentBlock.number() );
#endif
}

void Block::clearPartialReceipts() {
    // we got to the end of the block so we do not need partial transaction receipts anymore
    m_state.safeRemoveAllPartialTransactionReceipts();

    static uint64_t sanityCheckCounter = 0;
    if ( sanityCheckCounter++ % 10000 == 0 ) {
        sanityCheckPartialTransactionReceipts( info().number() );
    }
}

void Block::handleLegacyPartialReceipts( BlockChain const& _bc, const SyncContext& _context ) {
    bool weAreAtTheTimeStampBoundary = false;
    auto latestCommittedBlockTimeStamp = m_previousBlock.timestamp();

    if ( m_previousBlock.number() > 0 ) {
        auto beforeLatestCommittedBlockTimeStamp =
            _bc.info( m_previousBlock.parentHash() ).timestamp();
        weAreAtTheTimeStampBoundary =
            ClearPartialReceiptsPatch::isEnabledWhen( latestCommittedBlockTimeStamp ) &&
            !ClearPartialReceiptsPatch::isEnabledWhen( beforeLatestCommittedBlockTimeStamp );
    }

    // we need to specially handle the boundary case
    if ( weAreAtTheTimeStampBoundary ) {
        BOOST_LOG( m_loggerTrace ) << "Removing legacy partial receipts";
        m_state.safeRemoveLegacyPartialTransactionReceipts();
    }

    if ( !ClearPartialReceiptsPatch::isEnabledWhen( latestCommittedBlockTimeStamp ) ) {
        if ( !_context.receiptsOfCommitted.empty() ) {
            BOOST_LOG( m_loggerTrace ) << "Saving partial transaction receipts. Size: "
                                       << _context.receiptsOfCommitted.size();
            m_state.safeCommitLegacyPartialTransactionReceipts( _context.receiptsOfCommitted );
        }
    }
}

u256 Block::enactOn( VerifiedBlockRef const& _block, BlockChain const& _bc ) {
    MICROPROFILE_SCOPEI( "Block", "enactOn", MP_INDIANRED );

    noteChain( _bc );

#if ETH_TIMED_ENACTMENTS
    Timer t;
    double populateVerify;
    double populateGrand;
    double syncReset;
    double enactment;
#endif

    // Check family:
    BlockHeader biParent = _bc.info( _block.info.parentHash() );
    _block.info.verify( CheckNothingNew /*CheckParent*/, biParent );

#if ETH_TIMED_ENACTMENTS
    populateVerify = t.elapsed();
    t.restart();
#endif

    BlockHeader biGrandParent;
    if ( biParent.number() )
        biGrandParent = _bc.info( biParent.parentHash() );

#if ETH_TIMED_ENACTMENTS
    populateGrand = t.elapsed();
    t.restart();
#endif

    sync( _bc, _block.info.parentHash(), BlockHeader() );
    resetCurrent();

    m_state = m_state.createStateCopyAndClearCaches();

#if ETH_TIMED_ENACTMENTS
    syncReset = t.elapsed();
    t.restart();
#endif

    m_previousBlock = biParent;
    auto ret = enact( _block, _bc );

#if ETH_TIMED_ENACTMENTS
    enactment = t.elapsed();
    if ( populateVerify + populateGrand + syncReset + enactment > 0.5 )
        BOOST_LOG( m_loggerDebug )
            << "popVer/popGrand/syncReset/enactment = " << populateVerify << " / " << populateGrand
            << " / " << syncReset << " / " << enactment;
#endif
    return ret;
}

u256 Block::enact( VerifiedBlockRef const& _block, BlockChain const& _bc ) {
    noteChain( _bc );

    DEV_TIMED_FUNCTION_ABOVE( 500 );

    // m_currentBlock is assumed to be prepopulated and reset.
    assert( m_previousBlock.hash() == _block.info.parentHash() );
    assert( m_currentBlock.parentHash() == _block.info.parentHash() );

    if ( m_currentBlock.parentHash() != m_previousBlock.hash() )
        // Internal client error.
        BOOST_THROW_EXCEPTION( InvalidParentHash() );

    // Populate m_currentBlock with the correct values.
    m_currentBlock.noteDirty();
    m_currentBlock = _block.info;

    RLP rlp( _block.block );

    vector< bytes > receipts;

    // All ok with the block generally. Play back the transactions now...
    unsigned i = 0;
    DEV_TIMED_ABOVE( "txExec", 500 ) for ( Transaction const& tr : _block.transactions ) {
        try {
#ifndef FAIR
            const_cast< Transaction& >( tr ).checkOutExternalGas(
                _bc.chainParams(), _bc.info().timestamp(), _bc.number() );
#endif
            execute( _bc.lastBlockHashes(), tr, skale::Permanence::Committed, OnOpFunc(), i );
        } catch ( Exception& ex ) {
            ex << errinfo_transactionIndex( i );
            throw;
        }

        // The EIP-1559
        // transaction format is accepted before Berlin, but the typed-receipt
        // encoding must only change at the coordinated Berlin fork so blocks
        // produced before it keep their original receiptsRoot.
        // The parent block timestamp is used (not the global committed-block
        // timestamp) so the encoding is deterministic per block, including when
        // a block is re-enacted out of order.
        if ( BerlinForkPatch::isEnabledWhen( previousInfo().timestamp() ) &&
             m_receipts.back().txType() > 0 ) {
            receipts.push_back( m_receipts.back().typedRlp() );
        } else {
            RLPStream receiptRLP;
            m_receipts.back().streamRLP( receiptRLP );
            receipts.push_back( receiptRLP.out() );
        }
        ++i;
    }

    h256 receiptsRoot;
    DEV_TIMED_ABOVE( ".receiptsRoot()", 500 ) receiptsRoot = orderedTrieRoot( receipts );

    if ( receiptsRoot != m_currentBlock.receiptsRoot() ) {
        InvalidReceiptsStateRoot ex;
        ex << Hash256RequirementError( m_currentBlock.receiptsRoot(), receiptsRoot );
        ex << errinfo_receipts( receipts );
        for ( auto const& receipt : m_receipts ) {
            if ( !receipt.hasStatusCode() ) {
                BOOST_LOG( m_loggerWarning ) << "Skale does not support state root in receipt";
                break;
            }
        }
        BOOST_THROW_EXCEPTION( ex );
    }

    if ( m_currentBlock.logBloom() != logBloom() ) {
        InvalidLogBloom ex;
        ex << LogBloomRequirementError( m_currentBlock.logBloom(), logBloom() );
        ex << errinfo_receipts( receipts );
        BOOST_THROW_EXCEPTION( ex );
    }

    // Initialise total difficulty calculation.
    u256 tdIncrease = m_currentBlock.difficulty();

    // Check uncles & apply their rewards to state.
    if ( ParisForkPatch::isEnabledWhen( previousInfo().timestamp() ) ) {
        if ( rlp[2].itemCount() > 0 ) {
            TooManyUncles ex;
            ex << errinfo_max( 0 );
            ex << errinfo_got( rlp[2].itemCount() );
            BOOST_THROW_EXCEPTION( ex );
        }
    } else if ( rlp[2].itemCount() > 2 ) {
        TooManyUncles ex;
        ex << errinfo_max( 2 );
        ex << errinfo_got( rlp[2].itemCount() );
        BOOST_THROW_EXCEPTION( ex );
    }

    vector< BlockHeader > rewarded;
    h256Hash excluded;
    DEV_TIMED_ABOVE( "allKin", 500 )
    excluded = _bc.allKinFrom( m_currentBlock.parentHash(), 6 );
    excluded.insert( m_currentBlock.hash() );

    unsigned ii = 0;
    DEV_TIMED_ABOVE( "uncleCheck", 500 )
    for ( auto const& i : rlp[2] ) {
        try {
            auto h = sha3( i.data() );
            if ( excluded.count( h ) ) {
                UncleInChain ex;
                ex << errinfo_comment( "Uncle in block already mentioned" );
                ex << errinfo_unclesExcluded( excluded );
                ex << errinfo_hash256( sha3( i.data() ) );
                BOOST_THROW_EXCEPTION( ex );
            }
            excluded.insert( h );

            // CheckNothing since it's a VerifiedBlock.
            BlockHeader uncle( i.data(), HeaderData, h );

            BlockHeader uncleParent;
            if ( !_bc.isKnown( uncle.parentHash() ) )
                BOOST_THROW_EXCEPTION( UnknownParent() << errinfo_hash256( uncle.parentHash() ) );
            uncleParent = BlockHeader( _bc.block( uncle.parentHash() ) );

            // m_currentBlock.number() - uncle.number()		m_cB.n - uP.n()
            // 1											2
            // 2
            // 3
            // 4
            // 5
            // 6											7
            //												(8 Invalid)
            bigint depth = ( bigint ) m_currentBlock.number() - ( bigint ) uncle.number();
            if ( depth > 6 ) {
                UncleTooOld ex;
                ex << errinfo_uncleNumber( uncle.number() );
                ex << errinfo_currentNumber( m_currentBlock.number() );
                BOOST_THROW_EXCEPTION( ex );
            } else if ( depth < 1 ) {
                UncleIsBrother ex;
                ex << errinfo_uncleNumber( uncle.number() );
                ex << errinfo_currentNumber( m_currentBlock.number() );
                BOOST_THROW_EXCEPTION( ex );
            }
            // cB
            // cB.p^1	    1 depth, valid uncle
            // cB.p^2	---/  2
            // cB.p^3	-----/  3
            // cB.p^4	-------/  4
            // cB.p^5	---------/  5
            // cB.p^6	-----------/  6
            // cB.p^7	-------------/
            // cB.p^8
            auto expectedUncleParent = _bc.details( m_currentBlock.parentHash() ).parent;
            for ( unsigned i = 1; i < depth;
                  expectedUncleParent = _bc.details( expectedUncleParent ).parent, ++i ) {
            }
            if ( expectedUncleParent != uncleParent.hash() ) {
                UncleParentNotInChain ex;
                ex << errinfo_uncleNumber( uncle.number() );
                ex << errinfo_currentNumber( m_currentBlock.number() );
                BOOST_THROW_EXCEPTION( ex );
            }
            uncle.verify( CheckNothingNew /*CheckParent*/, uncleParent );

            rewarded.push_back( uncle );
            ++ii;
        } catch ( Exception& ex ) {
            ex << errinfo_uncleIndex( ii );
            throw;
        }
    }

    assert( _bc.sealEngine() );
    DEV_TIMED_ABOVE( "applyRewards", 500 )

#ifdef FAIR
    rewardAllForNonDefaultBlock( _bc.chainParams().getStakingContractAddress(),
        _bc.sealEngine()->blockReward( m_currentBlock.timestamp(), m_currentBlock.number() ) );
#else
    applyRewards( rewarded,
        _bc.sealEngine()->blockReward( previousInfo().timestamp(), m_currentBlock.number() ) );
#endif

    if ( m_currentBlock.gasUsed() != gasUsed() ) {
        // Do not commit changes of state
        BOOST_THROW_EXCEPTION( InvalidGasUsed() << RequirementError(
                                   bigint( m_currentBlock.gasUsed() ), bigint( gasUsed() ) ) );
    }

    // Commit all cached state changes to the state trie.
    bool removeEmptyAccounts =
        m_currentBlock.number() >= _bc.chainParams().getEIP158ForkBlock();  // TODO: use EVMSchedule
    DEV_TIMED_ABOVE( "commit", 500 )
    m_state.commit( removeEmptyAccounts ? dev::eth::CommitBehaviour::RemoveEmptyAccounts :
                                          dev::eth::CommitBehaviour::KeepEmptyAccounts,
        m_currentBlock.number() );

    return tdIncrease;
}


#ifdef HISTORIC_STATE
ExecutionResult Block::executeHistoricCall( LastBlockHashesFace const& _lh, Transaction const& _t,
    std::shared_ptr< AlethStandardTrace > _tracer, uint64_t _transactionIndex ) {
    try {
        auto onOp = OnOpFunc();

        if ( _tracer ) {
            onOp = _tracer->functionToExecuteOnEachOperation();
        }

        if ( isSealed() )
            BOOST_THROW_EXCEPTION( InvalidOperationOnSealedBlock() );

        uncommitToSeal();

        STATE_CHECK( _transactionIndex <= m_receipts.size() )

        u256 const gasUsed =
            _transactionIndex ? receipt( _transactionIndex - 1 ).cumulativeGasUsed() : 0;

        EnvInfo const envInfo( info(), _lh, this->previousInfo().timestamp(), gasUsed,
            m_sealEngine->chainParams().getChainId() );

        if ( _tracer ) {
            try {
                HistoricState stateBefore( m_state.mutableHistoricState() );

                auto resultReceipt = m_state.mutableHistoricState().execute(
                    envInfo, m_sealEngine->chainParams(), _t, skale::Permanence::Uncommitted, onOp
#ifdef BITE
                    ,
                    _transactionIndex
#endif
                );

                _tracer->finalizeAndPrintTrace(
                    resultReceipt.first, stateBefore, m_state.mutableHistoricState() );
                // for tracing the entire block is traced therefore, we save transaction receipt
                // as it is used for execution of the next transaction
                m_receipts.push_back( resultReceipt.second );
                return resultReceipt.first;
            } catch ( std::exception& e ) {
                throw dev::eth::VMTracingError( "Exception doing trace for transaction index:" +
                                                std::to_string( _transactionIndex ) + ":" +
                                                e.what() );
            }
        } else {
            auto resultReceipt = m_state.mutableHistoricState().execute(
                envInfo, m_sealEngine->chainParams(), _t, skale::Permanence::Reverted, onOp
#ifdef BITE
                ,
                _transactionIndex
#endif
            );
            return resultReceipt.first;
        }
    } catch ( std::exception& e ) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error( "Could not execute historic call for transactionIndex:" +
                                to_string( _transactionIndex ) + ":" + e.what() ) );
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error( "Could not execute historic call for transactionIndex:" +
                                to_string( _transactionIndex ) + ": unknown error" ) );
    }
}
#endif


ExecutionResult Block::execute( LastBlockHashesFace const& _lh, Transaction const& _t,
    Permanence _p, OnOpFunc const& _onOp, int64_t _transactionIndex ) {
    MICROPROFILE_SCOPEI( "Block", "execute transaction", MP_CORNFLOWERBLUE );
    if ( isSealed() )
        BOOST_THROW_EXCEPTION( InvalidOperationOnSealedBlock() );

    // Uncommitting is a non-trivial operation - only do it once we've verified as much of the
    // transaction as possible.
    uncommitToSeal();


    EnvInfo envInfo = EnvInfo( info(), _lh, previousInfo().timestamp(), gasUsed(),
        m_sealEngine->chainParams().getChainId() );

    // "bad" transaction receipt for failed transactions
    TransactionReceipt const null_receipt =
        envInfo.number() >= sealEngine()->chainParams().getByzantiumForkBlock() ?
            TransactionReceipt( 0, envInfo.gasUsed(), LogEntries() ) :
            TransactionReceipt( EmptyTrie, envInfo.gasUsed(), LogEntries() );

    std::pair< ExecutionResult, TransactionReceipt > resultReceipt{ ExecutionResult(),
        null_receipt };

    try {
        if ( _t.isInvalid() )
            throw -1;  // will catch below

        resultReceipt = m_state.execute(
            envInfo, m_sealEngine->chainParams(), _t, _p, _onOp, _transactionIndex );

        // use fake receipt created above if execution throws!!
    } catch ( const TransactionException& ex ) {
        // should not happen as exception in execute() means that tx should not be in block
        BOOST_LOG( m_loggerError ) << DETAILED_ERROR;
        assert( false );
    } catch ( const std::exception& ex ) {
        BOOST_LOG( m_loggerDebug ) << "Transaction with index " << _transactionIndex
                                   << " WouldNotBeInBlock: " << ex.what();
        if ( _p != Permanence::Reverted )  // if it is not call
            _p = Permanence::CommittedWithoutState;
        resultReceipt.first.excepted = TransactionException::WouldNotBeInBlock;
    } catch ( ... ) {
        BOOST_LOG( m_loggerDebug )
            << "Transaction with index " << _transactionIndex << " WouldNotBeInBlock: ...";
        if ( _p != Permanence::Reverted )  // if it is not call
            _p = Permanence::CommittedWithoutState;
        resultReceipt.first.excepted = TransactionException::WouldNotBeInBlock;
    }  // catch

    if ( _p != Permanence::Reverted ) {
        // Add to the user-originated transactions that we've executed.
        if ( !SkipInvalidTransactionsPatch::isEnabledWhen( previousInfo().timestamp() ) ||
             resultReceipt.first.excepted != TransactionException::WouldNotBeInBlock ) {
            m_transactions.push_back( _t );
            m_receipts.push_back( resultReceipt.second );
            m_transactionSet.insert( _t.sha3() );
        }
    }


    // if we are doing real block processing with commit, we currently clear cache
    // on each transaction. This can be done safely because state changes are committed to
    // disk. In other cases we do not clear cache. This is specifically handy for tests
    // because we do not commit to disk in some of the tests.
    if ( _p == Permanence::Committed ) {
        m_state = m_state.createStateCopyAndClearCaches();
    }

    return resultReceipt.first;
}

#ifdef FAIR
void Block::rewardAllForNonDefaultBlock(
    const dev::Address& _stakingContractAddress, u256 const& _blockReward ) {
    // if staking contract is set to ZeroAddress, full reward goes to block author
    auto evmSchedule =
        sealEngine()->evmSchedule( m_previousBlock.timestamp(), m_currentBlock.number() );
    size_t blockAuthorSharePromille = evmSchedule.shareOfBlockRewardToBlockAuthorPromille;
    if ( _stakingContractAddress == dev::ZeroAddress )
        blockAuthorSharePromille = 1000;

    // calculate block author's share
    u256 blockAuthorReward =
        dev::calculateShareWithPrecision( _blockReward, blockAuthorSharePromille );
    // calculate amount to be sent to staking contract
    u256 stakingContractReward = _blockReward - blockAuthorReward;

    // only distribute rewards for non-default blocks
    if ( m_currentBlock.author() != DEFAULT_BLOCK_OWNER_ADDRESS ) {
        // send to block author
        m_state.addBalance( m_currentBlock.author(), blockAuthorReward );

        // send to staking contract
        m_state.addBalance( _stakingContractAddress, stakingContractReward );
    }
}

const Address Block::DEFAULT_BLOCK_OWNER_ADDRESS =
    jsToAddress( "0x0000000000000000000000000000000000000000" );

#endif

void Block::applyRewards(
    vector< BlockHeader > const& _uncleBlockHeaders, u256 const& _blockReward ) {
    u256 r = _blockReward;
    for ( auto const& i : _uncleBlockHeaders ) {
        m_state.addBalance(
            i.author(), _blockReward * ( 8 + i.number() - m_currentBlock.number() ) / 8 );
        r += _blockReward / 32;
    }
    m_state.addBalance( m_currentBlock.author(), r );
}

void Block::performIrregularModifications() {
    u256 const& daoHardfork = m_sealEngine->chainParams().getDaoHardforkBlock();
    if ( daoHardfork != 0 && info().number() == daoHardfork ) {
        Address recipient( "0xbf4ed7b27f1d666546e30d74d50d173d20bca754" );
        Addresses allDAOs = childDaos();
        for ( Address const& dao : allDAOs )
            m_state.transferBalance( dao, recipient, m_state.balance( dao ) );
        m_state.commit( dev::eth::CommitBehaviour::KeepEmptyAccounts, info().number() );
    }
}

void Block::updateBlockhashContract() {
    u256 const& blockNumber = info().number();

    u256 const& forkBlock = m_sealEngine->chainParams().getExperimentalForkBlock();
    if ( blockNumber == forkBlock ) {
        if ( m_state.addressInUse( c_blockhashContractAddress ) ) {
            if ( m_state.code( c_blockhashContractAddress ) != c_blockhashContractCode ) {
                State state = m_state.createStateCopyAndClearCaches();
                state.setCode( c_blockhashContractAddress, bytes( c_blockhashContractCode ),
                    m_sealEngine->evmSchedule( this->m_previousBlock.timestamp(), blockNumber )
                        .accountVersion );
                state.commit( dev::eth::CommitBehaviour::KeepEmptyAccounts, info().number() );
            }
        } else {
            m_state.createContract( c_blockhashContractAddress );
            m_state.setCode( c_blockhashContractAddress, bytes( c_blockhashContractCode ),
                m_sealEngine->evmSchedule( this->m_previousBlock.timestamp(), blockNumber )
                    .accountVersion );
            m_state.commit( dev::eth::CommitBehaviour::KeepEmptyAccounts, info().number() );
        }
    }

    if ( blockNumber >= forkBlock ) {
        DummyLastBlockHashes lastBlockHashes;  // assuming blockhash contract won't need BLOCKHASH
                                               // itself
        // HACK 0 here is for gasPrice
        Executive e( *this, lastBlockHashes, 0 );
        h256 const parentHash = m_previousBlock.hash();
        if ( !e.call( c_blockhashContractAddress, SystemAddress, 0, 0, parentHash.ref(), 1000000 ) )
            e.go();
        e.finalize();

        m_state.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, info().number() );
    }
}

void Block::commitToSeal(
    BlockChain const& _bc, bytes const& _extraData, dev::h256 const& _stateRootHash ) {
    if ( isSealed() )
        BOOST_THROW_EXCEPTION( InvalidOperationOnSealedBlock() );

    noteChain( _bc );

    if ( m_committedToSeal )
        uncommitToSeal();
    else
        m_precommit = m_state;

    vector< BlockHeader > uncleBlockHeaders;

    RLPStream unclesData;
    unsigned unclesCount = 0;

    // here was code to handle 6 generations of uncles
    // it was waiting its results in two variables above

    BytesMap transactionsMap;
    BytesMap receiptsMap;

    RLPStream txs;
    txs.appendList( m_transactions.size() );

    for ( unsigned i = 0; i < m_transactions.size(); ++i ) {
        RLPStream k;
        k << i;

        // EIP-2718 typed-receipt encoding is gated on BerlinForkPatch:
        // the EIP-1559 transaction format is accepted
        // before Berlin, but the receipt encoding must only change at the
        // coordinated Berlin fork so pre-Berlin blocks keep their receiptsRoot.
        // The parent block timestamp is used (not the global committed-block
        // timestamp) so the encoding is deterministic per block and matches enact().
        bytes receiptBytes;
        if ( BerlinForkPatch::isEnabledWhen( previousInfo().timestamp() ) &&
             receipt( i ).txType() > 0 ) {
            receiptBytes = receipt( i ).typedRlp();
        } else {
            RLPStream receiptrlp;
            receipt( i ).streamRLP( receiptrlp );
            receiptBytes = receiptrlp.out();
        }
        receiptsMap.insert( std::make_pair( k.out(), receiptBytes ) );

        dev::bytes txOutput = m_transactions[i].toBytes();
        // EIP-2718: typed transactions go into the transactions trie wrapped as an
        // RLP byte string. Unlike the receipt encoding above, this is gated on
        // EIP1559TransactionsPatch because typed transactions are accepted
        // before Berlin.
        if ( EIP1559TransactionsPatch::isEnabledWhen( previousInfo().timestamp() ) &&
             m_transactions[i].txType() != dev::eth::TransactionType::Legacy ) {
            RLPStream s;
            s.append( txOutput );
            txOutput = s.out();
        }
        transactionsMap.insert( std::make_pair( k.out(), txOutput ) );

        txs.appendRaw( txOutput );
    }

    txs.swapOut( m_currentTxs );

    RLPStream( unclesCount ).appendRaw( unclesData.out(), unclesCount ).swapOut( m_currentUncles );

    // Apply rewards last of all.
    assert( _bc.sealEngine() );
#ifndef FAIR
    applyRewards( uncleBlockHeaders,
        _bc.sealEngine()->blockReward( previousInfo().timestamp(), m_currentBlock.number() ) );
#endif

    // Commit any and all changes to the trie that are in the cache, then update the state root
    // accordingly.
    DEV_TIMED_ABOVE( "commit", 500 )

    BOOST_LOG( m_loggerTrace ) << "Post-reward stateRoot: "
                               << "is not calculated in Skale state";
    BOOST_LOG( m_loggerTrace ) << m_state;

    m_currentBlock.setLogBloom( logBloom() );
    m_currentBlock.setGasUsed( gasUsed() );
    m_currentBlock.setRoots( hash256( transactionsMap ), hash256( receiptsMap ),
        sha3( m_currentUncles ), _stateRootHash );

    m_currentBlock.setParentHash( m_previousBlock.hash() );
    m_currentBlock.setExtraData( _extraData );
    if ( m_currentBlock.extraData().size() > 32 ) {
        auto ed = m_currentBlock.extraData();
        ed.resize( 32 );
        m_currentBlock.setExtraData( ed );
    }

    m_committedToSeal = true;
}

void Block::uncommitToSeal() {
    if ( m_committedToSeal ) {
        m_state = m_precommit;
        m_committedToSeal = false;
    }
}

bool Block::sealBlock( bytesConstRef _header ) {
    if ( !m_committedToSeal )
        return false;

    if ( BlockHeader( _header, HeaderData ).hash( WithoutSeal ) !=
         m_currentBlock.hash( WithoutSeal ) )
        return false;

    // Compile block:
    RLPStream ret;
    ret.appendList( 3 );
    ret.appendRaw( _header );
    ret.appendRaw( m_currentTxs );
    ret.appendRaw( m_currentUncles );
    ret.swapOut( m_currentBytes );
    m_currentBlock = BlockHeader( _header, HeaderData );
    // TODO: move into SealEngine

    m_state = m_precommit;

    // m_currentBytes is now non-empty; we're in a sealed state so no more transactions can be
    // added.

    return true;
}


h256 Block::stateRootBeforeTx( unsigned _i ) const {
    _i = min< unsigned >( _i, m_transactions.size() );
    try {
        return ( _i > 0 ? receipt( _i - 1 ).stateRoot() : m_previousBlock.stateRoot() );
    } catch ( TransactionReceiptVersionError const& ) {
        return {};
    }
}

LogBloom Block::logBloom() const {
    LogBloom ret;
    for ( TransactionReceipt const& i : m_receipts )
        ret |= i.bloom();
    return ret;
}

void Block::cleanup() {
    MICROPROFILE_SCOPEI( "Block", "cleanup", MP_BEIGE );

    m_state.commit( dev::eth::CommitBehaviour::RemoveEmptyAccounts, info().number() );

    BOOST_LOG( m_loggerDebug ) << "Committed: stateRoot is not calculated in Skale state";

    m_previousBlock = m_currentBlock;
    sealEngine()->populateFromParent( m_currentBlock, m_previousBlock );

    BOOST_LOG( m_loggerDebug ) << "finalising enactment. current -> previous, hash is "
                               << m_previousBlock.hash();

    resetCurrent();
}
