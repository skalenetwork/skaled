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

#include <libskale/SkaleDebug.h>
#include <libskale/broadcaster.h>

#include <libdevcore/Common.h>
#include <libdevcore/HashingThreadSafeQueue.h>
#include <libdevcore/Log.h>
#include <libdevcore/Worker.h>
#include <libethcore/ChainOperationParams.h>
#include <libethcore/Common.h>
#include <libethereum/InstanceMonitor.h>
#include <libethereum/Transaction.h>
#include <libskale/SkaleClient.h>

#include <jsonrpccpp/client/client.h>
#include <boost/chrono.hpp>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace dev {
namespace eth {
struct SyncStatus;
class Client;
class TransactionQueue;
class BlockHeader;
}  // namespace eth
}  // namespace dev

class ConsensusEngine;

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
    DefaultConsensusFactory( const dev::eth::Client& _client ) : m_client( _client ) {}
    virtual std::unique_ptr< ConsensusInterface > create( ConsensusExtFace& _extFace ) const;

private:
    const dev::eth::Client& m_client;
#if CONSENSUS
    void fillSgxInfo( ConsensusEngine& consensus ) const;
    void fillPublicKeyInfo( ConsensusEngine& consensus ) const;
    void fillRotationHistory( ConsensusEngine& consensus ) const;
#endif
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

    SkaleHost( dev::eth::Client& _client, const ConsensusFactory* _consFactory = nullptr,
        std::shared_ptr< InstanceMonitor > _instanceMonitor = nullptr,
        const std::string& _gethURL = "", bool _broadcastEnabled = true );
    virtual ~SkaleHost();

    void startWorking();
    void stopWorking();
    bool isWorking() const { return this->working; }

    void noteNewTransactions();
    void noteNewBlocks();
    void onBlockImported( dev::eth::BlockHeader const& _info );

    dev::h256 receiveTransaction( std::string );

    dev::u256 getGasPrice( unsigned _blockNumber = dev::eth::LatestBlock ) const;
    dev::u256 getBlockRandom() const;
    dev::eth::SyncStatus syncStatus() const;
    std::map< std::string, uint64_t > getConsensusDbUsage() const;
    std::array< std::string, 4 > getIMABLSPublicKey() const;

    // get node id for historic node in chain
    std::string getHistoricNodeId( unsigned _id ) const;

    // get schain index for historic node in chain
    std::string getHistoricNodeIndex( unsigned _idx ) const;

    // get public key for historic node in chain
    std::string getHistoricNodePublicKey( unsigned _idx ) const;

    uint64_t submitOracleRequest( const string& _spec, string& _receipt, string& _errorMessage );
    uint64_t checkOracleResult( const string& _receipt, string& _result );

    void pauseConsensus( bool _pause ) {
        if ( _pause && !m_consensusPaused ) {
            m_consensusPaused = true;
            m_consensusPauseMutex.lock();
        } else if ( !_pause && m_consensusPaused ) {
            m_consensusPaused = false;
            m_consensusPauseMutex.unlock();
        }
        // else do nothing
    }
    void pauseBroadcast( bool _pause ) { m_broadcastPauseFlag = _pause; }

    void forceEmptyBlock();

    void forcedBroadcast( const dev::eth::Transaction& _txn );

    SkaleDebugInterface::handler getDebugHandler() const { return m_debugHandler; }

private:
    std::atomic_bool working = false;

    std::unique_ptr< Broadcaster > m_broadcaster;

private:
    virtual ConsensusExtFace::transactions_vector pendingTransactions(
        size_t _limit, u256& _stateRoot );
    virtual void createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
        uint64_t _timeStamp, uint64_t _blockID, dev::u256 _gasPrice, u256 _stateRoot,
        uint64_t _winningNodeIndex );

    std::thread m_broadcastThread;
    void broadcastFunc();
    dev::h256Hash m_received;
    std::mutex m_receivedMutex;

    // TODO implement more nicely and more fine-grained!
    std::recursive_mutex m_pending_createMutex;  // for race conditions between
                                                 // pendingTransactions() and createBock()

    std::atomic_int m_bcast_counter = 0;

    void penalizePeer(){};  // fake function for now

    int64_t m_lastBlockWithBornTransactions = -1;  // to track txns need re-verification

    std::thread m_consensusThread;

    std::atomic_bool m_exitNeeded = false;

    std::mutex m_consensusPauseMutex;
    std::atomic_bool m_consensusPaused = false;
    std::atomic_bool m_broadcastPauseFlag = false;  // not pause - just ignore

    std::map< std::array< uint8_t, 32 >, dev::eth::Transaction >
        m_m_transaction_cache;  // used to find Transaction objects when
                                // creating block
    dev::eth::Client& m_client;
    dev::eth::TransactionQueue& m_tq;  // transactions ready to go to consensus

    std::shared_ptr< InstanceMonitor > m_instanceMonitor;
    std::atomic_bool m_ignoreNewBlocks = false;  // used when we need to exit at specific block

    bool m_broadcastEnabled;


    dev::Logger m_errorLogger{ dev::createLogger( dev::VerbosityError, "skale-host" ) };
    dev::Logger m_warningLogger{ dev::createLogger( dev::VerbosityWarning, "skale-host" ) };
    dev::Logger m_infoLogger{ dev::createLogger( dev::VerbosityInfo, "skale-host" ) };
    dev::Logger m_debugLogger{ dev::createLogger( dev::VerbosityDebug, "skale-host" ) };
    dev::Logger m_traceLogger{ dev::createLogger( dev::VerbosityTrace, "skale-host" ) };
    void logState();

    std::unique_ptr< ConsensusExtFace > m_extFace;
    std::unique_ptr< ConsensusInterface > m_consensus;

    std::optional< uint64_t > emptyBlockIntervalMsForRestore;  // used for temporary setting this
                                                               // to 0
    bool need_restore_emptyBlockInterval = false;

    SkaleDebugTracer m_debugTracer;
    SkaleDebugInterface::handler m_debugHandler;

#ifdef DEBUG_TX_BALANCE
    std::map< dev::h256, int > sent;
    std::set< dev::h256 > arrived;
#endif
    std::atomic_int total_sent, total_arrived;

    boost::chrono::high_resolution_clock::time_point latestBlockTime;

    // reject old transactions that come through broadcast
    // if current ts is much bigger than currentBlock.ts
    static const int REJECT_OLD_TRANSACTION_THROUGH_BROADCAST_INTERVAL_SEC;
};
