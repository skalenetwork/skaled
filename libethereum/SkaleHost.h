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
 * @file SkaleHost.h
 * @author Dima Litvinov
 * @date 2018
 */

#pragma once

#include "ConsensusStub.h"

#include <libskale/broadcaster.h>

#include <libconsensus/node/ConsensusInterface.h>
#include <libdevcore/Common.h>
#include <libdevcore/HashingThreadSafeQueue.h>
#include <libdevcore/Log.h>
#include <libdevcore/Worker.h>
#include <libethcore/ChainOperationParams.h>
#include <libethcore/Common.h>
#include <libethereum/Transaction.h>
#include <libskale/SkaleClient.h>

#include <jsonrpccpp/client/client.h>

#include <map>
#include <memory>
#include <queue>

namespace dev {
namespace eth {
class Client;
class TransactionQueue;
class BlockHeader;
}  // namespace eth
}  // namespace dev

struct tx_hash_small {
    size_t operator()( const dev::eth::Transaction& t ) const {
        const dev::h256& h = t.sha3();
        return boost::hash_range( h.begin(), h.end() );
    }
};

class ConsensusFactory {
public:
    virtual std::unique_ptr< ConsensusInterface > create( ConsensusExtFace& _extFace ) const = 0;
    virtual ~ConsensusFactory() = default;
};

class DefaultConsensusFactory : public ConsensusFactory {
public:
    DefaultConsensusFactory( const dev::eth::Client& _client, const string& _blsPrivateKey = "",
        const string& _blsPublicKey1 = "", const string& _blsPublicKey2 = "",
        const string& _blsPublicKey3 = "", const string& _blsPublicKey4 = "" )
        : m_client( _client ),
          m_blsPrivateKey( _blsPrivateKey ),
          m_blsPublicKey1( _blsPublicKey1 ),
          m_blsPublicKey2( _blsPublicKey2 ),
          m_blsPublicKey3( _blsPublicKey3 ),
          m_blsPublicKey4( _blsPublicKey4 ) {}
    virtual std::unique_ptr< ConsensusInterface > create( ConsensusExtFace& _extFace ) const;

private:
    const dev::eth::Client& m_client;
    std::string m_blsPrivateKey, m_blsPublicKey1, m_blsPublicKey2, m_blsPublicKey3, m_blsPublicKey4;
};

class SkaleHost {
    friend class ConsensusExtImpl;

    struct my_hash {
        size_t operator()( const dev::eth::Transaction& tx ) const { return hash( tx.sha3() ); }

    private:
        dev::FixedHash< 32 >::hash hash;
    };

    template < class T >
    struct no_hash {
        size_t operator()( const T& ) const { return 0; }
    };

public:
    class CreationException : public std::exception {
        virtual const char* what() const noexcept { return "Error creating SkaleHost"; }
    };

    SkaleHost( dev::eth::Client& _client, const ConsensusFactory* _consFactory = nullptr );
    virtual ~SkaleHost();

    void startWorking();
    void stopWorking();

    void noteNewTransactions();
    void noteNewBlocks();
    void onBlockImported( dev::eth::BlockHeader const& _info );

    dev::h256 receiveTransaction( std::string );

    void pauseConsensus( bool _pause ) {
        if ( _pause )
            m_consensusPauseMutex.lock();
        else
            m_consensusPauseMutex.unlock();
    }
    void pauseBroadcast( bool _pause ) { m_broadcastPauseFlag = _pause; }

private:
    bool working = false;

    std::unique_ptr< Broadcaster > m_broadcaster;

private:
    virtual ConsensusExtFace::transactions_vector pendingTransactions( size_t _limit );
    virtual void createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
        uint64_t _timeStamp, uint64_t _blockID );

    std::thread m_broadcastThread;
    void broadcastFunc();
    dev::h256Hash m_received;
    std::mutex m_receivedMutex;

    int m_bcast_counter = 0;

    void penalizePeer(){};  // fake function for now

    int64_t m_lastBlockWithBornTransactions = -1;  // to track txns need re-verification

    std::thread m_consensusThread;

    bool m_exitNeeded = false;

    std::mutex m_consensusPauseMutex;
    bool m_broadcastPauseFlag;  // not pause - just ignore

    std::map< array< uint8_t, 32 >, dev::eth::Transaction > m_transaction_cache;  // used to find
                                                                                  // Transaction
                                                                                  // objects when
                                                                                  // creating block

    dev::eth::Client& m_client;
    dev::eth::TransactionQueue& m_tq;  // transactions ready to go to consensus

    dev::Logger m_debugLogger{dev::createLogger( dev::VerbosityDebug, "skale-host" )};
    dev::Logger m_traceLogger{dev::createLogger( dev::VerbosityTrace, "skale-host" )};
    void logState();

    std::unique_ptr< ConsensusExtFace > m_extFace;
    std::unique_ptr< ConsensusInterface > m_consensus;

#ifdef DEBUG_TX_BALANCE
    std::map< dev::h256, int > sent;
    std::set< dev::h256 > arrived;
#endif
    int total_sent, total_arrived;
};
