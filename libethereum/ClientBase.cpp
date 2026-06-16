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
/** @file ClientBase.cpp
 * @author Gav Wood <i@gavwood.com>
 * @author Marek Kotewicz <marek@ethdev.com>
 * @date 2015
 */

#include "ClientBase.h"
#include <libethereum/SchainPatch.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "BlockChain.h"
#include "Executive.h"

using namespace std;
using std::make_pair;
using std::pair;

using namespace dev;
using namespace dev::eth;
using skale::Permanence;
using skale::State;

u256 ClientBase::getBaseFeeValue( BlockHeader const& _header ) const {
    if ( _header.baseFeePerGas() != 0 || _header.number() == 0 ||
         !LondonForkPatch::isEnabledWhen( static_cast< time_t >( _header.timestamp() ) ) )
        return _header.baseFeePerGas();

    try {
        return gasBidPrice( static_cast< unsigned >( _header.number() ) );
    } catch ( std::invalid_argument const& ) {
        BOOST_THROW_EXCEPTION(
            std::runtime_error( "Historical baseFeePerGas is unavailable for London block " +
                                toString( _header.number() ) ) );
    }
}

static const int64_t c_maxGasEstimate = 50000000;

ClientWatch::ClientWatch() : lastPoll( std::chrono::system_clock::now() ) {}

ClientWatch::ClientWatch(
    bool isWS, h256 _id, Reaping _r, fnClientWatchHandlerMulti_t fnOnNewChanges, unsigned iw )
    : id( _id ),
      iw_( iw ),
      isWS_( isWS ),
      fnOnNewChanges_( fnOnNewChanges ),
      lastPoll( ( _r == Reaping::Automatic ) ? std::chrono::system_clock::now() :
                                               std::chrono::system_clock::time_point::max() ) {}

LocalisedLogEntries ClientWatch::get_changes() const {
    return changes_;
}

void ClientWatch::swap_changes( LocalisedLogEntries& otherChanges ) {
    if ( ( ( void* ) &changes_ ) == ( ( void* ) &otherChanges ) )
        return;
    std::swap( changes_, otherChanges );
    if ( !changes_.empty() )
        fnOnNewChanges_( iw_ );
}

void ClientWatch::append_changes( const LocalisedLogEntries& otherChanges ) {
    if ( ( ( void* ) &changes_ ) == ( ( void* ) &otherChanges ) )
        return;
    changes_ += otherChanges;
    if ( !changes_.empty() )
        fnOnNewChanges_( iw_ );
}

void ClientWatch::append_changes( const LocalisedLogEntry& entry ) {
    changes_.push_back( entry );
    if ( !changes_.empty() )
        fnOnNewChanges_( iw_ );
}


std::pair< bool, ExecutionResult > ClientBase::estimateGasStep( int64_t _gas, Block& _latestBlock,
    Block& _pendingBlock, Address const& _from, Address const& _destination, u256 const& _value,
    u256 const& _gasPrice, bytes const& _data ) {
    u256 nonce = _latestBlock.transactionsFrom( _from );
    Transaction t;
    if ( _destination )
        t = Transaction( _value, _gasPrice, _gas, _destination, _data, nonce );
    else
        t = Transaction( _value, _gasPrice, _gas, _data, nonce );
    t.forceSender( _from );
    t.forceChainId( chainId() );
#ifndef FAIR
    t.ignoreExternalGas();
#endif
    EnvInfo const env( _pendingBlock.info(), bc().lastBlockHashes(),
        _pendingBlock.previousInfo().timestamp(), 0, _gas, bc().chainParams().getChainId() );
    // Make a copy of the state, it will be deleted after this step
    State tempState = _latestBlock.mutableState();
    tempState.addBalance( _from, ( u256 )( t.gas() * t.gasPrice() + t.value() ) );
    ExecutionResult executionResult =
        tempState.execute( env, bc().chainParams(), t, Permanence::Reverted ).first;
    if ( executionResult.excepted == TransactionException::OutOfGas ||
         executionResult.excepted == TransactionException::OutOfGasBase ||
         executionResult.excepted == TransactionException::OutOfGasIntrinsic ||
         executionResult.codeDeposit == CodeDeposit::Failed ||
         executionResult.excepted == TransactionException::BadJumpDestination ||
         executionResult.excepted == TransactionException::RevertInstruction ) {
        return make_pair( false, executionResult );
    } else {
        return make_pair( true, executionResult );
    }
}

