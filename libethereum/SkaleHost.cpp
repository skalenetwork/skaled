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

#include <string>

using namespace std;

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
    return make_unique< ConsensusStub >( _extFace );
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
    uint32_t /*_timeStampMs */, uint64_t _blockID, u256 /* _gasPrice */ ) {
    MICROPROFILE_SCOPEI( "ConsensusExtFace", "createBlock", MP_INDIANRED );
    m_host.createBlock( _approvedTransactions, _timeStamp, _blockID );
}

void ConsensusExtImpl::terminateApplication() {
    dev::ExitHandler::exitHandler( SIGINT );
}

SkaleHost::SkaleHost( dev::eth::Client& _client, const ConsensusFactory* _consFactory ) try
    : m_client( _client ),
      m_tq( _client.m_tq ),
      total_sent( 0 ),
      total_arrived( 0 ) {
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
    LOG( m_debugLogger ) << "sent_to_consensus = " << total_sent
                         << " got_from_consensus = " << total_arrived
                         << " m_transaction_cache = " << m_transaction_cache.size()
                         << " m_tq = " << m_tq.status().current
                         << " m_bcast_counter = " << m_bcast_counter;
}

h256 SkaleHost::receiveTransaction( std::string _rlp ) {
    Transaction transaction( jsToBytes( _rlp, OnFailed::Throw ), CheckTransaction::None );

    h256 sha = transaction.sha3();

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

    LOG( m_debugLogger ) << "Successfully received through broadcast " << sha;

    return sha;
}

ConsensusExtFace::transactions_vector SkaleHost::pendingTransactions( size_t _limit ) {
    assert( _limit > 0 );
    assert( _limit <= numeric_limits< unsigned int >::max() );

    MICROPROFILE_SCOPEI( "SkaleHost", "pendintTransactions", MP_LAWNGREEN );

    ConsensusExtFace::transactions_vector out_vector;

    h256Hash to_delete;

    Transactions txns =
        m_tq.topTransactionsSync( _limit, [this, &to_delete]( const Transaction& tx ) -> bool {
            if ( m_tq.getCategory( tx.sha3() ) != 1 )  // take broadcasted
                return false;

            if ( tx.verifiedOn < m_lastBlockWithBornTransactions )
                try {
                    Executive::verifyTransaction( tx,
                        static_cast< const Interface& >( m_client ).blockInfo( LatestBlock ),
                        m_client.state().startRead(), *m_client.sealEngine(), 0 );
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

    for ( auto sha : to_delete )
        m_tq.drop( sha );

    if ( txns.size() == 0 )
        return out_vector;  // time-out with 0 results

    try {
        for ( size_t i = 0; i < txns.size(); ++i ) {
            std::lock_guard< std::mutex > pauseLock( m_consensusPauseMutex );
            Transaction& txn = txns[i];

            h256 sha = txn.sha3();
            m_transaction_cache[sha.asArray()] = txn;
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

            LOG( m_traceLogger ) << "Sent txn: " << sha << std::endl;
        }
    } catch ( ... ) {
        clog( VerbosityError, "skale-host" ) << "BAD exception in pendingTransactions!";
    }

    logState();

    return out_vector;
}

void SkaleHost::createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
    uint64_t _timeStamp, uint64_t _blockID ) try {
    // convert bytes back to transactions (using caching), delete them from q and push results into
    // blockchain
    std::vector< Transaction > out_txns;  // resultant Transaction vector

    bool have_consensus_born = false;  // means we need to re-verify old txns

    for ( auto it = _approvedTransactions.begin(); it != _approvedTransactions.end(); ++it ) {
        const bytes& data = *it;
        h256 sha = sha3( data );
        LOG( m_traceLogger ) << "Arrived txn: " << sha << std::endl;

#ifdef DEBUG_TX_BALANCE
        if ( sent.count( sha ) != m_transaction_cache.count( sha.asArray() ) ) {
            std::cerr << "createBlock assert" << std::endl;
            //            sleep(200);
            assert( sent.count( sha ) == m_transaction_cache.count( sha.asArray() ) );
        }
        assert( arrived.count( sha ) == 0 );
        arrived.insert( sha );
#endif

        // if already known
        // TODO clear occasionally this cache?!
        if ( m_transaction_cache.count( sha.asArray() ) ) {
            Transaction& t = m_transaction_cache[sha.asArray()];

            out_txns.push_back( t );
            m_tq.dropGood( t );
            MICROPROFILE_SCOPEI( "SkaleHost", "erase from caches", MP_GAINSBORO );
            m_transaction_cache.erase( sha.asArray() );
            m_received.erase( sha );

            LOG( m_debugLogger ) << "m_received = " << m_received.size() << std::endl;
        }
        // if new
        else {
            Transaction t( data, CheckTransaction::Everything, true );
            t.checkOutExternalGas( m_client.chainParams().externalGasDifficulty );
            out_txns.push_back( t );
            LOG( m_debugLogger ) << "Will import consensus-born txn!";
            have_consensus_born = true;
        }  // else

        if ( m_tq.knownTransactions().count( sha ) != 0 ) {
            // TODO fix this!!?
            clog( VerbosityWarning, "skale-host" )
                << "Consensus returned 'future'' transaction that we didn't yet send!!";
        }

    }  // for
    // TODO Monitor somehow m_transaction_cache and delete long-lasting elements?

    if ( m_transaction_cache.size() != 0 ) {
        clog( VerbosityInfo, "skale-host" )
            << "Erasing " << m_transaction_cache.size() << " txns from m_transaction_cache";
        m_transaction_cache.clear();
    }

    total_arrived += out_txns.size();

    assert( _blockID == m_client.number() + 1 );

    size_t n_succeeded = m_client.importTransactionsAsBlock( out_txns, _timeStamp );
    if ( n_succeeded != out_txns.size() )
        penalizePeer();

    LOG( m_traceLogger ) << "Successfully imported " << n_succeeded << " of " << out_txns.size()
                         << " transactions" << std::endl;

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

    working = true;
}

// TODO finish all gracefully to allow all undone jobs be finished
void SkaleHost::stopWorking() {
    if ( !working )
        return;

    m_exitNeeded = true;
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

            // TODO XXX such blocks suck :(
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
                        m_broadcaster->broadcast( toJS( txn.rlp() ) );
                    }
                } catch ( const std::exception& ex ) {
                    cwarn << "BROADCAST EXCEPTION CAUGHT" << endl;
                    cwarn << ex.what() << endl;
                }  // catch

            }  // if

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

void SkaleHost::noteNewTransactions() {}

void SkaleHost::noteNewBlocks() {}

void SkaleHost::onBlockImported( BlockHeader const& /*_info*/ ) {}
