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
        uint64_t _blockID ) override;
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
    uint64_t _blockID ) {
    MICROPROFILE_SCOPEI( "ConsensusExtFace", "createBlock", MP_INDIANRED );
    m_host.createBlock( _approvedTransactions, _timeStamp, _blockID );
}

void ConsensusExtImpl::terminateApplication() {
    dev::ExitHandler::exitHandler( SIGINT );
}

SkaleHost::SkaleHost( dev::eth::Client& _client, dev::eth::TransactionQueue& _tq,
    const ConsensusFactory* _consFactory )
    : m_client( _client ), m_tq( _tq ), total_sent( 0 ), total_arrived( 0 ) {
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
}

SkaleHost::~SkaleHost() {}

void SkaleHost::logState() {
    LOG( m_debugLogger ) << "sent_to_consensus = " << total_sent
                         << " got_from_consensus = " << total_arrived
                         << " m_transaction_cache = " << m_transaction_cache.size()
                         << " m_tq = " << m_tq.knownTransactions().size()
                         << " broadcasted = " << m_broadcastedQueue.size();
}

h256 SkaleHost::receiveTransaction( std::string _rlp ) {
    Transaction transaction( jsToBytes( _rlp, OnFailed::Throw ), CheckTransaction::None );
    h256 sha = transaction.sha3();

    {
        std::lock_guard< std::mutex > localGuard( m_localMutex );
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

    size_t i = 0;
    try {
        while ( i < _limit ) {
            std::lock_guard< std::mutex > localGuard( m_localMutex );  // need to lock while in
                                                                       // transition from q to q
            Transaction txn = m_broadcastedQueue.pop();

            // re-verify transaction aginst current block
            // throws in case of error
            Executive::verifyTransaction( txn,
                static_cast< const Interface& >( m_client ).blockInfo( LatestBlock ),
                m_client.state().startRead(), *m_client.sealEngine(), 0 );

            std::lock_guard< std::mutex > pauseLock( m_consensusPauseMutex );

            h256 sha = txn.sha3();
            m_transaction_cache[sha.asArray()] = txn;
            out_vector.push_back( txn.rlp() );

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
            i++;
        }
    } catch ( std::length_error& ) {
        // just end-of-transactions
        if ( out_vector.size() == 0 )
            usleep( 100000 );  // TODO implement nice popSync() when i==0!!
        total_sent += out_vector.size();
        return out_vector;  // added by advice of D4
    } catch ( abort_exception& ) {
        assert( out_vector.size() == 0 );
        return out_vector;  // they should detect abort themselves
        // TODO What if someone calls HashingThreadSafeQueue AFTER abort?!
    } catch ( const exception& ex ) {
        // usually this is tx validation exception
        clog( VerbosityWarning, "skale-host" ) << ex.what();
    }

    total_sent += out_vector.size();

    logState();

    return out_vector;
}

void SkaleHost::createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
    uint64_t _timeStamp, uint64_t _blockID ) try {
    // convert bytes back to transactions (using caching), delete them from q and push results to
    // another q
    std::vector< Transaction > out_txns;  // resultant Transaction vector
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
            assert( !m_broadcastedQueue.contains( t ) );

            //            t.setNonce(t.nonce()-1);

            out_txns.push_back( t );
            m_tq.dropGood( t );
            std::lock_guard< std::mutex > localGuard( m_localMutex );
            MICROPROFILE_SCOPEI( "SkaleHost", "erase from caches", MP_GAINSBORO );
            m_transaction_cache.erase( sha.asArray() );
            m_received.erase( sha );

            LOG( m_debugLogger ) << "m_received = " << m_received.size() << std::endl;
        }
        // if new
        else {
            try {
                Transaction t( data, CheckTransaction::Everything, true );
                out_txns.push_back( t );
                LOG( m_debugLogger ) << "Will import consensus-born txn!";
            } catch ( const exception& ex ) {
                penalizePeer();
                clog( VerbosityWarning, "skale-host" )
                    << "Dropped consensus-born txn!" << ex.what();
            }  // catch
        }      // else

        if ( m_tq.knownTransactions().count( sha ) != 0 ) {
            // TODO fix this!!?
            clog( VerbosityWarning, "skale-host" )
                << "Consensus returned 'future'' transaction that we didn't yet send!!";
        }

    }  // for
    // TODO Monitor somehow m_transaction_cache and delete long-lasting elements?

    total_arrived += out_txns.size();

    assert( _blockID == m_client.number() + 1 );

    size_t n_succeeded = m_client.syncTransactions( out_txns, _timeStamp );
    if ( n_succeeded != out_txns.size() )
        penalizePeer();

    m_client.sealUnconditionally( false );
    m_client.importWorkingBlock();

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
        m_broadcastedQueue.abortWaiting();
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
    m_broadcastedQueue.abortWaiting();
    m_consensus->exitGracefully();
    m_consensusThread.join();

    m_broadcastThread.join();

    working = false;
}

void SkaleHost::broadcastFunc() {
    dev::setThreadName( "broadcastFunc" );
    while ( !m_exitNeeded ) {
        try {
            dev::eth::Transactions txns = m_tq.topTransactionsSync( 1, 0, 1 );
            if ( txns.empty() )  // means timeout
                continue;

            this->logState();

            MICROPROFILE_SCOPEI( "SkaleHost", "broadcastFunc", MP_BISQUE );

            assert( txns.size() == 1 );
            Transaction& txn = txns[0];
            h256 sha = txn.sha3();

            m_localMutex.lock();
            size_t received = m_received.count( sha );
            m_localMutex.unlock();

            if ( received == 0 ) {
                try {
                    if ( !m_broadcastPauseFlag )
                        m_broadcaster->broadcast( toJS( txn.rlp() ) );
                } catch ( const std::exception& ex ) {
                    cwarn << "BROADCAST EXCEPTION CAUGHT" << endl;
                    cwarn << ex.what() << endl;
                }  // catch

            }  // if

            // TODO some time this should become asynchronous
            m_broadcastedQueue.push( txn );

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
