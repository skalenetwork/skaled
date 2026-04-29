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
#include <chrono>
#include <future>
#include <string>

using namespace std;

#include <libconsensus/node/ConsensusEngine.h>

#include <libskale/AmsterdamFixPatch.h>

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
#include <skutils/utils.h>

using namespace dev;
using namespace dev::eth;

#ifndef CONSENSUS
#define CONSENSUS 1
#endif

const int SkaleHost::REJECT_OLD_TRANSACTION_THROUGH_BROADCAST_INTERVAL_SEC = 600;

std::unique_ptr< ConsensusInterface > DefaultConsensusFactory::create(
    ConsensusExtFace& _extFace ) const {
#if CONSENSUS
    const auto& nfo = static_cast< const Interface& >( m_client ).blockInfo( LatestBlock );

    BOOST_LOG( m_loggerInfo ) << "NOTE: Block number at startup is " << nfo.number();

    auto ts = nfo.timestamp();

    std::map< std::string, std::uint64_t > patchTimeStamps;

#ifndef FAIR
    patchTimeStamps["verifyDaSigsPatchTimestamp"] =
        m_client.chainParams().getPatchTimestamp( SchainPatchEnum::VerifyDaSigsPatch );
    patchTimeStamps["fastConsensusPatchTimestamp"] =
        m_client.chainParams().getPatchTimestamp( SchainPatchEnum::FastConsensusPatch );
    patchTimeStamps["verifyBlsSyncPatchTimestamp"] =
        m_client.chainParams().getPatchTimestamp( SchainPatchEnum::VerifyBlsSyncPatch );
#endif  // FAIR

    auto consensusEnginePtr = make_unique< ConsensusEngine >( _extFace, m_client.number(), ts, 0,
        patchTimeStamps, m_client.chainParams().getConsensusStorageLimit() );

    if ( !m_client.chainParams().isSyncNode() &&
         !m_client.chainParams().getSgxServerUrl().empty() ) {
        this->fillSgxInfo( *consensusEnginePtr );
    }

    this->fillPublicKeyInfo( *consensusEnginePtr );

    this->fillRotationHistory( *consensusEnginePtr );

#ifdef BITE
    consensusEnginePtr->setEpochId( m_client.getCurrentEpochId() );
#endif
    return consensusEnginePtr;
#else
    unsigned block_number = m_client.number();
    dev::h256 state_root =
        m_client.blockInfo( m_client.hashFromNumber( block_number ) ).stateRoot();
    return make_unique< ConsensusStub >( _extFace, block_number, state_root );
#endif  // CONSENSUS
}

#if CONSENSUS
void DefaultConsensusFactory::fillSgxInfo( ConsensusEngine& consensus ) const try {
    const std::string sgxServerUrl = m_client.chainParams().getSgxServerUrl();

    std::string sgx_cert_path = getenv( "SGX_CERT_FOLDER" ) ? getenv( "SGX_CERT_FOLDER" ) : "";
    if ( sgx_cert_path.empty() )
        sgx_cert_path = "/skale_node_data/sgx_certs/";
    else if ( sgx_cert_path[sgx_cert_path.length() - 1] != '/' )
        sgx_cert_path += '/';

    const char* sgx_cert_filename = getenv( "SGX_CERT_FILE" );
    if ( sgx_cert_filename == nullptr )
        sgx_cert_filename = "sgx.crt";

    const char* sgx_key_filename = getenv( "SGX_KEY_FILE" );
    if ( sgx_key_filename == nullptr )
        sgx_key_filename = "sgx.key";

    std::string sgxSSLKeyFilePath;
    std::string sgxSSLCertFilePath;
    // if https
    if ( sgxServerUrl.find( ':' ) == 5 ) {
        sgxSSLKeyFilePath = sgx_cert_path + sgx_key_filename;
        sgxSSLCertFilePath = sgx_cert_path + sgx_cert_filename;
    }

    std::string ecdsaKeyName = m_client.chainParams().getEcdsaKeyName();

    std::string blsKeyName = m_client.chainParams().getKeyShareName();

    consensus.setSGXKeyInfo(
        sgxServerUrl, sgxSSLKeyFilePath, sgxSSLCertFilePath, ecdsaKeyName, blsKeyName );


} catch ( const std::exception& ex ) {
    std::throw_with_nested(
        std::runtime_error( std::string( "Error filling SGX info (nodeGroups): " ) + ex.what() ) );
} catch ( ... ) {
    std::throw_with_nested( std::runtime_error( "Error filling SGX info (nodeGroups)" ) );
}