std::pair< u256, ExecutionResult > ClientBase::estimateGas( Address const& _from,
    u256 const& _value, Address const& _dest, bytes const& _data, int64_t _maxGas,
    u256 const& _gasPrice, GasEstimationCallback const& _callback ) {
    try {
        int64_t upperBound = _maxGas;
        if ( upperBound == Invalid256 || upperBound > c_maxGasEstimate )
            upperBound = c_maxGasEstimate;
        int64_t lowerBound;
        if ( CorrectForkInPowPatch::isEnabledInWorkingBlock() )
            lowerBound = Transaction::baseGasRequired( !_dest, &_data,
                bc().sealEngine()->chainParams().makeEvmSchedule(
                    bc().info().timestamp(), bc().number() ) );
        else
            lowerBound = Transaction::baseGasRequired( !_dest, &_data, EVMSchedule() );
        Block latest = latestBlock();
        Block pending = preSeal();

        if ( upperBound > pending.info().gasLimit() ) {
            upperBound = pending.info().gasLimit().convert_to< int64_t >();
        }
        u256 gasPrice = _gasPrice == Invalid256 ? gasBidPrice() : _gasPrice;

        // We execute transaction with maximum gas limit
        // to calculate how many of gas will be used.
        // Then we execute transaction with this gas limit
        // and check if it will be enough.
        // If not run binary search to find optimal gas limit.

        auto estimatedStep =
            estimateGasStep( upperBound, latest, pending, _from, _dest, _value, gasPrice, _data );
        if ( estimatedStep.first ) {
            auto executionResult = estimatedStep.second;
            auto gasUsed = std::max( executionResult.gasUsed.convert_to< int64_t >(), lowerBound );

            estimatedStep =
                estimateGasStep( gasUsed, latest, pending, _from, _dest, _value, gasPrice, _data );
            if ( estimatedStep.first ) {
                return make_pair( gasUsed, executionResult );
            }
            while ( lowerBound + 1 < upperBound ) {
                int64_t middle = ( lowerBound + upperBound ) / 2;
                estimatedStep = estimateGasStep(
                    middle, latest, pending, _from, _dest, _value, gasPrice, _data );
                if ( estimatedStep.first ) {
                    upperBound = middle;
                } else {
                    lowerBound = middle;
                }
                if ( _callback ) {
                    _callback( GasEstimationProgress{ lowerBound, upperBound } );
                }
            }
        }

        return make_pair( upperBound,
            estimateGasStep( upperBound, latest, pending, _from, _dest, _value, gasPrice, _data )
                .second );
    } catch ( ... ) {
        // TODO: Some sort of notification of failure.
        return make_pair( u256(), ExecutionResult() );
    }
}

ImportResult ClientBase::injectBlock( bytes const& _block ) {
    return bc().attemptImport( _block, preSeal().mutableState() ).first;
}


// TODO: remove try/catch, allow exceptions
LocalisedLogEntries ClientBase::logs( unsigned _watchId ) const {
    LogFilter f;
    try {
        Guard l( x_filtersWatches );
        f = m_filters.at( m_watches.at( _watchId ).id ).filter;
    } catch ( ... ) {
        return LocalisedLogEntries();
    }
    return logs( f );
}

