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
#include <skutils/task_performance.h>
#include <skutils/utils.h>

using namespace dev;
using namespace dev::eth;

const int SkaleHost::EXIT_FORCEFULLTY_SECONDS = 20;

#ifndef CONSENSUS
#define CONSENSUS 1
#endif

std::unique_ptr< ConsensusInterface > DefaultConsensusFactory::create(
    ConsensusExtFace& _extFace ) const {
#if CONSENSUS
    const auto& nfo = static_cast< const Interface& >( m_client ).blockInfo( LatestBlock );
    //
    std::cout << cc::note( "NOTE: Block number at startup is " ) << cc::size10( nfo.number() )
              << "\n";
    std::cout.flush();
    //
    auto ts = nfo.timestamp();
    auto consensus_engine_ptr = make_unique< ConsensusEngine >(
        _extFace, m_client.number(), ts, 0, m_client.chainParams().sChain.consensusStorageLimit );

    if ( m_client.chainParams().nodeInfo.sgxServerUrl != "" ) {
        this->fillSgxInfo( *consensus_engine_ptr );
    }


    this->fillPublicKeyInfo( *consensus_engine_ptr );


    this->fillRotationHistory( *consensus_engine_ptr );

    return consensus_engine_ptr;
#else
    unsigned block_number = m_client.number();
    dev::h256 state_root =
        m_client.blockInfo( m_client.hashFromNumber( block_number ) ).stateRoot();
    return make_unique< ConsensusStub >( _extFace, block_number, state_root );
#endif
}

#if CONSENSUS
void DefaultConsensusFactory::fillSgxInfo( ConsensusEngine& consensus ) const try {
    const std::string sgxServerUrl = m_client.chainParams().nodeInfo.sgxServerUrl;

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

    std::string ecdsaKeyName = m_client.chainParams().nodeInfo.ecdsaKeyName;

    std::string blsKeyName = m_client.chainParams().nodeInfo.keyShareName;

    consensus.setSGXKeyInfo(
        sgxServerUrl, sgxSSLKeyFilePath, sgxSSLCertFilePath, ecdsaKeyName, blsKeyName );


} catch ( ... ) {
    std::throw_with_nested( std::runtime_error( "Error filling SGX info (nodeGroups)" ) );
}

void DefaultConsensusFactory::fillPublicKeyInfo( ConsensusEngine& consensus ) const try {
    const std::string sgxServerUrl = m_client.chainParams().nodeInfo.sgxServerUrl;

    std::shared_ptr< std::vector< std::string > > ecdsaPublicKeys =
        std::make_shared< std::vector< std::string > >();
    for ( const auto& node : m_client.chainParams().sChain.nodes ) {
        if ( node.publicKey.size() == 0 )
            return;  // just don't do anything
        ecdsaPublicKeys->push_back( node.publicKey.substr( 2 ) );
    }

    std::vector< std::shared_ptr< std::vector< std::string > > > blsPublicKeys;
    for ( const auto& node : m_client.chainParams().sChain.nodes ) {
        std::vector< std::string > public_key_share( 4 );
        if ( node.id != this->m_client.chainParams().nodeInfo.id ) {
            public_key_share[0] = node.blsPublicKey[0];
            public_key_share[1] = node.blsPublicKey[1];
            public_key_share[2] = node.blsPublicKey[2];
            public_key_share[3] = node.blsPublicKey[3];
        } else {
            public_key_share[0] = m_client.chainParams().nodeInfo.BLSPublicKeys[0];
            public_key_share[1] = m_client.chainParams().nodeInfo.BLSPublicKeys[1];
            public_key_share[2] = m_client.chainParams().nodeInfo.BLSPublicKeys[2];
            public_key_share[3] = m_client.chainParams().nodeInfo.BLSPublicKeys[3];
        }

        blsPublicKeys.push_back(
            std::make_shared< std::vector< std::string > >( public_key_share ) );
    }

    auto blsPublicKeysPtr =
        std::make_shared< std::vector< std::shared_ptr< std::vector< std::string > > > >(
            blsPublicKeys );

    size_t n = m_client.chainParams().sChain.nodes.size();
    size_t t = ( 2 * n + 1 ) / 3;

    if ( ecdsaPublicKeys->size() && ecdsaPublicKeys->at( 0 ).size() && blsPublicKeys.size() &&
         blsPublicKeys[0]->at( 0 ).size() )
        consensus.setPublicKeyInfo( ecdsaPublicKeys, blsPublicKeysPtr, t, n );
} catch ( ... ) {
    std::throw_with_nested( std::runtime_error( "Error filling SGX info (nodeGroups)" ) );
}