void DefaultConsensusFactory::fillPublicKeyInfo( ConsensusEngine& consensus ) const try {
    if ( m_client.chainParams().isTestSignaturesEnabled() )
        // no keys in tests
        return;

    const std::string sgxServerUrl = m_client.chainParams().getSgxServerUrl();

    std::shared_ptr< std::vector< std::string > > ecdsaPublicKeys =
        std::make_shared< std::vector< std::string > >();
    for ( const auto& node : m_client.chainParams().getSchainNodes() ) {
        ecdsaPublicKeys->push_back( node.publicKey.substr( 2 ) );
    }

    std::vector< std::shared_ptr< std::vector< std::string > > > blsPublicKeys;
    for ( const auto& node : m_client.chainParams().getSchainNodes() ) {
#ifdef FAIR
        std::vector< std::string > public_key_share(
            node.blsPublicKey.begin(), node.blsPublicKey.end() );
#else
        std::vector< std::string > public_key_share( 4 );
        if ( node.id != m_client.chainParams().getSelfNodeId() ) {
            public_key_share[0] = node.blsPublicKey.at( 0 );
            public_key_share[1] = node.blsPublicKey.at( 1 );
            public_key_share[2] = node.blsPublicKey.at( 2 );
            public_key_share[3] = node.blsPublicKey.at( 3 );
        } else {
            auto blsPublicKey = m_client.chainParams().getSelfBlsPublicKey();
            public_key_share[0] = blsPublicKey.at( 0 );
            public_key_share[1] = blsPublicKey.at( 1 );
            public_key_share[2] = blsPublicKey.at( 2 );
            public_key_share[3] = blsPublicKey.at( 3 );
        }
#endif  // FAIR

        blsPublicKeys.push_back(
            std::make_shared< std::vector< std::string > >( public_key_share ) );
    }

    auto blsPublicKeysPtr =
        std::make_shared< std::vector< std::shared_ptr< std::vector< std::string > > > >(
            blsPublicKeys );

    size_t n = m_client.chainParams().getNodesCount();
    size_t t = ( 2 * n + 1 ) / 3;

    consensus.setPublicKeyInfo(
        ecdsaPublicKeys, blsPublicKeysPtr, t, n, m_client.chainParams().isSyncNode() );
} catch ( const std::exception& ex ) {
    std::throw_with_nested( std::runtime_error(
        std::string( "Error filling public keys info (nodeGroups): " ) + ex.what() ) );
} catch ( ... ) {
    std::throw_with_nested( std::runtime_error( "Error filling public keys info (nodeGroups)" ) );
}


void DefaultConsensusFactory::fillRotationHistory( ConsensusEngine& consensus ) const try {
    std::map< uint64_t, std::vector< std::string > > previousBLSKeys;
    std::map< uint64_t, std::string > historicECDSAKeys;
    std::map< uint64_t, std::vector< uint64_t > > historicNodeGroups;
    auto u256toUint64 = []( const dev::u256& u ) { return std::stoull( u.str() ); };
    for ( const auto& nodeGroup : m_client.chainParams().getNodeGroups() ) {
        std::vector< string > commonBLSPublicKey = { nodeGroup.blsPublicKey.at( 0 ),
            nodeGroup.blsPublicKey.at( 1 ), nodeGroup.blsPublicKey.at( 2 ),
            nodeGroup.blsPublicKey.at( 3 ) };
        previousBLSKeys[nodeGroup.finishTs] = commonBLSPublicKey;
        std::vector< uint64_t > nodes;
        // add ecdsa keys info and historic groups info
        for ( const auto& node : nodeGroup.nodes ) {
            historicECDSAKeys[u256toUint64( node.id )] = node.publicKey.substr( 2 );
            nodes.push_back( u256toUint64( node.id ) );
        }
        historicNodeGroups[nodeGroup.finishTs] = nodes;
    }
    consensus.setRotationHistory(
        std::make_shared< std::map< uint64_t, std::vector< std::string > > >( previousBLSKeys ),
        std::make_shared< std::map< uint64_t, std::string > >( historicECDSAKeys ),
        std::make_shared< std::map< uint64_t, std::vector< uint64_t > > >( historicNodeGroups ) );
} catch ( const std::exception& ex ) {
    std::throw_with_nested( std::runtime_error(
        std::string( "Error reading rotation history (nodeGroups): " ) + ex.what() ) );
} catch ( ... ) {
    std::throw_with_nested( std::runtime_error( "Error reading rotation history (nodeGroups)" ) );
}

#endif  // CONSENSUS

class ConsensusExtImpl : public ConsensusExtFace {
public:
    ConsensusExtImpl( SkaleHost& _host );
    virtual transactions_vector pendingTransactions( size_t _limit, u256& _stateRoot ) override;
    virtual void createBlock( const transactions_vector& _approvedTransactions,
#ifdef BITE
        shared_ptr< DecryptedTransactionFieldsMap > _decryptedTransactionFields,
#endif
        uint64_t _timeStamp, uint32_t _timeStampMs, uint64_t _blockID, u256 _gasPrice,
        u256 _stateRoot, uint64_t _winningNodeIndex ) override;
    virtual void terminateApplication() override;
    virtual ~ConsensusExtImpl() override = default;

private:
    SkaleHost& m_host;
};

ConsensusExtImpl::ConsensusExtImpl( SkaleHost& _host ) : m_host( _host ) {}

ConsensusExtFace::transactions_vector ConsensusExtImpl::pendingTransactions(
    size_t _limit, u256& _stateRoot ) {
    auto ret = m_host.pendingTransactions( _limit, _stateRoot );
    return ret;
}

void ConsensusExtImpl::createBlock(
    const ConsensusExtFace::transactions_vector& _approvedTransactions,
#ifdef BITE
    shared_ptr< DecryptedTransactionFieldsMap > _decryptedTransactionFields,
#endif
    uint64_t _timeStamp, uint32_t, uint64_t _blockID, u256 _gasPrice, u256 _stateRoot,
    uint64_t _winningNodeIndex ) {
    MICROPROFILE_SCOPEI( "ConsensusExtFace", "createBlock", MP_INDIANRED );
    m_host.createBlock( _approvedTransactions,
#ifdef BITE
        _decryptedTransactionFields,
#endif
        _timeStamp, _blockID, _gasPrice, _stateRoot, _winningNodeIndex );
}

