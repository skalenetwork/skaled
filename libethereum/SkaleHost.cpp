/*
    Copyright (C) 2018-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file SkaleHost.cpp
 * @author Dima Litvinov
 * @date 2018
 */

#include "SkaleHost.h"

#include <atomic>
#include <string>

using namespace std;

#include <libconsensus/node/ConsensusEngine.h>

#include <libdevcore/microprofile.h>

#include <libdevcore/FileSystem.h>
#include <libdevcore/HashingThreadSafeQueue.h>
#include <libdevcore/RLP.h>
#include <libethcore/CommonJS.h>

#include <libethereum/ChainParams.h>
#include <libethereum/Client.h>
#include <libethereum/CommonNet.h>
#include <libethereum/Executive.h>
#include <libethereum/TransactionQueue.h>

#include <libweb3jsonrpc/JsonHelper.h>

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <libdevcore/microprofile.h>

#include <skutils/console_colors.h>

using namespace dev;
using namespace dev::eth;

#ifndef CONSENSUS
#define CONSENSUS 1
#endif

std::unique_ptr< ConsensusInterface > DefaultConsensusFactory::create(
    ConsensusExtFace& _extFace ) const {
#if CONSENSUS
    const auto& nfo = static_cast< const Interface& >( m_client ).blockInfo( LatestBlock );
    auto ts = nfo.timestamp();
    return make_unique< ConsensusEngine >( _extFace, m_client.number(), ts );
#else
    return make_unique< ConsensusStub >( _extFace, m_client.number() );
#endif
}

class ConsensusExtImpl : public ConsensusExtFace {
public:
    ConsensusExtImpl( SkaleHost& _host );
    virtual transactions_vector pendingTransactions( size_t _limit ) override;
    virtual void createBlock( const transactions_vector& _approvedTransactions, uint64_t _timeStamp,
        uint32_t _timeStampMs, uint64_t _blockID, u256 _gasPrice ) override;
    virtual void terminateApplication() override;
    virtual ~ConsensusExtImpl() override = default;

private:
    SkaleHost& m_host;
};

ConsensusExtImpl::ConsensusExtImpl( SkaleHost& _host ) : m_host( _host ) {}

ConsensusExtFace::transactions_vector ConsensusExtImpl::pendingTransactions( size_t _limit ) {
    auto ret = m_host.pendingTransactions( _limit );
    return ret;
}

void ConsensusExtImpl::createBlock(
    const ConsensusExtFace::transactions_vector& _approvedTransactions, uint64_t _timeStamp,
    uint32_t /*_timeStampMs */, uint64_t _blockID, u256 _gasPrice ) {
    MICROPROFILE_SCOPEI( "ConsensusExtFace", "createBlock", MP_INDIANRED );
    m_host.createBlock( _approvedTransactions, _timeStamp, _blockID, _gasPrice );
}

void ConsensusExtImpl::terminateApplication() {
    dev::ExitHandler::exitHandler( SIGINT );
}

SkaleHost::SkaleHost( dev::eth::Client& _client, const ConsensusFactory* _consFactory ) try
    : m_client( _client ),
      m_tq( _client.m_tq ),
      total_sent( 0 ),
      total_arrived( 0 ) {
    m_debugInterface.add_handler( [this]( const std::string& arg ) -> std::string {
        return DebugTracer_handler( arg, this->m_debugTracer );
    } );

    // m_broadcaster.reset( new HttpBroadcaster( _client ) );
    m_broadcaster.reset( new ZmqBroadcaster( _client, *this ) );

    m_extFace.reset( new ConsensusExtImpl( *this ) );

    // set up consensus
    // XXX
    if ( !_consFactory )
        m_consensus = DefaultConsensusFactory( m_client ).create( *m_extFace );
    else
        m_consensus = _consFactory->create( *m_extFace );

    m_consensus->parseFullConfigAndCreateNode( m_client.chainParams().getOriginalJson() );
} catch ( const std::exception& ) {
    std::throw_with_nested( CreationException() );
}

SkaleHost::~SkaleHost() {}

void SkaleHost::logState() {
    LOG( m_debugLogger ) << cc::debug( "sent_to_consensus = " ) << total_sent
                         << cc::debug( " got_from_consensus = " ) << total_arrived
                         << cc::debug( " m_transaction_cache = " ) << m_m_transaction_cache.size()
                         << cc::debug( " m_tq = " ) << m_tq.status().current
                         << cc::debug( " m_bcast_counter = " ) << m_bcast_counter;
}

