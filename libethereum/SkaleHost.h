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
    SkaleHost( dev::eth::Client& _client, dev::eth::TransactionQueue& _tq );
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
    std::unique_ptr< Broadcaster > m_broadcaster;

private:
    virtual ConsensusExtFace::transactions_vector pendingTransactions( size_t _limit );
    virtual void createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
        uint64_t _timeStamp, uint64_t _blockID );

    std::thread m_broadcastThread;
    void broadcastFunc();
    dev::h256Hash m_received;

    //    dev::h256Hash m_broadcastedHash;
    HashingThreadSafeQueue< dev::eth::Transaction, tx_hash_small, true > m_broadcastedQueue;

    std::thread m_blockImportThread;
    void blockImportFunc();

    std::thread m_consensusThread;

    std::mutex m_localMutex;  // used to protect local caches/hashes; TODO rethink multithreading!!
    bool m_exitNeeded = false;

    std::mutex m_consensusPauseMutex;
    bool m_broadcastPauseFlag;  // not pause - just ignore

    std::map< array< uint8_t, 32 >, dev::eth::Transaction > m_transaction_cache;  // used to find
                                                                                  // Transaction
                                                                                  // objects when
                                                                                  // creating block

    dev::eth::Client& m_client;
    dev::eth::TransactionQueue& m_tq;  // transactions ready to go to consensus

    struct bq_item {
        std::vector< dev::eth::Transaction > transactions;
        uint64_t timestamp;
        uint64_t block_id;
        // TODO This should not be needed!
        bool operator==( const bq_item& rhs ) const {
            return transactions == rhs.transactions && timestamp == rhs.timestamp &&
                   block_id == rhs.block_id;
        }
    };
    HashingThreadSafeQueue< bq_item, no_hash< bq_item >, false > m_bq;  // blocks returned by
                                                                        // consensus (for
                                                                        // thread-safety see
                                                                        // below!!)

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
