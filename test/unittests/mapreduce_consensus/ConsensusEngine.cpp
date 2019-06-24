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
 * @file ConsensusEngine.cpp
 * @author Dima Litvinov
 * @date 2018
 */

#include <libconsensus/node/ConsensusEngine.h>

#include "../libweb3jsonrpc/WebThreeStubClient.h"

#include <test/tools/libtesteth/TestHelper.h>

#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/Debug.h>
#include <libweb3jsonrpc/Eth.h>
#include <libweb3jsonrpc/ModularServer.h>
#include <libweb3jsonrpc/Skale.h>
#include <libweb3jsonrpc/Test.h>
#include <libweb3jsonrpc/Web3.h>

#include <libethereum/ChainParams.h>
#include <libethereum/Client.h>
#include <libethereum/TransactionQueue.h>

#include <libdevcore/CommonJS.h>
#include <libethcore/SealEngine.h>

#include <boost/test/unit_test.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>

namespace dev {
namespace eth {};
};  // namespace dev

using namespace dev;
using namespace dev::eth;

class TestIpcServer : public jsonrpc::AbstractServerConnector {
public:
    bool StartListening() override { return true; }
    bool StopListening() override { return true; }
    bool SendResponse( std::string const& _response, void* _addInfo = nullptr ) /*override*/ {
        *static_cast< std::string* >( _addInfo ) = _response;
        return true;
    }
};

class TestIpcClient : public jsonrpc::IClientConnector {
public:
    explicit TestIpcClient( TestIpcServer& /*_server*/ )
    //: m_server{_server} // l_sergiy: clang did detected this as unused
    {}

    void SendRPCMessage( const std::string& /*_message*/, std::string& /*_result*/ ) /*noexcept(
                                                                                        false )*/
        override {
        // TO-FIX: l_sergiy: migration to new json-rpc-cpp
        ////////////////////////////////m_server.OnRequest( _message, &_result );
    }

private:
    // TestIpcServer& m_server; // l_sergiy: clang did detected this as unused
};

class ConsensusExtFaceFixture : public ConsensusExtFace {
protected:
    std::shared_ptr< ConsensusEngine > m_consensus;
    std::thread m_consensusThread;

    transactions_vector buffer;

    std::mutex m_transactionsMutex;
    std::condition_variable m_transactionsCond;

    std::mutex m_blockMutex;
    std::condition_variable m_blockCond;

    std::vector< int > m_arrived_blocks;

protected:  // remote peer
    unique_ptr< Client > client;
    unique_ptr< ModularServer<> > rpcServer;
    unique_ptr< WebThreeStubClient > rpcClient;

    unique_ptr< FixedAccountHolder > accountHolder;
    dev::KeyPair coinbase{KeyPair::create()};

public:
    ConsensusExtFaceFixture() {
        ChainParams chainParams;
        chainParams.sealEngineName = NoProof::name();
        chainParams.allowFutureBlocks = true;
        chainParams.difficulty = chainParams.minimumDifficulty;
        chainParams.gasLimit = chainParams.maxGasLimit;
        chainParams.extraData = h256::random().asBytes();

        sChainNode node2{u256( 2 ), "127.0.0.12", u256( 11111 ), u256( 1 )};
        chainParams.sChain.nodes.push_back( node2 );
        //////////////////////////////////////////////

        m_consensus.reset( new ConsensusEngine(
            *this, 0, BlockHeader( chainParams.genesisBlock() ).timestamp() ) );
        m_consensus->parseFullConfigAndCreateNode( chainParams.getOriginalJson() );

        m_consensusThread = std::thread( [this]() {
            m_consensus->startAll();
            m_consensus->bootStrapAll();
        } );

        //////////////////////////////////////////////
        chainParams.nodeInfo.ip = "127.0.0.12";
        chainParams.nodeInfo.id = 2;
        chainParams.nodeInfo.name = "Node2";
        chainParams.resetJson();

        //        web3.reset( new WebThreeDirect(
        //            "eth tests", "", "", chainParams, WithExisting::Kill, {"eth"}, true ) );

        client.reset(
            new eth::Client( chainParams, ( int ) chainParams.networkID, shared_ptr< GasPricer >(),
                "", "", WithExisting::Kill, TransactionQueue::Limits{100000, 1024} ) );

        client->injectSkaleHost();
        client->startWorking();

        client->setAuthor( coinbase.address() );

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( {coinbase} );

        using FullServer = ModularServer< rpc::EthFace, rpc::SkaleFace, rpc::Web3Face,
            rpc::DebugFace, rpc::TestFace >;

        auto ethFace = new rpc::Eth( *client, *accountHolder.get() );

        rpcServer.reset( new FullServer( ethFace, new rpc::Skale( *client->skaleHost() ),
            new rpc::Web3( /*web3->clientVersion()*/ ), new rpc::Debug( *client ),  // TODO add
                                                                                    // version here?
            new rpc::Test( *client ) ) );
        auto ipcServer = new TestIpcServer;
        rpcServer->addConnector( ipcServer );
        ipcServer->StartListening();

        auto client = new TestIpcClient{*ipcServer};
        rpcClient = unique_ptr< WebThreeStubClient >( new WebThreeStubClient( *client ) );
    }