h256 SkaleHost::receiveTransaction( std::string _rlp ) {
    Transaction transaction( jsToBytes( _rlp, OnFailed::Throw ), CheckTransaction::None );

    h256 sha = transaction.sha3();

    m_debugTracer.tracepoint( "receive_transaction" );
    {
        std::lock_guard< std::mutex > localGuard( m_receivedMutex );
        m_received.insert( sha );
        LOG( m_debugLogger ) << "m_received = " << m_received.size() << std::endl;
    }

#if ( defined _DEBUG )
    h256 sha2 =
#endif
        m_client.importTransaction( transaction );
#if ( defined _DEBUG )
    assert( sha == sha2 );
#endif

    m_debugTracer.tracepoint( "receive_transaction_success" );
    LOG( m_debugLogger ) << "Successfully received through broadcast " << sha;

    return sha;
}

ConsensusExtFace::transactions_vector SkaleHost::pendingTransactions( size_t _limit ) {
    assert( _limit > 0 );
    assert( _limit <= numeric_limits< unsigned int >::max() );

    std::lock_guard< std::mutex > pauseLock( m_consensusPauseMutex );

    if ( this->emptyBlockIntervalMsForRestore.has_value() ) {
        this->m_consensus->setEmptyBlockIntervalMs( this->emptyBlockIntervalMsForRestore.value() );
        this->emptyBlockIntervalMsForRestore.reset();
    }

    MICROPROFILE_SCOPEI( "SkaleHost", "pendingTransactions", MP_LAWNGREEN );


    ConsensusExtFace::transactions_vector out_vector;

    h256Hash to_delete;

    m_debugTracer.tracepoint( "fetch_transactions" );

    int counter = 0;

    Transactions txns = m_tq.topTransactionsSync(
        _limit, [this, &to_delete, &counter]( const Transaction& tx ) -> bool {
            if ( m_tq.getCategory( tx.sha3() ) != 1 )  // take broadcasted
                return false;

            // XXX TODO Invent good way to do this
            if ( counter++ == 0 )
                m_pending_createMutex.lock();

            if ( tx.verifiedOn < m_lastBlockWithBornTransactions )
                try {
                    Executive::verifyTransaction( tx,
                        static_cast< const Interface& >( m_client ).blockInfo( LatestBlock ),
                        m_client.state().startRead(), *m_client.sealEngine(), 0, getGasPrice() );
                } catch ( const exception& ex ) {
                    if ( to_delete.count( tx.sha3() ) == 0 )
                        clog( VerbosityInfo, "skale-host" )
                            << "Dropped now-invalid transaction in pending queue " << tx.sha3()
                            << ":" << ex.what();
                    to_delete.insert( tx.sha3() );
                    return false;
                }

            return true;
        } );

    if ( counter++ == 0 )
        m_pending_createMutex.lock();

    std::lock_guard< std::recursive_mutex > lock( m_pending_createMutex, std::adopt_lock );

    m_debugTracer.tracepoint( "drop_bad_transactions" );

    for ( auto sha : to_delete ) {
        m_debugTracer.tracepoint( "drop_bad" );
        m_tq.drop( sha );
    }

    if ( txns.size() == 0 )
        return out_vector;  // time-out with 0 results

    try {
        for ( size_t i = 0; i < txns.size(); ++i ) {
            Transaction& txn = txns[i];

            h256 sha = txn.sha3();

            if ( m_m_transaction_cache.find( sha.asArray() ) != m_m_transaction_cache.cend() )
                m_debugTracer.tracepoint( "sent_txn_again" );
            else {
                m_debugTracer.tracepoint( "sent_txn_new" );
                m_m_transaction_cache[sha.asArray()] = txn;
            }

            out_vector.push_back( txn.rlp() );

            ++total_sent;

#ifdef DEBUG_TX_BALANCE
            if ( sent.count( sha ) != 0 ) {
                int prev = sent[sha];
                std::cerr << "Prev no = " << prev << std::endl;

                if ( sent.count( sha ) != 0 ) {
                    // TODO fix this!!?
                    clog( VerbosityWarning, "skale-host" )
                        << "Sending to consensus duplicate transaction (sent before!)";
                }
            }
            sent[sha] = total_sent + i;
#endif

            m_debugTracer.tracepoint( "sent_txn" );
            LOG( m_traceLogger ) << "Sent txn: " << sha << std::endl;
        }
    } catch ( ... ) {
        clog( VerbosityError, "skale-host" ) << "BAD exception in pendingTransactions!";
    }

    logState();

    m_debugTracer.tracepoint( "send_to_consensus" );

    return out_vector;
}