LocalisedLogEntries ClientBase::logs( LogFilter const& _f ) const {
    LocalisedLogEntries ret;
    unsigned begin = min( bc().number() + 1, ( unsigned ) _f.latest() );
    unsigned end = min( bc().number(), min( begin, ( unsigned ) _f.earliest() ) );

    if ( begin >= end && begin - end > ( uint64_t ) bc().chainParams().getLogsBlocksLimit() )
        BOOST_THROW_EXCEPTION( TooBigResponse() );

    bool addPending = false;
    if ( begin > bc().number() ) {
        addPending = true;
        begin = bc().number();
    }

    // Handle blocks from main chain
    set< unsigned > matchingBlocks;
    if ( !_f.isRangeFilter() )
        for ( auto const& i : _f.bloomPossibilities() ) {
            std::vector< unsigned > matchingBlocksVector = bc().withBlockBloom( i, end, begin );
            matchingBlocks.insert( matchingBlocksVector.begin(), matchingBlocksVector.end() );
        }
    else
        // if it is a range filter, we want to get all logs from all blocks in given range
        for ( unsigned i = end; i <= begin; i++ )
            matchingBlocks.insert( i );
    for ( auto n : matchingBlocks )
        appendLogsFromBlock( _f, bc().numberHash( n ), BlockPolarity::Live, ret );

    if ( addPending ) {
        // Handle pending transactions differently as they're not on the block chain.
        Block temp = postSeal();
        for ( unsigned i = 0; i < temp.pending().size(); ++i ) {
            // Might have a transaction that contains a matching log.
            TransactionReceipt const& tr = temp.receipt( i );
            LogEntries le = _f.matches( tr );
            for ( unsigned j = 0; j < le.size(); ++j )
                ret.emplace_back( LocalisedLogEntry( le[j] ) );
        }
    }

    return ret;
}

void ClientBase::appendLogsFromBlock( LogFilter const& _f, h256 const& _blockHash,
    BlockPolarity _polarity, LocalisedLogEntries& io_logs ) const {
    auto receiptsBundle = bc().receipts( _blockHash );
    const auto& receipts = receiptsBundle.receipts;
    const auto& hashes = bc().transactionHashes( _blockHash );
    auto blockNumber = ( BlockNumber ) bc().number( _blockHash );

    unsigned logIndex = 0;
    const int64_t logCountLimit = bc().chainParams().getResponseLogCountLimit();
    const bool limitEnabled = logCountLimit != -1;

    for ( size_t i = 0; i < receipts.size(); i++ ) {
        const TransactionReceipt& receipt = receipts[i];
        const h256& th = hashes[i];

        if ( _f.isRangeFilter() ) {
            for ( const auto& e : receipt.log() ) {
                if ( limitEnabled && io_logs.size() + 1 > static_cast< size_t >( logCountLimit ) )
                    BOOST_THROW_EXCEPTION( LogCountLimitExceeded() );

                io_logs.emplace_back(
                    LocalisedLogEntry( e, _blockHash, blockNumber, th, i, logIndex++, _polarity ) );
            }
            continue;
        }

        if ( _f.matches( receipt.bloom() ) ) {
            const auto& filterAddresses = _f.getAddresses();
            const auto& filterTopicsArray = _f.getTopics();

            for ( const auto& e : receipt.log() ) {
                if ( !filterAddresses.empty() &&
                     std::find( filterAddresses.begin(), filterAddresses.end(), e.address ) ==
                         filterAddresses.end() ) {
                    ++logIndex;
                    continue;
                }

                bool isGood = true;
                for ( unsigned j = 0; j < 4 && isGood; ++j ) {
                    const auto& topics = filterTopicsArray[j];
                    if ( !topics.empty() &&
                         ( e.topics.size() <= j || std::find( topics.begin(), topics.end(),
                                                       e.topics[j] ) == topics.end() ) ) {
                        isGood = false;
                    }
                }

                if ( isGood ) {
                    if ( limitEnabled && io_logs.size() >= static_cast< size_t >( logCountLimit ) )
                        BOOST_THROW_EXCEPTION( LogCountLimitExceeded() );

                    io_logs.emplace_back( LocalisedLogEntry(
                        e, _blockHash, blockNumber, th, i, logIndex++, _polarity ) );
                } else {
                    ++logIndex;
                }
            }
        } else {
            logIndex += receipt.log().size();
        }
    }
}