void ConsensusExtImpl::terminateApplication() {
    dev::ExitHandler::exitHandler( -1, dev::ExitHandler::ec_consensus_terminate_request );
}

SkaleHost::SkaleHost( dev::eth::Client& _client, const ConsensusFactory* _consFactory,
    std::shared_ptr< InstanceMonitor > _instanceMonitor,
#ifndef FAIR
    const std::string& _gethURL,
#endif
    [[maybe_unused]] bool _broadcastEnabled )
    : m_client( _client ),
      m_tq( _client.m_tq ),
      m_instanceMonitor( _instanceMonitor ),
      total_sent( 0 ),
      total_arrived( 0 ),
      latestBlockTime( boost::chrono::high_resolution_clock::time_point() ) {
    try {
        m_debugHandler = [this]( const std::string& arg ) -> std::string {
            return DebugTracer_handler( arg, this->m_debugTracer );
        };

        m_debugTracer.call_on_tracepoint( [this]( const std::string& name ) {
            static uint64_t last_block_when_log = -1;
            if ( name == "fetch_transactions" || name == "drop_bad_transactions" ) {
                uint64_t current_block = this->m_client.number();
                if ( current_block == last_block_when_log )
                    return;
                if ( name == "drop_bad_transactions" )
                    last_block_when_log = current_block;
            }

            BOOST_LOG( m_loggerTrace )
                << "TRACEPOINT " << name << " " << m_debugTracer.get_tracepoint_count( name );
        } );

        m_broadcaster.reset( new ZmqBroadcaster( _client, *this ) );

        m_extFace.reset( new ConsensusExtImpl( *this ) );

    } catch ( const std::exception& e ) {
        BOOST_LOG( m_loggerError ) << "Could not init SkaleHost" << e.what();
        std::throw_with_nested( CreationException() );
    }

    try {
        // set up consensus
        // XXX
        if ( !_consFactory )
            m_consensus = DefaultConsensusFactory( m_client ).create( *m_extFace );
        else
            m_consensus = _consFactory->create( *m_extFace );
    } catch ( const std::exception& e ) {
        BOOST_LOG( m_loggerError ) << "Could not create consensus in SkaleHost" << e.what();
        std::throw_with_nested( CreationException() );
    }

    try {
#ifdef FAIR
        m_consensus->parseFullConfigAndCreateNode(
            m_client.chainParams().getConfigForConsensus(), "" );
#else
        m_consensus->parseFullConfigAndCreateNode(
            m_client.chainParams().getOriginalJson(), _gethURL );
#endif
    } catch ( const std::exception& e ) {
        BOOST_LOG( m_loggerError )
            << "Could not create parse consensus config in SkaleHost" << e.what();
        std::throw_with_nested( CreationException() );
    }
}

SkaleHost::~SkaleHost() {}

void SkaleHost::logState() {
    BOOST_LOG( m_loggerTrace ) << " sent_to_consensus = " << total_sent
                               << " got_from_consensus = " << total_arrived
                               << " m_tq = " << m_tq.status().current
                               << " m_bcast_counter = " << m_bcast_counter;
}

constexpr uint64_t MAX_BROADCAST_QUEUE_SIZE = 2048;

void SkaleHost::pushToBroadcastQueue( const Transaction& _t ) {
    {
        std::lock_guard< std::mutex > lock( m_broadcastQueueMutex );
        this->m_broadcastQueue.push_back( _t );
        // normally broadcast queue will never be large since
        // it is an intermediate queue on the way to zeromq
        // and zeromq writes do not block
        // we still keep its size limited
        while ( m_broadcastQueue.size() > MAX_BROADCAST_QUEUE_SIZE ) {
            // behavior on overflow similar to ZeroMQ - erase the latest
            m_broadcastQueue.erase( m_broadcastQueue.begin() );
        }
    }
    m_broadcastQueueCondition.notify_all();  // Notify the condition variable
}

#ifdef BITE2
void SkaleHost::pushToBITE2Queue( dev::eth::Transaction&& _transaction ) {
    m_tq.importBITE2Transaction( std::move( _transaction ) );
}
#endif

h256 SkaleHost::receiveTransaction( std::string _rlp ) {
    // drop incoming transactions if skaled has an outdated state
    if ( m_client.bc().info().timestamp() + REJECT_OLD_TRANSACTION_THROUGH_BROADCAST_INTERVAL_SEC <
         std::time( NULL ) ) {
        BOOST_LOG( m_loggerDebug ) << "Dropped the transaction received through broadcast";
        return h256();
    }

    Transaction transaction( jsToBytes( _rlp, OnFailed::Throw ), CheckTransaction::None, false,
        EIP1559TransactionsPatch::isEnabledInWorkingBlock(),
        InvalidTransactionFormatPatch::isEnabledInWorkingBlock() );
    h256 sha = transaction.sha3();

    //
    m_debugTracer.tracepoint( "receive_transaction" );

#if ( defined _DEBUG )
    h256 sha2 =
#endif
        m_client.importTransaction( transaction, TransactionBroadcast::DontBroadcast );
#if ( defined _DEBUG )
    assert( sha == sha2 );
#endif

    m_debugTracer.tracepoint( "receive_transaction_success" );
    BOOST_LOG( m_loggerDebug ) << "Successfully received through broadcast " << sha;


    return sha;
}