    virtual transactions_vector pendingTransactions( size_t _limit ) override {
        if ( _limit < buffer.size() )
            assert( false );

        std::unique_lock< std::mutex > lock( m_transactionsMutex );
        if ( buffer.empty() ) {
            m_transactionsCond.wait_for( lock, std::chrono::milliseconds( 100 ) );
        }

        transactions_vector tmp;
        buffer.swap( tmp );
        return tmp;
    }

    virtual void createBlock( const transactions_vector& _approvedTransactions, uint64_t _timeStamp,
        uint32_t /* timeStampMs */, uint64_t _blockID, u256 /*_gasPrice */ ) override {
        ( void ) _timeStamp;
        ( void ) _blockID;
        std::cerr << "Block arrived with " << _approvedTransactions.size() << " txns" << std::endl;
        m_arrived_blocks.push_back( _approvedTransactions.size() );
        m_blockCond.notify_one();
    }

    virtual ~ConsensusExtFaceFixture() override {
        // TODO Kill everyone
        m_consensus->exitGracefully();
        m_consensusThread.join();
    }

    void pushTransactions( const Transactions& txns ) {
        assert( buffer.empty() );

        for ( const Transaction& txn : txns ) {
            buffer.push_back( txn.rlp() );
        }  // for

        m_transactionsCond.notify_one();
    }

    void waitBlock() {
        std::unique_lock< std::mutex > lock( m_blockMutex );
        m_blockCond.wait( lock );
    }

    void customTransactionsTest( std::vector< int > commands ) {
        Json::Value t;
        t["to"] = toJS( coinbase.address() );
        t["nonce"] = jsToDecimal( "0" );
        t["value"] = jsToDecimal( "0" );
        t["gasPrice"] = jsToDecimal( "0" );

        std::vector< KeyPair > senders;
        for ( size_t i = 0; i < commands.size(); ++i ) {
            senders.push_back( KeyPair::create() );
        }  // for

        accountHolder->setAccounts( senders );

        try {
            Transactions local_txns;

            for ( size_t i = 0; i < commands.size(); ++i ) {
                t["from"] = toJS( senders[i].address() );

                auto signedTx = rpcClient->eth_signTransaction( t );
                std::string stringTx = signedTx["raw"].asString();
                Transaction tx( jsToBytes( stringTx, OnFailed::Throw ), CheckTransaction::None );

                if ( commands[i] & ( 1 << 0 ) )
                    local_txns.push_back( tx );
                if ( commands[i] & ( 1 << 1 ) )
                    rpcClient->skale_receiveTransaction( stringTx );
            }

            pushTransactions( local_txns );

            waitBlock();

            sleep( 4 );

            ///////////////////////////////////////////////////////////////

            std::cerr << "Node1 has " << m_arrived_blocks.size() << " blocks" << std::endl;
            for ( size_t i = 0; i < m_arrived_blocks.size(); i++ ) {
                std::cerr << "Node1's block " << ( i + 1 ) << " contains " << m_arrived_blocks[i]
                          << std::endl;
            }

            auto number = client->number();
            std::cerr << "Node2 has " << number << " blocks" << std::endl;
            for ( size_t i = 0; i <= number; i++ ) {
                auto tx_count = client->transactionCount( BlockNumber( i ) );
                std::cerr << "Node2's block " << i << " contains " << tx_count << std::endl;
            }

        } catch ( std::exception& ex ) {
            std::cerr << ex.what() << std::endl;
        }
    }
};

// Now there is an error in ConsensusExtFaceFixture on start

BOOST_AUTO_TEST_SUITE( ConsensusTests, *boost::unit_test::disabled() )

BOOST_AUTO_TEST_CASE( OneTransaction ) {}

BOOST_AUTO_TEST_CASE( TwoTransactions ) {}

BOOST_AUTO_TEST_CASE( DifferentTransactions ) {}

BOOST_AUTO_TEST_CASE( MissingTransaction1 ) {}

BOOST_AUTO_TEST_CASE( MissingTransaction2 ) {}

BOOST_AUTO_TEST_SUITE_END()


// Now there is an error in ConsensusExtFaceFixture on start

// BOOST_FIXTURE_TEST_SUIT( ConsensusTests, ConsensusExtFaceFixture )

// BOOST_AUTO_TEST_CAS( OneTransaction ) {
//    customTransactionsTest( std::vector< int >{3} );
//}

// BOOST_AUTO_TEST_CAS( TwoTransactions ) {
//    customTransactionsTest( std::vector< int >{3, 3} );
//}

// BOOST_AUTO_TEST_CAS( DifferentTransactions ) {
//    customTransactionsTest( std::vector< int >{1, 2} );
//}

// BOOST_AUTO_TEST_CAS( MissingTransaction1 ) {
//    customTransactionsTest( std::vector< int >{1, 3} );
//}

// BOOST_AUTO_TEST_CAS( MissingTransaction2 ) {
//    customTransactionsTest( std::vector< int >{3, 1} );
//}

// BOOST_AUTO_TEST_SUIT_END()