unsigned ClientBase::installWatch(
    LogFilter const& _f, Reaping _r, fnClientWatchHandlerMulti_t fnOnNewChanges, bool isWS ) {
    h256 h = _f.sha3();
    {
        Guard l( x_filtersWatches );
        if ( !m_filters.count( h ) ) {
            BOOST_LOG( m_loggerWatch ) << "FFF" << _f << h;
            m_filters.insert( make_pair( h, _f ) );
        }
    }
    return installWatch( h, _r, fnOnNewChanges, isWS );
}

unsigned ClientBase::installWatch(
    h256 const& _h, Reaping _r, fnClientWatchHandlerMulti_t fnOnNewChanges, bool isWS ) {
    unsigned ret;
    {
        Guard l( x_filtersWatches );
        ret = m_watches.size() ? m_watches.rbegin()->first + 1 : 0;
        m_watches[ret] = ClientWatch( isWS, _h, _r, fnOnNewChanges, ret );
        BOOST_LOG( m_loggerWatch ) << "+++" << ret << _h;
    }
#if INITIAL_STATE_AS_CHANGES
    auto ch = logs( ret );
    if ( ch.empty() )
        ch.push_back( InitialChange );
    {
        Guard l( x_filtersWatches );
        m_watches[ret].swap_changes( ch );
    }
#endif
    return ret;
}

bool ClientBase::uninstallWatch( unsigned _i ) {
    BOOST_LOG( m_loggerWatch ) << "Uninstalling watch " << _i;

    Guard l( x_filtersWatches );

    auto it = m_watches.find( _i );
    if ( it == m_watches.end() )
        return false;
    auto id = it->second.id;
    m_watches.erase( it );

    auto fit = m_filters.find( id );
    if ( fit != m_filters.end() )
        if ( !--fit->second.refCount ) {
            BOOST_LOG( m_loggerWatch ) << "*X*" << fit->first << ":" << fit->second.filter;
            m_filters.erase( fit );
        }
    return true;
}

LocalisedLogEntries ClientBase::checkWatch( unsigned _watchId ) {
    Guard l( x_filtersWatches );
    LocalisedLogEntries ret;

    //	BOOST_LOG(m_loggerWatch) << "checkWatch" << _watchId;
    auto& w = m_watches.at( _watchId );
    //	BOOST_LOG(m_loggerWatch) << "lastPoll updated to " <<
    // chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
    w.swap_changes( ret );
    if ( w.lastPoll != chrono::system_clock::time_point::max() )
        w.lastPoll = chrono::system_clock::now();

    return ret;
}

BlockHeader ClientBase::blockInfo( h256 const& _hash ) const {
    if ( _hash == PendingBlockHash )
        return preSeal().info();

    BlockHeader blockInfo( bc().block( _hash ) );
    blockInfo.setBaseFeePerGas( getBaseFeeValue( blockInfo ) );
    return blockInfo;
}

BlockDetails ClientBase::blockDetails( h256 const& _hash ) const {
    return bc().details( _hash );
}

Transaction ClientBase::transaction( h256 const& _transactionHash ) const {
    // allow invalid!
    auto tl = bc().transactionLocation( _transactionHash );
    auto blockTimestamp = blockInfo( numberFromHash( tl.first ) - 1 ).timestamp();
    return Transaction( bc().transaction( _transactionHash ), CheckTransaction::Cheap, true,
        EIP1559TransactionsPatch::isEnabledWhen( blockTimestamp ),
        InvalidTransactionFormatPatch::isEnabledWhen( blockTimestamp ),
        BerlinForkPatch::isEnabledWhen( blockTimestamp )
#ifdef BITE
            ,
        Bite2Patch::isEnabledWhen( blockTimestamp )
#endif  // BITE
    );
}

LocalisedTransaction ClientBase::localisedTransaction( h256 const& _transactionHash ) const {
    std::pair< h256, unsigned > tl = bc().transactionLocation( _transactionHash );
    return localisedTransaction( tl.first, tl.second );
}