// keeps mutex unlocked when exists
template < class M >
class unlock_guard {
private:
    M& mutex_ref;
    std::atomic_bool m_will_exit = false;

public:
    explicit unlock_guard( M& m ) : mutex_ref( m ) { mutex_ref.unlock(); }
    ~unlock_guard() {
        if ( !m_will_exit )
            mutex_ref.lock();
    }
    void will_exit() { m_will_exit = true; }
};

ConsensusExtFace::transactions_vector SkaleHost::pendingTransactions(
    size_t _limit, u256& _stateRoot ) {
    assert( _limit > 0 );
    assert( _limit <= numeric_limits< unsigned int >::max() );

    ConsensusExtFace::transactions_vector out_vector;

    if ( m_exitNeeded )
        return out_vector;

    if ( m_exitNeeded )
        return out_vector;

    std::lock_guard< std::mutex > pauseLock( m_consensusPauseMutex );

    if ( m_exitNeeded )
        return out_vector;

    if ( m_exitNeeded )
        return out_vector;

    if ( need_restore_emptyBlockInterval ) {
        this->m_consensus->setEmptyBlockIntervalMs( this->emptyBlockIntervalMsForRestore.value() );
        this->emptyBlockIntervalMsForRestore.reset();
        need_restore_emptyBlockInterval = false;
    }

    MICROPROFILE_SCOPEI( "SkaleHost", "pendingTransactions", MP_LAWNGREEN );


    _stateRoot = dev::h256::Arith( m_client.getReadOnlyLatestBlockCopy().info().stateRoot() );

    h256Hash to_delete;

    m_debugTracer.tracepoint( "fetch_transactions" );

    int counter = 0;
    BlockHeader latestInfo = static_cast< const Interface& >( m_client ).blockInfo( LatestBlock );

    Transactions txns = m_tq.topTransactionsSync(
        _limit, [this, &to_delete, &counter, &latestInfo]( const Transaction& tx ) -> bool {
            // XXX TODO Invent good way to do this
            if ( counter++ == 0 )
                m_pending_createMutex.lock();

            try {
                bool isMtmEnabled = m_client.chainParams().isMultiTransactionModeEnabled();
                Executive::verifyTransaction( tx, latestInfo.timestamp(), latestInfo,
                    m_client.state().createReadOnlySnapBasedCopy(), m_client.chainParams(), 0,
                    getGasPrice(), isMtmEnabled );
            } catch ( const exception& ex ) {
                if ( to_delete.count( tx.sha3() ) == 0 )
                    BOOST_LOG( m_loggerInfo ) << "Dropped now-invalid transaction in pending queue "
                                              << tx.sha3() << ":" << ex.what();
                to_delete.insert( tx.sha3() );
                return false;
            }
            return true;
        } );

    // now we need to delete old transactions from the queue
    m_tq.dropMany( to_delete );

    if ( counter++ == 0 )
        m_pending_createMutex.lock();

    std::lock_guard< std::recursive_mutex > lock( m_pending_createMutex, std::adopt_lock );

    // drop by block gas limit
    u256 blockGasLimit = this->m_client.chainParams().getGasLimit();
    u256 gasAcc = 0;
    auto first_to_drop_it = txns.begin();
    for ( ; first_to_drop_it != txns.end(); ++first_to_drop_it ) {
        gasAcc += first_to_drop_it->gas();
        if ( gasAcc > blockGasLimit )
            break;
    }  // for
    txns.erase( first_to_drop_it, txns.end() );

    m_debugTracer.tracepoint( "drop_bad_transactions" );

    if ( this->emptyBlockIntervalMsForRestore.has_value() )
        need_restore_emptyBlockInterval = true;

    if ( txns.size() == 0 )
        return out_vector;  // time-out with 0 results

    try {
        for ( size_t i = 0; i < txns.size(); ++i ) {
            Transaction& txn = txns[i];

            h256 sha = txn.sha3();

            out_vector.push_back( txn.toBytes() );

            ++total_sent;

#ifdef DEBUG_TX_BALANCE
            if ( sent.count( sha ) != 0 ) {
                int prev = sent[sha];
                BOOST_LOG( m_loggerError ) << "Prev no = " << prev;

                if ( sent.count( sha ) != 0 ) {
                    // TODO fix this!!?
                    BOOST_LOG( m_loggerWarning )
                        << "Sending to consensus duplicate transaction (sent before!)";
                }
            }
            sent[sha] = total_sent + i;
#endif

            m_debugTracer.tracepoint( "sent_txn" );
            BOOST_LOG( m_loggerTrace ) << "Sent txn: " << sha;
        }
    } catch ( ... ) {
        BOOST_LOG( m_loggerError ) << "BAD exception in pendingTransactions!";
    }

    m_debugTracer.tracepoint( "send_to_consensus" );

    return out_vector;
}