void DefaultConsensusFactory::fillRotationHistory( ConsensusEngine& consensus ) const try {
    std::map< uint64_t, std::vector< std::string > > previousBLSKeys;
    std::map< uint64_t, std::string > historicECDSAKeys;
    std::map< uint64_t, std::vector< uint64_t > > historicNodeGroups;
    auto u256toUint64 = []( const dev::u256& u ) { return std::stoull( u.str() ); };
    for ( const auto& nodeGroup : m_client.chainParams().sChain.nodeGroups ) {
        std::vector< string > commonBLSPublicKey = { nodeGroup.blsPublicKey[0],
            nodeGroup.blsPublicKey[1], nodeGroup.blsPublicKey[2], nodeGroup.blsPublicKey[3] };
        previousBLSKeys[nodeGroup.finishTs] = commonBLSPublicKey;
        std::vector< uint64_t > nodes;
        // add ecdsa keys info and historic groups info
        for ( const auto& node : nodeGroup.nodes ) {
            historicECDSAKeys[u256toUint64( node.id )] = node.publicKey;
            nodes.push_back( u256toUint64( node.id ) );
        }
        historicNodeGroups[nodeGroup.finishTs] = nodes;
    }
    consensus.setRotationHistory(
        std::make_shared< std::map< uint64_t, std::vector< std::string > > >( previousBLSKeys ),
        std::make_shared< std::map< uint64_t, std::string > >( historicECDSAKeys ),
        std::make_shared< std::map< uint64_t, std::vector< uint64_t > > >( historicNodeGroups ) );
} catch ( ... ) {
    std::throw_with_nested( std::runtime_error( "Error reading rotation history (nodeGroups)" ) );
}
#endif

class ConsensusExtImpl : public ConsensusExtFace {
public:
    ConsensusExtImpl( SkaleHost& _host );
    virtual transactions_vector pendingTransactions( size_t _limit, u256& _stateRoot ) override;
    virtual void createBlock( const transactions_vector& _approvedTransactions, uint64_t _timeStamp,
        uint32_t _timeStampMs, uint64_t _blockID, u256 _gasPrice, u256 _stateRoot,
        uint64_t _winningNodeIndex ) override;
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
    const ConsensusExtFace::transactions_vector& _approvedTransactions, uint64_t _timeStamp,
    uint32_t /*_timeStampMs */, uint64_t _blockID, u256 _gasPrice, u256 _stateRoot,
    uint64_t _winningNodeIndex ) {
    MICROPROFILE_SCOPEI( "ConsensusExtFace", "createBlock", MP_INDIANRED );
    m_host.createBlock(
        _approvedTransactions, _timeStamp, _blockID, _gasPrice, _stateRoot, _winningNodeIndex );
}

void ConsensusExtImpl::terminateApplication() {
    dev::ExitHandler::exitHandler( SIGINT, dev::ExitHandler::ec_consensus_terminate_request );
}