Transaction ClientBase::transaction( h256 const& _blockHash, unsigned _i ) const {
    auto bl = bc().block( _blockHash );
    RLP b( bl );
    auto blockTimestamp = blockInfo( numberFromHash( _blockHash ) - 1 ).timestamp();
    if ( _i < b[1].itemCount() )
        // allow invalid
        return Transaction( b[1][_i].data(), CheckTransaction::Cheap, true,
            EIP1559TransactionsPatch::isEnabledWhen( blockTimestamp ),
            InvalidTransactionFormatPatch::isEnabledWhen( blockTimestamp ),
            BerlinForkPatch::isEnabledWhen( blockTimestamp )
#ifdef BITE
                ,
            Bite2Patch::isEnabledWhen( blockTimestamp )
#endif  // BITE
        );
    else
        return Transaction();
}

LocalisedTransaction ClientBase::localisedTransaction( h256 const& _blockHash, unsigned _i ) const {
    auto blockTimestamp = blockInfo( numberFromHash( _blockHash ) - 1 ).timestamp();
    // allow invalid
    Transaction t = Transaction( bc().transaction( _blockHash, _i ), CheckTransaction::Cheap, true,
        EIP1559TransactionsPatch::isEnabledWhen( blockTimestamp ),
        InvalidTransactionFormatPatch::isEnabledWhen( blockTimestamp ),
        BerlinForkPatch::isEnabledWhen( blockTimestamp )
#ifdef BITE
            ,
        Bite2Patch::isEnabledWhen( blockTimestamp )
#endif  // BITE
    );
    return LocalisedTransaction( t, _blockHash, _i, numberFromHash( _blockHash ) );
}

TransactionReceipt ClientBase::transactionReceipt( h256 const& _transactionHash ) const {
    return bc().transactionReceipt( _transactionHash );
}

LocalisedTransactionReceipt ClientBase::localisedTransactionReceipt(
    h256 const& _transactionHash ) const {
    auto [blockHash, transactionIdx] = bc().transactionLocation( _transactionHash );
    dev::eth::BlockNumber blockNumber{ numberFromHash( blockHash ) };

    auto blockTimestamp = blockInfo( blockNumber - 1 ).timestamp();

    Transaction t = Transaction( bc().transaction( blockHash, transactionIdx ),  // rlp
        CheckTransaction::Cheap,                                                 // Check sig
        true,                                                                    // allow invalid
        EIP1559TransactionsPatch::isEnabledWhen( blockTimestamp ),
        InvalidTransactionFormatPatch::isEnabledWhen( blockTimestamp ),
        BerlinForkPatch::isEnabledWhen( blockTimestamp )
#ifdef BITE
            ,
        Bite2Patch::isEnabledWhen( blockTimestamp )
#endif  // BITE
    );

    TransactionReceipt receipt = bc().transactionReceipt( blockHash, transactionIdx );

    u256 gasUsed = receipt.cumulativeGasUsed();
    if ( transactionIdx > 0 ) {
        gasUsed -= bc().transactionReceipt( blockHash, transactionIdx - 1 ).cumulativeGasUsed();
    }

    // The "contractAddress" field must be null for all types of transactions but contract
    // deployment ones. The contract deployment transaction is special because it's the only type of
    // transaction with "to" filed set to null.
    dev::Address contractAddress;
    if ( !t.isInvalid() && t.to() == dev::Address( 0 ) ) {
        // if this transaction is contract deployment
        contractAddress = toAddress( t.from(), t.nonce() );
    }

    // Set transaction's 'to' address
    // If BITE -> get decrypted 'to'
    // Else    -> get 'to'
    dev::Address to;
    if ( !t.isInvalid() ) {
#ifdef BITE
        if ( t.isBite() ) {
            DecryptedTransactionData decryptedData = decryptedTransactionData( _transactionHash );
            to = decryptedData.to();
        } else
#endif

            to = t.to();

    } else {
        to = dev::Address( 0 );
    }

    // Build all other needed fields
    dev::h256 txHash{ t.sha3() };
    dev::Address from{ t.isInvalid() ? dev::Address( 0 ) : t.from() };
    int txType{ t.txType() };
    dev::u256 effectiveGasPrice{ 0 };
    if ( !t.isInvalid() ) {
        const auto blkInfo = blockInfo( blockHash );
        const bool isLondon =
            LondonForkPatch::isEnabledWhen( static_cast< time_t >( blkInfo.timestamp() ) );
#ifndef FAIR
        // A tx rebuilt from RLP isn't external-gas-checked, so re-run checkOutExternalGas with the
        // parent block's context (as live execution does) to recover the zero-fee verdict.
        if ( bc().chainParams().getExternalGasDifficulty() > 0 && blockNumber > 0 ) {
            try {
                const auto parentInfo = blockInfo( blockNumber - 1 );
                t.checkOutExternalGas( bc().chainParams(),
                    static_cast< time_t >( parentInfo.timestamp() ),
                    static_cast< uint64_t >( blockNumber - 1 ) );
            } catch ( Exception const& _e ) {
                // Only InvalidSignature is possible here and it can't fire for a committed tx;
                // stay defensive so receipt RPC never fails.
                cwarn << "External-gas recheck failed for receipt of a tx in block " << blockNumber
                      << ": " << _e.what();
            }
        }
#endif
        effectiveGasPrice = t.getEffectiveGasPrice( isLondon, blkInfo.baseFeePerGas() );
    }

    return LocalisedTransactionReceipt( receipt, txHash, blockHash, blockNumber, transactionIdx,
        from, to, gasUsed, contractAddress, txType, effectiveGasPrice );
}