void SkaleHost::checkStateRoot( uint64_t _blockID, uint64_t _winningNodeIndex, u256 _stateRoot ) {
    dev::h256 stCurrent =
        this->m_client.blockInfo( this->m_client.hashFromNumber( _blockID - 1 ) ).stateRoot();

    BOOST_LOG( m_loggerTrace ) << "STATE ROOT FOR BLOCK: " << std::to_string( _blockID - 1 ) << " "
                               << stCurrent.hex();

    // FATAL if mismatch in non-default
    if ( _winningNodeIndex != 0 && dev::h256::Arith( stCurrent ) != _stateRoot ) {
        BOOST_LOG( m_loggerError )
            << "FATAL STATE ROOT MISMATCH ERROR: current state root "
            << dev::h256::Arith( stCurrent ).str() << " is not equal to arrived state root "
            << _stateRoot.str() << " with block ID #" << _blockID
            << ", /data_dir cleanup is recommended, exiting with code "
            << int( ExitHandler::ec_state_root_mismatch ) << "...";
        if ( AmsterdamFixPatch::stateRootCheckingEnabled( m_client ) ) {
            m_ignoreNewBlocks = true;
            m_consensus->exitGracefully();
            ExitHandler::exitHandler( -1, ExitHandler::ec_state_root_mismatch );
        }
    }

    // WARN if default but non-zero
    if ( _winningNodeIndex == 0 && _stateRoot != u256() )
        BOOST_LOG( m_loggerWarning ) << "WARNING: STATE ROOT MISMATCH!"
                                     << "Current block is DEFAULT BUT arrived state root is "
                                     << _stateRoot.str() << " with block ID #" << _blockID;
}

void SkaleHost::createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
#ifdef BITE
    shared_ptr< DecryptedTransactionFieldsMap > _decryptedTransactionFields,
#endif
    uint64_t _timeStamp, uint64_t _blockID, u256 _gasPrice, u256 _stateRoot,
    uint64_t _winningNodeIndex ) try {
    boost::chrono::high_resolution_clock::time_point skaledTimeStart;
    skaledTimeStart = boost::chrono::high_resolution_clock::now();

    std::lock_guard< std::recursive_mutex > lock( m_pending_createMutex );

    if ( m_ignoreNewBlocks ) {
        BOOST_LOG( m_loggerWarning ) << "WARNING: skaled got new block #" << _blockID
                                     << " after timestamp-related exit initiated!";
        return;
    }

    BOOST_LOG( m_loggerDebug ) << "createBlock ID = #" << _blockID;
    m_debugTracer.tracepoint( "create_block" );

    // convert bytes back to transactions (using caching), delete them from q and push results into
    // blockchain

    if ( this->m_client.chainParams().getSnapshotIntervalSec() > 0 )
        checkStateRoot( _blockID, _winningNodeIndex, _stateRoot );

    std::vector< Transaction > out_txns;  // resultant Transaction vector

    size_t n_succeeded;

    BlockHeader latestInfo = static_cast< const Interface& >( m_client ).blockInfo( LatestBlock );

    DEV_GUARDED( m_client.m_blockImportMutex ) {
        m_debugTracer.tracepoint( "drop_good_transactions" );

#ifdef BITE
        auto decryptedTransactionFieldsIt = _decryptedTransactionFields->begin();
#endif
        for ( size_t i = 0; i < _approvedTransactions.size(); ++i ) {
            const bytes& data = _approvedTransactions.at( i );
            h256 sha = sha3( data );
            BOOST_LOG( m_loggerTrace ) << "Arrived txn: " << sha;

            Transaction t( data, CheckTransaction::Everything, true,
                EIP1559TransactionsPatch::isEnabledInWorkingBlock(),
                InvalidTransactionFormatPatch::isEnabledInWorkingBlock() );
#ifdef BITE
            if ( decryptedTransactionFieldsIt != _decryptedTransactionFields->end() &&
                 decryptedTransactionFieldsIt->first == i ) {
                DecryptedTransactionFields& txFields = decryptedTransactionFieldsIt->second;

                dev::Address to = dev::Address( txFields.to.get() );
                t.setDecryptedFields( txFields.data, std::make_shared< dev::Address >( to ) );
                ++decryptedTransactionFieldsIt;
            }

#endif

#ifndef FAIR
            t.checkOutExternalGas(
                m_client.chainParams(), latestInfo.timestamp(), m_client.number() );

            if ( !ExternalGasPatch::isEnabledWhen( latestInfo.timestamp() ) ) {
                auto hash = t.sha3();
                if ( m_client.m_tq.isTransactionKnown( hash ) ) {
                    // if a transaction is in the pending queue
                    // do checkOutExternal gas twice to repeat incorrect behavior that
                    // existed before the patch
                    t.checkOutExternalGas(
                        m_client.chainParams(), latestInfo.timestamp(), m_client.number() );
                }
            }
#endif
            out_txns.push_back( t );
            m_debugTracer.tracepoint( "drop_good" );
            m_tq.dropGood( t );
        }

        total_arrived += out_txns.size();

        if ( _blockID != m_client.number() + 1 ) {
            BOOST_LOG( m_loggerError )
                << "Mismatch in block number:SKALED_NUMBER:" << m_client.number()
                << ":CONSENSUS_NUMBER:" << _blockID;
            m_ignoreNewBlocks = true;
            m_consensus->exitGracefully();
            ExitHandler::exitHandler( -1, ExitHandler::ec_block_mismatch_with_consensus );
        }

        m_debugTracer.tracepoint( "import_block" );

        n_succeeded = m_client.importTransactionsAsBlock( out_txns,

#ifdef BITE
            _decryptedTransactionFields,
#endif
            _gasPrice,
#ifdef FAIR
            _winningNodeIndex,
#endif
            _timeStamp );
    }  // m_blockImportMutex

#ifdef FAIR
    syncNodeGroups();
#endif

    if ( n_succeeded != out_txns.size() )
        penalizePeer();


    auto skaledTimeFinish = boost::chrono::high_resolution_clock::now();


    auto swt = boost::chrono::duration_cast< boost::chrono::milliseconds >(
        skaledTimeFinish - skaledTimeStart )
                   .count();


    if ( latestBlockTime != boost::chrono::high_resolution_clock::time_point() ) {
        BOOST_LOG( m_loggerInfo ) << "SWT:" << swt << ':' << "BFT:"
                                  << boost::chrono::duration_cast< boost::chrono::milliseconds >(
                                         skaledTimeFinish - latestBlockTime )
                                         .count()
                                  << ":TQBYTES:CTQ:" << m_tq.status().currentBytes
                                  << ":FTQ:" << m_tq.status().futureBytes
                                  << ":TQSIZE:CTQ:" << m_tq.status().current
                                  << ":FTQ:" << m_tq.status().future;
    } else {
        BOOST_LOG( m_loggerInfo ) << "SWT:" << swt << ":TQBYTES:CTQ:" << m_tq.status().currentBytes
                                  << ":FTQ:" << m_tq.status().futureBytes
                                  << ":TQSIZE:CTQ:" << m_tq.status().current
                                  << ":FTQ:" << m_tq.status().future;
    }

    latestBlockTime = skaledTimeFinish;
    BOOST_LOG( m_loggerDebug ) << "Successfully imported " << n_succeeded << " of "
                               << out_txns.size() << " transactions";


    if ( m_instanceMonitor != nullptr ) {
        if ( m_instanceMonitor->isTimeToRotate( _timeStamp ) ) {
            m_instanceMonitor->prepareRotation();
            m_ignoreNewBlocks = true;
            m_consensus->exitGracefully();
            ExitHandler::exitHandler( -1, ExitHandler::ec_rotation_complete );
            BOOST_LOG( m_loggerInfo ) << "Rotation is completed. Instance is exiting";
        }
    }


} catch ( const std::exception& ex ) {
    BOOST_LOG( m_loggerError ) << "CRITICAL " << ex.what() << " (in createBlock)";
    BOOST_LOG( m_loggerError ) << "\n" << skutils::signal::generate_stack_trace();
} catch ( ... ) {
    BOOST_LOG( m_loggerError ) << "CRITICAL unknown exception (in createBlock)";
    BOOST_LOG( m_loggerError ) << "\n" << skutils::signal::generate_stack_trace();
}