void SkaleHost::createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
    uint64_t _timeStamp, uint64_t _blockID, u256 _gasPrice ) try {
    LOG( m_traceLogger ) << cc::debug( "createBlock " ) << cc::notice( "ID" ) << cc::debug( " = " )
                         << cc::warn( "#" ) << cc::num10( _blockID ) << std::endl;
    m_debugTracer.tracepoint( "create_block" );

    // convert bytes back to transactions (using caching), delete them from q and push results into
    // blockchain

    std::lock_guard< std::recursive_mutex > lock( m_pending_createMutex );

    std::vector< Transaction > out_txns;  // resultant Transaction vector

    std::atomic_bool have_consensus_born = false;  // means we need to re-verify old txns

    m_debugTracer.tracepoint( "drop_good_transactions" );

    for ( auto it = _approvedTransactions.begin(); it != _approvedTransactions.end(); ++it ) {
        const bytes& data = *it;
        h256 sha = sha3( data );
        LOG( m_traceLogger ) << cc::debug( "Arrived txn: " ) << sha << std::endl;

#ifdef DEBUG_TX_BALANCE
        if ( sent.count( sha ) != m_transaction_cache.count( sha.asArray() ) ) {
            std::cerr << cc::error( "createBlock assert" ) << std::endl;
            //            sleep(200);
            assert( sent.count( sha ) == m_transaction_cache.count( sha.asArray() ) );
        }
        assert( arrived.count( sha ) == 0 );
        arrived.insert( sha );
#endif

        // if already known
        // TODO clear occasionally this cache?!
        if ( m_m_transaction_cache.find( sha.asArray() ) != m_m_transaction_cache.cend() ) {
            Transaction t = m_m_transaction_cache.at( sha.asArray() );
            out_txns.push_back( t );
            LOG( m_debugLogger ) << "Dropping good txn " << sha << std::endl;
            m_debugTracer.tracepoint( "drop_good" );
            m_tq.dropGood( t );
            MICROPROFILE_SCOPEI( "SkaleHost", "erase from caches", MP_GAINSBORO );
            m_m_transaction_cache.erase( sha.asArray() );
            std::lock_guard< std::mutex > localGuard( m_receivedMutex );
            m_received.erase( sha );
            LOG( m_debugLogger ) << "m_received = " << m_received.size() << std::endl;
        } else {
            Transaction t( data, CheckTransaction::Everything, true );
            t.checkOutExternalGas( m_client.chainParams().externalGasDifficulty );
            out_txns.push_back( t );
            LOG( m_debugLogger ) << "Will import consensus-born txn!";
            m_debugTracer.tracepoint( "import_consensus_born" );
            have_consensus_born = true;
        }
        if ( m_tq.knownTransactions().count( sha ) != 0 ) {
            // TODO fix this!!?
            clog( VerbosityWarning, "skale-host" )
                << "Consensus returned 'future'' transaction that we didn't yet send!!";
            m_debugTracer.tracepoint( "import_future" );
        }

    }  // for
    // TODO Monitor somehow m_transaction_cache and delete long-lasting elements?

    total_arrived += out_txns.size();

    // assert( _blockID == m_client.number() + 1 );

    m_debugTracer.tracepoint( "import_block" );

    size_t n_succeeded = m_client.importTransactionsAsBlock( out_txns, _gasPrice, _timeStamp );
    if ( n_succeeded != out_txns.size() )
        penalizePeer();

    LOG( m_traceLogger ) << cc::success( "Successfully imported " ) << n_succeeded
                         << cc::success( " of " ) << out_txns.size()
                         << cc::success( " transactions" ) << std::endl;

    if ( have_consensus_born )
        this->m_lastBlockWithBornTransactions = _blockID;

    logState();
} catch ( const std::exception& ex ) {
    cerror << "CRITICAL " << ex.what() << " (in createBlock)";
} catch ( ... ) {
    cerror << "CRITICAL unknown exception (in createBlock)";
}