pair< h256, unsigned > ClientBase::transactionLocation( h256 const& _transactionHash ) const {
    return bc().transactionLocation( _transactionHash );
}

Transactions ClientBase::transactions( h256 const& _blockHash ) const {
    auto bl = bc().block( _blockHash );
    RLP b( bl );
    Transactions res;
    auto blockTimestamp = blockInfo( numberFromHash( _blockHash ) - 1 ).timestamp();
    for ( unsigned i = 0; i < b[1].itemCount(); i++ ) {
        auto txRlp = b[1][i];
        res.emplace_back( bytesRefFromTransactionRlp( txRlp ), CheckTransaction::Cheap, true,
            EIP1559TransactionsPatch::isEnabledWhen( blockTimestamp ),
            InvalidTransactionFormatPatch::isEnabledWhen( blockTimestamp ),
            BerlinForkPatch::isEnabledWhen( blockTimestamp )
#ifdef BITE
                ,
            Bite2Patch::isEnabledWhen( blockTimestamp )
#endif  // BITE
        );
    }
    return res;
}

TransactionHashes ClientBase::transactionHashes( h256 const& _blockHash ) const {
    return bc().transactionHashes( _blockHash );
}

#ifdef BITE
DecryptedTransactionData ClientBase::decryptedTransactionData(
    h256 const& _transactionHash ) const {
    return bc().decryptedTransactionData( _transactionHash );
}

dev::h256 ClientBase::ctxOrigin( const dev::h256& _ctxHash ) const {
    auto tx = transaction( _ctxHash );
    if ( !tx.isCTX() )
        throw std::logic_error( "Trying to get ctxOrigin for non-CTX" );
    return bc().ctxOrigin( _ctxHash ).value;
}

std::vector< dev::h256 > ClientBase::craftedCTXs( const dev::h256& _transactionHash ) const {
    auto tl = bc().transactionLocation( _transactionHash );
    CreatedCTXs ctxHashesLists = bc().ctxHashesForBlock( tl.first );
    if ( ctxHashesLists.count() < tl.second + 1 )
        return {};
    return ctxHashesLists[tl.second];
}
#endif  // BITE

BlockHeader ClientBase::uncle( h256 const& _blockHash, unsigned _i ) const {
    auto bl = bc().block( _blockHash );
    RLP b( bl );
    if ( _i < b[2].itemCount() )
        return BlockHeader( b[2][_i].data(), HeaderData );
    else
        return BlockHeader();
}

UncleHashes ClientBase::uncleHashes( h256 const& _blockHash ) const {
    return bc().uncleHashes( _blockHash );
}

unsigned ClientBase::transactionCount( h256 const& _blockHash ) const {
    auto bl = bc().block( _blockHash );
    RLP b( bl );
    return b[1].itemCount();
}