#ifdef FAIR
void SkaleHost::runCommitteeRotationForConsensus() {
    if ( m_committeeRotationMonitorThread != nullptr &&
         m_committeeRotationMonitorThread->joinable() )
        m_committeeRotationMonitorThread->join();
    BOOST_LOG( m_loggerInfo ) << "Committee rotation is in progress.";
    m_ignoreNewBlocks = true;
    m_broadcastRestartNeeded = true;
    // stop all services first
    // exitGracefully() interferes with exit procedure
    // TODO: make it more elegant to avoid collisions
    m_consensus->exitGracefully();
    m_committeeRotationMonitorThread.reset( new std::thread( [this]() {
        while ( m_consensus->getStatus() != consensus_engine_status::CONSENSUS_EXITED ) {
            usleep( 100 * 1000 );  // sleep 100ms
        }
        BOOST_LOG( m_loggerDebug ) << "Committee rotation is completed. Creating consensus agents.";
        // reset ConsensusInterface to use relevant block numbers
        m_consensus = DefaultConsensusFactory( m_client ).create( *m_extFace );
        m_consensus->parseFullConfigAndCreateNode(
            m_client.chainParams().getConfigForConsensus(), "" );
        m_consensusUpdateHappened = true;
        // restart all services to fetch latest nodes info
        try {
            BOOST_LOG( m_loggerDebug ) << "Starting consensus after rotation";
            m_consensus->startAll();
        } catch ( const std::exception& ex ) {
            BOOST_LOG( m_loggerError )
                << "Exception occurred in startAll() after committee rotation: " << ex.what();
            // cleanup
            m_exitNeeded = true;
            m_broadcastThread.join();
            ExitHandler::exitHandler( -1, ExitHandler::ec_termninated_by_signal );
            return;
        } catch ( ... ) {
            BOOST_LOG( m_loggerError )
                << "Unknown exception in startAll() after committee rotation";
            // cleanup
            m_exitNeeded = true;
            m_broadcastThread.join();
            ExitHandler::exitHandler( -1, ExitHandler::ec_termninated_by_signal );
            return;
        }
        try {
            static const char g_strThreadName[] = "bootStrapAllAfterCommitteeRotation";
            dev::setThreadName( g_strThreadName );
            BOOST_LOG( m_loggerInfo ) << "Thread " << g_strThreadName << " started";
            m_consensus->bootStrapAll();
            BOOST_LOG( m_loggerInfo ) << "Thread " << g_strThreadName << " will exit";
        } catch ( std::exception& ex ) {
            std::string s = ex.what();
            if ( s.empty() )
                s = "no description";
            BOOST_LOG( m_loggerError )
                << "Consensus thread in skale host after committee rotation will "
                   "exit with exception: "
                << s;
        } catch ( ... ) {
            BOOST_LOG( m_loggerError )
                << "Consensus thread in skale host after committee rotation will "
                   "exit with unknown exception\n"
                << skutils::signal::generate_stack_trace();
        }
        m_ignoreNewBlocks = false;
        BOOST_LOG( m_loggerInfo ) << "Committee rotation is completed.";
    } ) );
}