SkaleHost::SkaleHost( dev::eth::Client& _client, const ConsensusFactory* _consFactory,
    std::shared_ptr< InstanceMonitor > _instanceMonitor, const std::string& _gethURL,
    bool _broadcastEnabled ) try : m_client( _client ),
                                   m_tq( _client.m_tq ),
                                   m_instanceMonitor( _instanceMonitor ),
                                   m_broadcastEnabled( _broadcastEnabled ),
                                   total_sent( 0 ),
                                   total_arrived( 0 ) {
    m_debugHandler = [this]( const std::string& arg ) -> std::string {
        return DebugTracer_handler( arg, this->m_debugTracer );
    };

    m_debugTracer.call_on_tracepoint( [this]( const std::string& name ) {
        skutils::task::performance::action action(
            "trace/" + name, std::to_string( m_debugTracer.get_tracepoint_count( name ) ) );

        // HACK reduce TRACEPOINT log output
        static uint64_t last_block_when_log = -1;
        if ( name == "fetch_transactions" || name == "drop_bad_transactions" ) {
            uint64_t current_block = this->m_client.number();
            if ( current_block == last_block_when_log )
                return;
            if ( name == "drop_bad_transactions" )
                last_block_when_log = current_block;
        }

        LOG( m_traceLogger ) << "TRACEPOINT " << name << " "
                             << m_debugTracer.get_tracepoint_count( name );
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

    m_consensus->parseFullConfigAndCreateNode( m_client.chainParams().getOriginalJson(), _gethURL );
} catch ( const std::exception& ) {
    std::throw_with_nested( CreationException() );
}

SkaleHost::~SkaleHost() {}

void SkaleHost::logState() {
    LOG( m_traceLogger ) << cc::debug( " sent_to_consensus = " ) << total_sent
                         << cc::debug( " got_from_consensus = " ) << total_arrived
                         << cc::debug( " m_transaction_cache = " ) << m_m_transaction_cache.size()
                         << cc::debug( " m_tq = " ) << m_tq.status().current
                         << cc::debug( " m_bcast_counter = " ) << m_bcast_counter;
}

h256 SkaleHost::receiveTransaction( std::string _rlp ) {
    Transaction transaction( jsToBytes( _rlp, OnFailed::Throw ), CheckTransaction::None );

    h256 sha = transaction.sha3();

    //
    static std::atomic_size_t g_nReceiveTransactionsTaskNumber = 0;
    size_t nReceiveTransactionsTaskNumber = g_nReceiveTransactionsTaskNumber++;
    std::string strPerformanceQueueName = "bc/receive_transaction";
    std::string strPerformanceActionName =
        skutils::tools::format( "receive task %zu", nReceiveTransactionsTaskNumber );
    skutils::task::performance::action a( strPerformanceQueueName, strPerformanceActionName );
    //
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

    // HACK this should be field (or better do it another way)
    static bool first_run = true;
    if ( first_run ) {
        m_consensusWorkingMutex.lock();
        first_run = false;
    }
    if ( m_exitNeeded )
        return out_vector;

    std::lock_guard< std::mutex > pauseLock( m_consensusPauseMutex );

    if ( m_exitNeeded )
        return out_vector;

    unlock_guard< std::timed_mutex > unlocker( m_consensusWorkingMutex );

    if ( m_exitNeeded ) {
        unlocker.will_exit();
        return out_vector;
    }

    if ( this->emptyBlockIntervalMsForRestore.has_value() ) {
        this->m_consensus->setEmptyBlockIntervalMs( this->emptyBlockIntervalMsForRestore.value() );
        this->emptyBlockIntervalMsForRestore.reset();
    }

    MICROPROFILE_SCOPEI( "SkaleHost", "pendingTransactions", MP_LAWNGREEN );


    _stateRoot = dev::h256::Arith( this->m_client.latestBlock().info().stateRoot() );

    h256Hash to_delete;

    //
    static std::atomic_size_t g_nFetchTransactionsTaskNumber = 0;
    size_t nFetchTransactionsTaskNumber = g_nFetchTransactionsTaskNumber++;
    std::string strPerformanceQueueName_fetch_transactions = "bc/fetch_transactions";
    std::string strPerformanceActionName_fetch_transactions =
        skutils::tools::format( "fetch task %zu", nFetchTransactionsTaskNumber );
    skutils::task::performance::json jsn = skutils::task::performance::json::object();
    jsn["limit"] = toJS( _limit );
    jsn["stateRoot"] = toJS( _stateRoot );
    skutils::task::performance::action a_fetch_transactions(
        strPerformanceQueueName_fetch_transactions, strPerformanceActionName_fetch_transactions,
        jsn );
    //
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
                    bool isMtmEnabled = m_client.chainParams().sChain.multiTransactionMode;
                    Executive::verifyTransaction( tx,
                        static_cast< const Interface& >( m_client ).blockInfo( LatestBlock ),
                        m_client.state().startRead(), *m_client.sealEngine(), 0, getGasPrice(),
                        isMtmEnabled );
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


    //
    a_fetch_transactions.finish();
    //

    if ( counter++ == 0 )
        m_pending_createMutex.lock();

    std::lock_guard< std::recursive_mutex > lock( m_pending_createMutex, std::adopt_lock );

    m_debugTracer.tracepoint( "drop_bad_transactions" );

    {
        std::lock_guard< std::mutex > localGuard( m_receivedMutex );
        //
        static std::atomic_size_t g_nDropBadTransactionsTaskNumber = 0;
        size_t nDropBadTransactionsTaskNumber = g_nDropBadTransactionsTaskNumber++;
        std::string strPerformanceQueueName_drop_bad_transactions = "bc/fetch_transactions";
        std::string strPerformanceActionName_drop_bad_transactions =
            skutils::tools::format( "fetch task %zu", nDropBadTransactionsTaskNumber );
        skutils::task::performance::json jsn = skutils::task::performance::json::object();
        skutils::task::performance::json jarrDroppedTransactions =
            skutils::task::performance::json::array();
        for ( auto sha : to_delete ) {
            jarrDroppedTransactions.push_back( toJS( sha ) );
        }
        jsn["droppedTransactions"] = jarrDroppedTransactions;
        skutils::task::performance::action a_drop_bad_transactions(
            strPerformanceQueueName_drop_bad_transactions,
            strPerformanceActionName_drop_bad_transactions, jsn );
        //
        for ( auto sha : to_delete ) {
            m_debugTracer.tracepoint( "drop_bad" );
            m_tq.drop( sha );
            if ( m_received.count( sha ) != 0 )
                m_received.erase( sha );
            LOG( m_debugLogger ) << "m_received = " << m_received.size() << std::endl;
        }
    }

    if ( this->m_exitNeeded )
        unlocker.will_exit();

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

    if ( this->m_exitNeeded )
        unlocker.will_exit();

    return out_vector;
}

void SkaleHost::createBlock( const ConsensusExtFace::transactions_vector& _approvedTransactions,
    uint64_t _timeStamp, uint64_t _blockID, u256 _gasPrice, u256 _stateRoot,
    uint64_t _winningNodeIndex ) try {
    //
    static std::atomic_size_t g_nCreateBlockTaskNumber = 0;
    size_t nCreateBlockTaskNumber = g_nCreateBlockTaskNumber++;
    std::string strPerformanceQueueName_create_block = "bc/create_block";
    std::string strPerformanceActionName_create_block =
        skutils::tools::format( "b-create %zu", nCreateBlockTaskNumber );
    skutils::task::performance::json jsn_create_block = skutils::task::performance::json::object();
    jsn_create_block["blockID"] = toJS( _blockID );
    jsn_create_block["timeStamp"] = toJS( _timeStamp );
    jsn_create_block["gasPrice"] = toJS( _gasPrice );
    jsn_create_block["stateRoot"] = toJS( _stateRoot );
    skutils::task::performance::json jarrApprovedTransactions =
        skutils::task::performance::json::array();
    for ( auto it = _approvedTransactions.begin(); it != _approvedTransactions.end(); ++it ) {
        const bytes& data = *it;
        h256 sha = sha3( data );
        jarrApprovedTransactions.push_back( toJS( sha ) );
    }
    jsn_create_block["approvedTransactions"] = jarrApprovedTransactions;
    skutils::task::performance::action a_create_block( strPerformanceQueueName_create_block,
        strPerformanceActionName_create_block, jsn_create_block );

    std::lock_guard< std::recursive_mutex > lock( m_pending_createMutex );

    LOG( m_debugLogger ) << cc::debug( "createBlock " ) << cc::notice( "ID" ) << cc::debug( " = " )
                         << cc::warn( "#" ) << cc::num10( _blockID ) << std::endl;
    m_debugTracer.tracepoint( "create_block" );

    // convert bytes back to transactions (using caching), delete them from q and push results into
    // blockchain

    if ( this->m_client.chainParams().sChain.snapshotIntervalSec > 0 ) {
        dev::h256 stCurrent =
            this->m_client.blockInfo( this->m_client.hashFromNumber( _blockID - 1 ) ).stateRoot();

        LOG( m_traceLogger ) << cc::debug( "STATE ROOT FOR BLOCK: " )
                             << cc::debug( std::to_string( _blockID - 1 ) ) << ' '
                             << cc::debug( stCurrent.hex() ) << std::endl;

        // FATAL if mismatch in non-default
        if ( _winningNodeIndex != 0 && dev::h256::Arith( stCurrent ) != _stateRoot ) {
            clog( VerbosityError, "skale-host" )
                << cc::fatal( "FATAL STATE ROOT MISMATCH ERROR:" )
                << cc::error( " current state root " )
                << cc::warn( dev::h256::Arith( stCurrent ).str() )
                << cc::error( " is not equal to arrived state root " )
                << cc::warn( _stateRoot.str() ) << cc::error( " with block ID " )
                << cc::notice( "#" ) << cc::num10( _blockID ) << cc::warn( ", " )
                << cc::p( "/data_dir" )
                << cc::error( " cleanup is recommended, exiting with code " )
                << cc::num10( int( ExitHandler::ec_state_root_mismatch ) ) << "...";
            ExitHandler::exitHandler( SIGABRT, ExitHandler::ec_state_root_mismatch );
            _exit( int( ExitHandler::ec_state_root_mismatch ) );
        }

        // WARN if default but non-zero
        if ( _winningNodeIndex == 0 && _stateRoot != u256() )
            clog( VerbosityWarning, "skale-host" )
                << cc::warn( "WARNING: STATE ROOT MISMATCH!" )
                << cc::warn( " Current block is DEFAULT BUT arrived state root is " )
                << cc::warn( _stateRoot.str() ) << cc::warn( " with block ID " )
                << cc::notice( "#" ) << cc::num10( _blockID );
    }

    std::vector< Transaction > out_txns;  // resultant Transaction vector

    std::atomic_bool have_consensus_born = false;  // means we need to re-verify old txns

    // HACK this is for not allowing new transactions in tq between deletion and block creation!
    // TODO decouple SkaleHost and Client!!!
    size_t n_succeeded;
    DEV_GUARDED( m_client.m_blockImportMutex ) {
        m_debugTracer.tracepoint( "drop_good_transactions" );

        skutils::task::performance::json jarrProcessedTxns =
            skutils::task::performance::json::array();

        for ( auto it = _approvedTransactions.begin(); it != _approvedTransactions.end(); ++it ) {
            const bytes& data = *it;
            h256 sha = sha3( data );
            LOG( m_traceLogger ) << cc::debug( "Arrived txn: " ) << sha << std::endl;
            jarrProcessedTxns.push_back( toJS( sha ) );
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
                // for test std::thread( [t, this]() { m_client.importTransaction( t ); }
                // ).detach();
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

        assert( _blockID == m_client.number() + 1 );

        //
        a_create_block.finish();
        //
        static std::atomic_size_t g_nImportBlockTaskNumber = 0;
        size_t nImportBlockTaskNumber = g_nImportBlockTaskNumber++;
        std::string strPerformanceQueueName_import_block = "bc/import_block";
        std::string strPerformanceActionName_import_block =
            skutils::tools::format( "b-import %zu", nImportBlockTaskNumber );
        skutils::task::performance::json jsn_import_block =
            skutils::task::performance::json::object();
        jsn_import_block["txns"] = jarrProcessedTxns;
        skutils::task::performance::action a_import_block( strPerformanceQueueName_import_block,
            strPerformanceActionName_import_block, jsn_import_block );
        //
        m_debugTracer.tracepoint( "import_block" );

        n_succeeded = m_client.importTransactionsAsBlock( out_txns, _gasPrice, _timeStamp );
    }  // m_blockImportMutex

    if ( n_succeeded != out_txns.size() )
        penalizePeer();

    LOG( m_debugLogger ) << cc::success( "Successfully imported " ) << n_succeeded
                         << cc::success( " of " ) << out_txns.size()
                         << cc::success( " transactions" ) << std::endl;

    if ( have_consensus_born )
        this->m_lastBlockWithBornTransactions = _blockID;

    logState();

    if ( m_instanceMonitor != nullptr ) {
        if ( m_instanceMonitor->isTimeToRotate( _timeStamp ) ) {
            m_instanceMonitor->prepareRotation();
            m_consensus->exitGracefully();
            ExitHandler::exitHandler( SIGTERM, ExitHandler::ec_rotation_complete );
            clog( VerbosityInfo, "skale-host" ) << "Rotation is completed. Instance is exiting";
        }
    }
} catch ( const std::exception& ex ) {
    cerror << "CRITICAL " << ex.what() << " (in createBlock)";
    cerror << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
} catch ( ... ) {
    cerror << "CRITICAL unknown exception (in createBlock)";
    cerror << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
}

void SkaleHost::startWorking() {
    if ( working )
        return;

    // TODO Should we do it at end of this func? (problem: broadcaster receives transaction and
    // recursively calls this func - so working is still false!)
    working = true;
    m_exitedForcefully = false;

    if ( m_broadcastEnabled ) {
        try {
            m_broadcaster->startService();
        } catch ( const Broadcaster::StartupException& ) {
            working = false;
            std::throw_with_nested( SkaleHost::CreationException() );
        }

        auto bcast_func = std::bind( &SkaleHost::broadcastFunc, this );
        m_broadcastThread = std::thread( bcast_func );
    }

    try {
        m_consensus->startAll();
    } catch ( const std::exception& ) {
        // cleanup
        m_exitNeeded = true;
        if ( m_broadcastEnabled ) {
            m_broadcastThread.join();
        }
        throw;
    }

    std::promise< void > bootstrap_promise;

    auto csus_func = [&]() {
        try {
            static const char g_strThreadName[] = "bootStrapAll";
            dev::setThreadName( g_strThreadName );
            std::cout << "Thread " << g_strThreadName << " started\n";
            m_consensus->bootStrapAll();
            std::cout << "Thread " << g_strThreadName << " will exit\n";
        } catch ( std::exception& ex ) {
            std::string s = ex.what();
            if ( s.empty() )
                s = "no description";
            std::cout << "Consensus thread in scale host will exit with exception: " << s << "\n";
        } catch ( ... ) {
            std::cout << "Consensus thread in scale host will exit with unknown exception\n";
            std::cout << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
        }

        bootstrap_promise.set_value();
    };  // func

    m_consensusThread = std::thread( csus_func );
    bootstrap_promise.get_future().wait();
}

// TODO finish all gracefully to allow all undone jobs be finished
void SkaleHost::stopWorking() {
    if ( !working )
        return;

    bool locked =
        m_consensusWorkingMutex.try_lock_for( std::chrono::seconds( EXIT_FORCEFULLTY_SECONDS ) );
    auto lock = locked ? std::make_unique< std::lock_guard< std::timed_mutex > >(
                             m_consensusWorkingMutex, std::adopt_lock ) :
                         std::unique_ptr< std::lock_guard< std::timed_mutex > >();
    ( void ) lock;  // for Codacy

    // if we could not lock from 1st attempt - then exit forcefully!
    if ( !locked ) {
        m_exitedForcefully = true;
        clog( VerbosityWarning, "skale-host" )
            << cc::fatal( "ATTENTION:" ) << " "
            << cc::error( "Forcefully shutting down consensus!" );
    }


    m_exitNeeded = true;
    pauseConsensus( false );

    std::cerr << "1 before exitGracefully()" << std::endl;

    if ( ExitHandler::shouldExit() ) {
        // requested exit
        int signal = ExitHandler::getSignal();
        int exitCode = ExitHandler::requestedExitCode();
        clog( VerbosityInfo, "skale-host" )
            << cc::info( "Exit requested with signal " ) << signal << " and exit code " << exitCode;
    } else {
        clog( VerbosityInfo, "skale-host" ) << cc::info( "Exiting without request" );
    }

    m_consensus->exitGracefully();

    std::cerr << "2 after exitGracefully()" << std::endl;

    while ( m_consensus->getStatus() != CONSENSUS_EXITED ) {
        timespec ms100{ 0, 100000000 };
        nanosleep( &ms100, nullptr );
    }

    std::cerr << "3 after wait loop" << std::endl;

    if ( m_consensusThread.joinable() )
        m_consensusThread.join();

    if ( m_broadcastThread.joinable() )
        m_broadcastThread.join();

    working = false;

    std::cerr << "4 before dtor" << std::endl;
}

void SkaleHost::broadcastFunc() {
    dev::setThreadName( "broadcastFunc" );
    size_t nBroadcastTaskNumber = 0;
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
                        std::string rlp = toJS( txn.rlp() );
                        std::string h = toJS( txn.sha3() );
                        //
                        std::string strPerformanceQueueName = "bc/broadcast";
                        std::string strPerformanceActionName =
                            skutils::tools::format( "broadcast %zu", nBroadcastTaskNumber++ );
                        skutils::task::performance::json jsn =
                            skutils::task::performance::json::object();
                        jsn["rlp"] = rlp;
                        jsn["hash"] = h;
                        skutils::task::performance::action a(
                            strPerformanceQueueName, strPerformanceActionName, jsn );
                        //
                        m_debugTracer.tracepoint( "broadcast" );
                        m_broadcaster->broadcast( rlp );
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
            cerror << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
            sleep( 2 );
        } catch ( ... ) {
            cerror << "CRITICAL unknown exception (restarting broadcastFunc)";
            cerror << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
            sleep( 2 );
        }
    }  // while

    m_broadcaster->stopService();
}

u256 SkaleHost::getGasPrice() const {
    return m_consensus->getPriceForBlockId( m_client.number() );
}

u256 SkaleHost::getBlockRandom() const {
    return m_consensus->getRandomForBlockId( m_client.number() );
}

std::array< std::string, 4 > SkaleHost::getIMABLSPublicKey() const {
    return m_client.getIMABLSPublicKey();
}

uint64_t SkaleHost::submitOracleRequest( const string& _spec, string& _receipt ) {
    return m_consensus->submitOracleRequest( _spec, _receipt );
}

uint64_t SkaleHost::checkOracleResult( const string& _receipt, string& _result ) {
    return m_consensus->checkOracleResult( _receipt, _result );
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

void SkaleHost::noteNewTransactions() {}

void SkaleHost::noteNewBlocks() {}

void SkaleHost::onBlockImported( BlockHeader const& /*_info*/ ) {}