unsigned ClientBase::uncleCount( h256 const& _blockHash ) const {
    auto bl = bc().block( _blockHash );
    RLP b( bl );
    return b[2].itemCount();
}

unsigned ClientBase::number() const {
    return bc().number();
}

h256s ClientBase::pendingHashes() const {
    return h256s() + postSeal().pendingHashes();
}

BlockHeader ClientBase::pendingInfo() const {
    return postSeal().info();
}

BlockDetails ClientBase::pendingDetails() const {
    auto pm = postSeal().info();
    auto li = Interface::blockDetails( LatestBlock );
    return BlockDetails( ( unsigned ) pm.number(), li.totalDifficulty + pm.difficulty(),
        pm.parentHash(), h256s{}, postSeal().blockData().size() );
}

EVMSchedule ClientBase::evmSchedule() const {
    return sealEngine()->evmSchedule( bc().info().timestamp(), pendingInfo().number() );
}

u256 ClientBase::gasLimitRemaining() const {
    return postSeal().gasLimitRemaining();
}

Address ClientBase::author() const {
    return preSeal().author();
}

h256 ClientBase::hashFromNumber( BlockNumber _number ) const {
    if ( _number == PendingBlock )
        return h256();
    if ( _number == LatestBlock )
        return bc().currentHash();
    return bc().numberHash( _number );
}

BlockNumber ClientBase::numberFromHash( h256 const& _blockHash ) const {
    if ( _blockHash == PendingBlockHash )
        return bc().number() + 1;
    else if ( _blockHash == LatestBlockHash )
        return bc().number();
    else if ( _blockHash == EarliestBlockHash )
        return 0;
    return bc().number( _blockHash );
}

int ClientBase::compareBlockHashes( h256 const& _h1, h256 const& _h2 ) const {
    BlockNumber n1 = numberFromHash( _h1 );
    BlockNumber n2 = numberFromHash( _h2 );

    if ( n1 > n2 )
        return 1;
    else if ( n1 == n2 )
        return 0;
    return -1;
}

bool ClientBase::isKnown( h256 const& _hash ) const {
    return _hash == PendingBlockHash || _hash == LatestBlockHash || _hash == EarliestBlockHash ||
           bc().isKnown( _hash );
}

bool ClientBase::isKnown( BlockNumber _block ) const {
    if ( _block == PendingBlock )
        return true;
    if ( _block == LatestBlock )
        return true;
    auto a = bc().numberHash( _block );
    auto b = h256();
    if ( a != b )
        return true;
    return false;
}

bool ClientBase::isKnownTransaction( h256 const& _transactionHash ) const {
    return bc().isKnownTransaction( _transactionHash );
}

bool ClientBase::isKnownTransaction( h256 const& _blockHash, unsigned _i ) const {
    bytes block = bc().block( _blockHash );

    if ( block.empty() )
        return false;

    VerifiedBlockRef vb = bc().verifyBlock( &block, function< void( Exception& ) >() );

    return vb.transactions.size() > _i;
}

Block ClientBase::latestBlock() const {
    Block res = postSeal();
    return res;
}

uint64_t ClientBase::chainId() const {
    return bc().chainParams().getChainId();
}


u256 ClientBase::countAt( Address const& _a ) const {
    return getReadOnlyLatestBlockCopy().state().getNonce( _a );
}

u256 ClientBase::balanceAt( Address const& _a ) const {
    return getReadOnlyLatestBlockCopy().state().balance( _a );
}

u256 ClientBase::stateAt( Address const& _a, u256 const& _l ) const {
    return getReadOnlyLatestBlockCopy().state().storage( _a, _l );
}

bytes ClientBase::codeAt( Address const& _a ) const {
    return getReadOnlyLatestBlockCopy().state().code( _a );
}

h256 ClientBase::codeHashAt( Address const& _a ) const {
    return getReadOnlyLatestBlockCopy().state().codeHash( _a );
}

map< h256, pair< u256, u256 > > ClientBase::storageAt( Address const& _a ) const {
    return getReadOnlyLatestBlockCopy().state().storage( _a );
}