void SkaleHost::handleConsensusUpdate() const {
    m_consensus->updateLogger();
    m_consensusUpdateHappened = false;
}

void SkaleHost::syncNodeGroups() {
    bool groupUpdated = m_client.updateGroupIfNeeded();
    const auto& nodeGroups = m_client.chainParams().getNodeGroups();
    if ( ( !nodeGroups.empty() && m_client.updateHistoricGroupIndex() ) || groupUpdated ) {
        runCommitteeRotationForConsensus();
    }
}
#endif

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
    } catch ( ... ) {
        working = false;
        std::throw_with_nested( std::runtime_error( "Error in starting broadcaster service" ) );
    }

    auto broadcastFunction = std::bind( &SkaleHost::broadcastFunc, this );
    m_broadcastThread = std::thread( broadcastFunction );

    auto consensusFunction = [&]() {
        try {
            m_consensus->startAll();
        } catch ( ... ) {
            // cleanup
            m_exitNeeded = true;
            m_broadcastThread.join();
            ExitHandler::exitHandler( -1, ExitHandler::ec_termninated_by_signal );
            return;
        }

        try {
            static const char g_strThreadName[] = "bootStrapAll";
            dev::setThreadName( g_strThreadName );
            BOOST_LOG( m_loggerInfo ) << "Thread " << g_strThreadName << " started";
            m_consensus->bootStrapAll();
            BOOST_LOG( m_loggerInfo ) << "Thread " << g_strThreadName << " will exit";
        } catch ( std::exception& ex ) {
            std::string s = ex.what();
            if ( s.empty() )
                s = "no description";
            BOOST_LOG( m_loggerError )
                << "Consensus thread in skale host will exit with exception: " << s;
        } catch ( ... ) {
            BOOST_LOG( m_loggerError )
                << "Consensus thread in skale host will exit with unknown exception\n"
                << skutils::signal::generate_stack_trace();
        }
    };  // func

    m_consensusThread = std::thread( consensusFunction );
}

// TODO finish all gracefully to allow all undone jobs be finished
void SkaleHost::stopWorking() {
    if ( !working )
        return;

    m_exitNeeded = true;
    pauseConsensus( false );

    if ( ExitHandler::shouldExit() ) {
        // requested exit
        int signal = ExitHandler::getSignal();
        int exitCode = ExitHandler::requestedExitCode();
        if ( signal > 0 )
            BOOST_LOG( m_loggerInfo )
                << "Exit requested with signal " << signal << " and exit code " << exitCode;
        else
            BOOST_LOG( m_loggerInfo ) << "Exit requested internally with exit code " << exitCode;
    } else {
        BOOST_LOG( m_loggerInfo ) << "Exiting without request";
    }


    cnote << "Skaled initiating consensus graceful exit";

    m_consensus->exitGracefully();

    BOOST_LOG( m_loggerInfo ) << "Exit initiated. Skaled waiting for consensus exit.";

    while ( m_consensus->getStatus() != CONSENSUS_EXITED ) {
        timespec ms100{ 0, 100000000 };
        nanosleep( &ms100, nullptr );
    }

    BOOST_LOG( m_loggerInfo )
        << "Consensus status is exited. Skaled is waiting for consensus and broadcast to finish.";

    if ( m_consensusThread.joinable() )
        m_consensusThread.join();

    if ( m_broadcastThread.joinable() )
        m_broadcastThread.join();

#ifdef FAIR
    if ( m_committeeRotationMonitorThread != nullptr )
        m_committeeRotationMonitorThread->join();
#endif

    working = false;

    BOOST_LOG( m_loggerInfo ) << "Consensus and broadcat threads finished.";
}

void SkaleHost::broadcastFunc() {
    dev::setThreadName( "broadcastFunc" );
    while ( !m_exitNeeded ) {
        try {
#ifdef FAIR
            if ( m_broadcastRestartNeeded ) [[unlikely]] {
                m_broadcaster->resetServerSocket();
                m_broadcastRestartNeeded = false;
            } else
#endif
                m_broadcaster->initSocket();

            MICROPROFILE_SCOPEI( "SkaleHost", "broadcastFunc", MP_BISQUE );

            // Wait for the queue to have transactions

            static auto MAX_WAIT_TIME = std::chrono::milliseconds( 100 );

            std::list< Transaction > queueCopy;

            {
                std::unique_lock< std::mutex > lock( m_broadcastQueueMutex );

                m_broadcastQueueCondition.wait_for( lock, MAX_WAIT_TIME );

                if ( m_broadcastPauseFlag || m_broadcastQueue.empty() ) {
                    continue;
                }

                queueCopy = m_broadcastQueue;
                m_broadcastQueue = std::list< Transaction >();
            }

            for ( auto&& txn : queueCopy ) {
                try {
                    MICROPROFILE_SCOPEI( "SkaleHost", "broadcastFunc.broadcast", MP_CHARTREUSE1 );
                    std::string rlp = toJS( txn.toBytes() );
                    m_debugTracer.tracepoint( "broadcast" );
                    m_broadcaster->broadcast( rlp );
                    ++m_bcast_counter;
                } catch ( const std::exception& ex ) {
                    BOOST_LOG( m_loggerWarning ) << "BROADCAST EXCEPTION CAUGHT";
                    BOOST_LOG( m_loggerWarning ) << ex.what();
                }  // catch
            }

            logState();
        } catch ( const std::exception& ex ) {
            BOOST_LOG( m_loggerError ) << "CRITICAL " << ex.what() << " (restarting broadcastFunc)";
            BOOST_LOG( m_loggerError ) << "\n" << skutils::signal::generate_stack_trace();
            sleep( 2 );
        } catch ( ... ) {
            BOOST_LOG( m_loggerError ) << "CRITICAL unknown exception (restarting broadcastFunc)";
            BOOST_LOG( m_loggerError ) << "\n" << skutils::signal::generate_stack_trace();
            sleep( 2 );
        }
    }  // while

    m_broadcaster->stopService();
}