void SkaleHost::startWorking() {
    if ( working )
        return;

    // TODO Should we do it at end of this func? (problem: broadcaster receives transaction and
    // recursively calls this func - so working is still false!)
    working = true;

    try {
        m_broadcaster->startService();
    } catch ( const Broadcaster::StartupException& ) {
        working = false;
        std::throw_with_nested( SkaleHost::CreationException() );
    }

    auto bcast_func = std::bind( &SkaleHost::broadcastFunc, this );
    m_broadcastThread = std::thread( bcast_func );

    try {
        m_consensus->startAll();
    } catch ( const std::exception& ) {
        // cleanup
        m_exitNeeded = true;
        m_broadcastThread.join();
        throw;
    }

    auto csus_func = [&]() {
        try {
            dev::setThreadName( "bootStrapAll" );
            m_consensus->bootStrapAll();
        } catch ( std::exception& ex ) {
            std::string s = ex.what();
            if ( s.empty() )
                s = "no description";
            std::cout << "Consensus thread in scale host will exit with exception: " << s << "\n";
        } catch ( ... ) {
            std::cout << "Consensus thread in scale host will exit with unknown exception\n";
        }
    };
    // std::bind(&ConsensusEngine::bootStrapAll, m_consensus.get());
    // m_consensus->bootStrapAll();
    m_consensusThread = std::thread( csus_func );  // TODO Stop function for it??!
}

// TODO finish all gracefully to allow all undone jobs be finished
void SkaleHost::stopWorking() {
    if ( !working )
        return;

    m_exitNeeded = true;
    pauseConsensus( false );
    m_consensus->exitGracefully();
    m_consensusThread.join();

    m_broadcastThread.join();

    working = false;
}

void SkaleHost::broadcastFunc() {
    dev::setThreadName( "broadcastFunc" );
    while ( !m_exitNeeded ) {
        try {
            m_broadcaster->broadcast( "" );  // HACK this is just to initialize sockets

            dev::eth::Transactions txns = m_tq.topTransactionsSync( 1, 0, 1 );
            if ( txns.empty() )  // means timeout
                continue;

            this->logState();

            MICROPROFILE_SCOPEI( "SkaleHost", "broadcastFunc", MP_BISQUE );

            assert( txns.size() == 1 );
            Transaction& txn = txns[0];
            h256 sha = txn.sha3();

            // TODO XXX such blocks are bad :(
            size_t received;
            {
                std::lock_guard< std::mutex > lock( m_receivedMutex );
                received = m_received.count( sha );
            }

            if ( received == 0 ) {
                try {
                    if ( !m_broadcastPauseFlag ) {
                        MICROPROFILE_SCOPEI(
                            "SkaleHost", "broadcastFunc.broadcast", MP_CHARTREUSE1 );
                        m_debugTracer.tracepoint( "broadcast" );
                        m_broadcaster->broadcast( toJS( txn.rlp() ) );
                    }
                } catch ( const std::exception& ex ) {
                    cwarn << "BROADCAST EXCEPTION CAUGHT" << endl;
                    cwarn << ex.what() << endl;
                }  // catch

            }  // if
            else
                m_debugTracer.tracepoint( "broadcast_already_have" );

            ++m_bcast_counter;

            logState();
        } catch ( const std::exception& ex ) {
            cerror << "CRITICAL " << ex.what() << " (restarting broadcastFunc)";
            sleep( 2 );
        } catch ( ... ) {
            cerror << "CRITICAL unknown exception (restarting broadcastFunc)";
            sleep( 2 );
        }
    }  // while

    m_broadcaster->stopService();
}

u256 SkaleHost::getGasPrice() const {
    return m_consensus->getPriceForBlockId( m_client.number() );
}

void SkaleHost::forceEmptyBlock() {
    assert( !this->emptyBlockIntervalMsForRestore.has_value() );
    this->emptyBlockIntervalMsForRestore = this->m_consensus->getEmptyBlockIntervalMs();
    // HACK it should be less than time-out in pendingTransactions - but not 0!
    this->m_consensus->setEmptyBlockIntervalMs( 50 );  // just 1-time!
}

void SkaleHost::forcedBroadcast( const Transaction& _txn ) {
    m_broadcaster->broadcast( toJS( _txn.rlp() ) );
}

std::string SkaleHost::debugCall( const std::string& arg ) {
    return m_debugInterface.call( arg );
}

void SkaleHost::noteNewTransactions() {}

void SkaleHost::noteNewBlocks() {}

void SkaleHost::onBlockImported( BlockHeader const& /*_info*/ ) {}