u256 SkaleHost::getGasPrice( unsigned _blockNumber ) const {
    if ( _blockNumber == dev::eth::LatestBlock )
        _blockNumber = m_client.number();
    return m_consensus->getPriceForBlockId( _blockNumber );
}

u256 SkaleHost::getBlockRandom( unsigned _blockNumber, bool _isCalledFromTxn ) const {
    // for FAIR patch is always enabled
    // check that patch enabled after block _blockNumber - 1
    // if so - return correct value
    // if not - return value for previous block
    // works for historic calls, eth_call, eth_estimateGas
    // and regular transactions
    if ( _blockNumber == 0 ) {
        // handle corner case of genesis block
        // is never a case unless called from debug_traceBlock / eth_call on genesis
        return m_consensus->getRandomForBlockId( _blockNumber );
    }
    auto previousBlockTimestamp =
        m_client.blockInfo( m_client.hashFromNumber( _blockNumber - 1 ) ).timestamp();
    if ( CurrentBlockRandomPatch::isEnabledWhen( previousBlockTimestamp ) ) {
        if ( !_isCalledFromTxn ) {
            // means a call outside of block is being executed
            // if blockNumberToCall > currentBlockNumber, need to decrease it by 1
            // otherwise the exception is thrown
            if ( _blockNumber > m_client.number() )
                --_blockNumber;
        }
        return m_consensus->getRandomForBlockId( _blockNumber );
    }
    return m_consensus->getRandomForBlockId( _blockNumber - 1 );
}

dev::eth::SyncStatus SkaleHost::syncStatus() const {
    if ( !m_consensus )
        BOOST_THROW_EXCEPTION( std::runtime_error( "Consensus was not initialized" ) );
    auto syncInfo = m_consensus->getSyncInfo();
    dev::eth::SyncStatus syncStatus;
    // SKALE: catchup downloads blocks with transactions, then the node executes them
    // we don't download state changes separately
    syncStatus.state = syncInfo.isSyncing ? dev::eth::SyncState::Blocks : dev::eth::SyncState::Idle;
    syncStatus.startBlockNumber = syncInfo.startingBlock;
    syncStatus.currentBlockNumber = syncInfo.currentBlock;
    syncStatus.highestBlockNumber = syncInfo.highestBlock;
    syncStatus.majorSyncing = syncInfo.isSyncing;
    return syncStatus;
}

std::map< std::string, uint64_t > SkaleHost::getConsensusDbUsage() const {
    return m_consensus->getConsensusDbUsage();
}

bool SkaleHost::ignoreNewBlocksEnabled() const {
    return m_ignoreNewBlocks;
};

std::array< std::string, 4 > SkaleHost::getCurrentBLSPublicKey() const {
    return m_client.getCurrentBLSPublicKey();
}

std::string SkaleHost::getHistoricNodeId( unsigned _id ) const {
    return m_client.getHistoricNodeId( _id );
}

std::string SkaleHost::getHistoricNodeIndex( unsigned _index ) const {
    return m_client.getHistoricNodeIndex( _index );
}

std::string SkaleHost::getHistoricNodePublicKey( unsigned _idx ) const {
    return m_client.getHistoricNodePublicKey( _idx );
}

#ifndef FAIR
uint64_t SkaleHost::submitOracleRequest(
    const string& _spec, string& _receipt, string& _errorMessage ) {
    return m_consensus->submitOracleRequest( _spec, _receipt, _errorMessage );
}

uint64_t SkaleHost::checkOracleResult( const string& _receipt, string& _result ) {
    return m_consensus->checkOracleResult( _receipt, _result );
}
#endif

void SkaleHost::forceEmptyBlock() {
    assert( !this->emptyBlockIntervalMsForRestore.has_value() );
    this->emptyBlockIntervalMsForRestore = this->m_consensus->getEmptyBlockIntervalMs();
    // HACK it should be less than time-out in pendingTransactions - but not 0!
    this->m_consensus->setEmptyBlockIntervalMs( 50 );  // just 1-time!
}

void SkaleHost::forcedBroadcast( const Transaction& _txn ) {
    m_broadcaster->broadcast( toJS( _txn.toBytes() ) );
}

void SkaleHost::noteNewTransactions() {}

void SkaleHost::noteNewBlocks() {}

void SkaleHost::onBlockImported( BlockHeader const& /*_info*/ ) {}
