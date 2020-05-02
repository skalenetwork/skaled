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

#pragma GCC diagnostic ignored "-Wdeprecated"

#include "WebThreeStubClient.h"

#include <jsonrpccpp/server/abstractserverconnector.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/TransientDirectory.h>
#include <libethcore/CommonJS.h>
#include <libethcore/KeyManager.h>
#include <libethereum/ChainParams.h>
#include <libethereum/ClientTest.h>
#include <libethereum/TransactionQueue.h>
#include <libp2p/Network.h>
#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/AdminEth.h>
#include <libweb3jsonrpc/JsonHelper.h>
// SKALE#include <libweb3jsonrpc/AdminNet.h>
#include <libweb3jsonrpc/Debug.h>
#include <libweb3jsonrpc/Eth.h>
#include <libweb3jsonrpc/ModularServer.h>
// SKALE #include <libweb3jsonrpc/Net.h>
#include <libweb3jsonrpc/Test.h>
#include <libweb3jsonrpc/Web3.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/lexical_cast.hpp>
#include <boost/test/unit_test.hpp>

// This is defined by some weird windows header - workaround for now.
#undef GetMessage

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;

static std::string const c_genesisConfigString =
    R"(
{
    "sealEngine": "NoProof",
    "params": {
         "accountStartNonce": "0x00",
         "maximumExtraDataSize": "0x1000000",
         "blockReward": "0x4563918244F40000",
         "allowFutureBlocks": true,
         "homesteadForkBlock": "0x00",
         "EIP150ForkBlock": "0x00",
         "EIP158ForkBlock": "0x00",
         "byzantiumForkBlock": "0x00"
    },
    "genesis": {
        "author" : "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
        "difficulty" : "0x20000",
        "gasLimit" : "0x0f4240",
        "nonce" : "0x00",
        "extraData" : "0x00",
        "timestamp" : "0x00",
        "mixHash" : "0x00",
        "stateRoot": "0x01"
    },
    "skaleConfig": {
        "nodeInfo": {
            "nodeName": "Node1",
            "nodeID": 1112,
            "bindIP": "127.0.0.1",
            "basePort": 1231,
            "logLevel": "trace",
            "logLevelProposal": "trace"
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "storageLimit": 128,
            "nodes": [
                { "nodeID": 1112, "ip": "127.0.0.1", "basePort": 1231, "schainIndex" : 1}
            ]
        }
    },
    "accounts": {
        "0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
        "0000000000000000000000000000000000000005": {
            "precompiled": {
                "name": "createFile",
                "linear": {
                    "base": 15,
                    "word": 0
                },
                "restrictAccess": ["00000000000000000000000000000000000000AA", "692a70d2e424a56d2c6c27aa97d1a86395877b3a"]
            }
        },)"
    /*
            pragma solidity ^0.4.25;
            contract Caller {
                function call() public {
                    bool status;
                    string memory fileName = "test";
                    address sender = 0x000000000000000000000000000000AA;
                    assembly{
                        let ptr := mload(0x40)
                        mstore(ptr, sender)
                        mstore(add(ptr, 0x20), 4)
                        mstore(add(ptr, 0x40), mload(add(fileName, 0x20)))
                        mstore(add(ptr, 0x60), 1)
                        status := call(not(0), 0x05, 0, ptr, 0x80, ptr, 32)
                    }
                }
            }
    */
    R"("0x692a70d2e424a56d2c6c27aa97d1a86395877b3a" : {
            "balance" : "0x00",
            "code" : "0x608060405260043610603f576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff16806328b5e32b146044575b600080fd5b348015604f57600080fd5b5060566058565b005b6000606060006040805190810160405280600481526020017f7465737400000000000000000000000000000000000000000000000000000000815250915060aa905060405181815260046020820152602083015160408201526001606082015260208160808360006005600019f19350505050505600a165627a7a72305820a32bd2de440ff0b16fac1eba549e1f46ebfb51e7e4fe6bfe1cc0d322faf7af540029",
            "nonce" : "0x00",
            "storage" : {
            }
        },
        "0x095e7baea6a6c7c4c2dfeb977efac326af552d87" : {
            "balance" : "0x0de0b6b3a7640000",
            "code" : "0x6001600101600055",
            "nonce" : "0x00",
            "storage" : {
            }
        },
        "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b" : {
            "balance" : "0x0de0b6b3a7640000",
            "code" : "0x",
            "nonce" : "0x00",
            "storage" : {
            }
        }
    }
}
)";


namespace {
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
    explicit TestIpcClient( TestIpcServer& _server ) : m_server{_server} {}

    void SendRPCMessage( const std::string& _message, std::string& _result ) override {
        m_server.ProcessRequest( _message, _result );
    }

private:
    TestIpcServer& m_server;
};

struct JsonRpcFixture : public TestOutputHelperFixture {
    JsonRpcFixture( const std::string& _config = "", bool _owner = true ) {
        dev::p2p::NetworkPreferences nprefs;
        ChainParams chainParams;
        if ( _config != "" ) {
            Json::Value ret;
            Json::Reader().parse( _config, ret );
            if ( _owner ) {
                ret["skaleConfig"]["sChain"]["schainOwner"] = toJS( coinbase.address() );
            } else {
                ret["skaleConfig"]["sChain"]["schainOwner"] = toJS( account2.address() );
            }
            Json::FastWriter fastWriter;
            std::string output = fastWriter.write( ret );
            chainParams = chainParams.loadConfig( output );
        } else {
            chainParams.sealEngineName = NoProof::name();
            chainParams.allowFutureBlocks = true;
            chainParams.difficulty = chainParams.minimumDifficulty;
            chainParams.gasLimit = chainParams.maxGasLimit;
            chainParams.byzantiumForkBlock = 0;
            chainParams.externalGasDifficulty = 1;
            chainParams.sChain.storageLimit = 128;
            // add random extra data to randomize genesis hash and get random DB path,
            // so that tests can be run in parallel
            // TODO: better make it use ethemeral in-memory databases
            chainParams.extraData = h256::random().asBytes();
        }

        //        web3.reset( new WebThreeDirect(
        //            "eth tests", tempDir.path(), "", chainParams, WithExisting::Kill, {"eth"},
        //            true ) );

        client.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
            shared_ptr< GasPricer >(), NULL, tempDir.path(), WithExisting::Kill ) );

        //        client.reset(
        //            new eth::Client( chainParams, ( int ) chainParams.networkID, shared_ptr<
        //            GasPricer >(),
        //                tempDir.path(), "", WithExisting::Kill, TransactionQueue::Limits{100000,
        //                1024} ) );

        client->injectSkaleHost();
        client->startWorking();

        client->setAuthor( coinbase.address() );

        using FullServer = ModularServer< rpc::EthFace, /* rpc::NetFace,*/ rpc::Web3Face,
            rpc::AdminEthFace /*, rpc::AdminNetFace*/, rpc::DebugFace, rpc::TestFace >;

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( {coinbase, account2} );

        sessionManager.reset( new rpc::SessionManager() );
        adminSession =
            sessionManager->newSession( rpc::SessionPermissions{{rpc::Privilege::Admin}} );

        auto ethFace = new rpc::Eth( *client, *accountHolder.get() );

        gasPricer = make_shared< eth::TrivialGasPricer >( 0, DefaultGasPrice );

        rpcServer.reset( new FullServer( ethFace /*, new rpc::Net(*web3)*/,
            new rpc::Web3( /*web3->clientVersion()*/ ),  // TODO Add real version?
            new rpc::AdminEth( *client, *gasPricer, keyManager, *sessionManager.get() ),
            /*new rpc::AdminNet(*web3, *sessionManager), */ new rpc::Debug( *client ),
            new rpc::Test( *client ) ) );
        auto ipcServer = new TestIpcServer;
        rpcServer->addConnector( ipcServer );
        ipcServer->StartListening();

        auto client = new TestIpcClient( *ipcServer );
        rpcClient = unique_ptr< WebThreeStubClient >( new WebThreeStubClient( *client ) );
    }

    ~JsonRpcFixture() { system( "rm -rf /tmp/*.db*" ); }

    string sendingRawShouldFail( string const& _t ) {
        try {
            rpcClient->eth_sendRawTransaction( _t );
            BOOST_FAIL( "Exception expected." );
        } catch ( jsonrpc::JsonRpcException const& _e ) {
            return _e.GetMessage();
        }
        return string();
    }

    unique_ptr< Client > client;
    dev::KeyPair coinbase{KeyPair::create()};
    dev::KeyPair account2{KeyPair::create()};
    unique_ptr< FixedAccountHolder > accountHolder;
    unique_ptr< rpc::SessionManager > sessionManager;
    std::shared_ptr< eth::TrivialGasPricer > gasPricer;
    KeyManager keyManager{KeyManager::defaultPath(), SecretStore::defaultPath()};
    unique_ptr< ModularServer<> > rpcServer;
    unique_ptr< WebThreeStubClient > rpcClient;
    std::string adminSession;

    TransientDirectory tempDir;
};

struct RestrictedAddressFixture : public JsonRpcFixture {
    RestrictedAddressFixture() : JsonRpcFixture( c_genesisConfigString ) {
        ownerAddress = Address( "00000000000000000000000000000000000000AA" );
        std::string fileName = "test";
        path = dev::getDataDir() / "filestorage" / Address( ownerAddress ).hex() / fileName;
        data =
            ( "0x"
              "00000000000000000000000000000000000000000000000000000000000000AA"
              "0000000000000000000000000000000000000000000000000000000000000004"
              "7465737400000000000000000000000000000000000000000000000000000000"  // test
              "0000000000000000000000000000000000000000000000000000000000000004" );
    }

    ~RestrictedAddressFixture() override { remove( path.c_str() ); }

    Address ownerAddress;
    std::string data;
    boost::filesystem::path path;
};

string fromAscii( string _s ) {
    bytes b = asBytes( _s );
    return toHexPrefixed( b );
}
}  // namespace

BOOST_AUTO_TEST_SUITE( JsonRpcSuite )


BOOST_AUTO_TEST_CASE( jsonrpc_gasPrice ) {
    JsonRpcFixture fixture;
    string gasPrice = fixture.rpcClient->eth_gasPrice();
    BOOST_CHECK_EQUAL( gasPrice, toJS( 20 * dev::eth::shannon ) );
}

// SKALE disabled
// BOOST_AUTO_TEST_CASE(jsonrpc_isListening)
//{
//    web3->startNetwork();
//    bool listeningOn = rpcClient->net_listening();
//    BOOST_CHECK_EQUAL(listeningOn, web3->isNetworkStarted());
//
//    web3->stopNetwork();
//    bool listeningOff = rpcClient->net_listening();
//    BOOST_CHECK_EQUAL(listeningOff, web3->isNetworkStarted());
//}

BOOST_AUTO_TEST_CASE( jsonrpc_accounts,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    JsonRpcFixture fixture;
    std::vector< dev::KeyPair > keys = {KeyPair::create(), KeyPair::create()};
    fixture.accountHolder->setAccounts( keys );
    Json::Value k = fixture.rpcClient->eth_accounts();
    fixture.accountHolder->setAccounts( {} );
    BOOST_CHECK_EQUAL( k.isArray(), true );
    BOOST_CHECK_EQUAL( k.size(), keys.size() );
    for ( auto& i : k ) {
        auto it = std::find_if( keys.begin(), keys.end(), [i]( dev::KeyPair const& keyPair ) {
            return jsToAddress( i.asString() ) == keyPair.address();
        } );
        BOOST_CHECK_EQUAL( it != keys.end(), true );
    }
}

BOOST_AUTO_TEST_CASE( jsonrpc_number ) {
    JsonRpcFixture fixture;
    auto number = jsToU256( fixture.rpcClient->eth_blockNumber() );
    BOOST_CHECK_EQUAL( number, fixture.client->number() );
    dev::eth::mine( *( fixture.client ), 1 );
    auto numberAfter = jsToU256( fixture.rpcClient->eth_blockNumber() );
    BOOST_CHECK_GE( numberAfter, number + 1 );
    BOOST_CHECK_EQUAL( numberAfter, fixture.client->number() );
}

// SKALE disabled
// BOOST_AUTO_TEST_CASE(jsonrpc_peerCount)
//{
//    auto peerCount = jsToU256(rpcClient->net_peerCount());
//    BOOST_CHECK_EQUAL(web3->peerCount(), peerCount);
//}

// BOOST_AUTO_TEST_CASE(jsonrpc_setListening)
//{
//    rpcClient->admin_net_start(adminSession);
//    BOOST_CHECK_EQUAL(web3->isNetworkStarted(), true);
//
//    rpcClient->admin_net_stop(adminSession);
//    BOOST_CHECK_EQUAL(web3->isNetworkStarted(), false);
//}

BOOST_AUTO_TEST_CASE( jsonrpc_setMining ) {
    JsonRpcFixture fixture;
    fixture.rpcClient->admin_eth_setMining( true, fixture.adminSession );
    BOOST_CHECK_EQUAL( fixture.client->wouldSeal(), true );

    fixture.rpcClient->admin_eth_setMining( false, fixture.adminSession );
    BOOST_CHECK_EQUAL( fixture.client->wouldSeal(), false );
}

BOOST_AUTO_TEST_CASE( jsonrpc_stateAt ) {
    JsonRpcFixture fixture;
    dev::KeyPair key = KeyPair::create();
    auto address = key.address();
    string stateAt = fixture.rpcClient->eth_getStorageAt( toJS( address ), "0", "latest" );
    BOOST_CHECK_EQUAL( fixture.client->stateAt( address, 0 ), jsToU256( stateAt ) );
}

BOOST_AUTO_TEST_CASE( eth_coinbase,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    JsonRpcFixture fixture;
    string coinbase = fixture.rpcClient->eth_coinbase();
    BOOST_REQUIRE_EQUAL( jsToAddress( coinbase ), fixture.client->author() );
}

BOOST_AUTO_TEST_CASE( eth_sendTransaction ) {
    JsonRpcFixture fixture;
    auto address = fixture.coinbase.address();
    u256 countAt =
        jsToU256( fixture.rpcClient->eth_getTransactionCount( toJS( address ), "latest" ) );

    BOOST_CHECK_EQUAL( countAt, fixture.client->countAt( address ) );
    BOOST_CHECK_EQUAL( countAt, 0 );
    u256 balance = fixture.client->balanceAt( address );
    string balanceString = fixture.rpcClient->eth_getBalance( toJS( address ), "latest" );
    BOOST_CHECK_EQUAL( toJS( balance ), balanceString );
    BOOST_CHECK_EQUAL( jsToDecimal( balanceString ), "0" );

    dev::eth::simulateMining( *( fixture.client ), 1 );
    //    BOOST_CHECK_EQUAL(client->blockByNumber(LatestBlock).author(), address);
    balance = fixture.client->balanceAt( address );
    balanceString = fixture.rpcClient->eth_getBalance( toJS( address ), "latest" );

    BOOST_REQUIRE_GT( balance, 0 );
    BOOST_CHECK_EQUAL( toJS( balance ), balanceString );


    auto txAmount = balance / 2u;
    auto gasPrice = 10 * dev::eth::szabo;
    auto gas = EVMSchedule().txGas;

    auto receiver = KeyPair::create();

    Json::Value t;
    t["from"] = toJS( address );
    t["value"] = jsToDecimal( toJS( txAmount ) );
    t["to"] = toJS( receiver.address() );
    t["data"] = toJS( bytes() );
    t["gas"] = toJS( gas );
    t["gasPrice"] = toJS( gasPrice );

    std::string txHash = fixture.rpcClient->eth_sendTransaction( t );
    BOOST_REQUIRE( !txHash.empty() );

    fixture.accountHolder->setAccounts( {} );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    countAt = jsToU256( fixture.rpcClient->eth_getTransactionCount( toJS( address ), "latest" ) );
    u256 balance2 = fixture.client->balanceAt( receiver.address() );
    string balanceString2 =
        fixture.rpcClient->eth_getBalance( toJS( receiver.address() ), "latest" );

    BOOST_CHECK_EQUAL( countAt, fixture.client->countAt( address ) );
    BOOST_CHECK_EQUAL( countAt, 1 );
    BOOST_CHECK_EQUAL( toJS( balance2 ), balanceString2 );
    BOOST_CHECK_EQUAL( jsToU256( balanceString2 ), txAmount );
    BOOST_CHECK_EQUAL( txAmount, balance2 );
}

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_validTransaction,

    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    JsonRpcFixture fixture;
    auto senderAddress = fixture.coinbase.address();
    auto receiver = KeyPair::create();

    Json::Value t;
    t["from"] = toJS( senderAddress );
    t["to"] = toJS( receiver.address() );
    t["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    // Mine to generate a non-zero account balance
    const int blocksToMine = 1;
    const u256 blockReward = 3 * dev::eth::ether;
    cerr << "Reward: " << blockReward << endl;
    cerr << "Balance before: " << fixture.client->balanceAt( senderAddress ) << endl;
    dev::eth::simulateMining( *( fixture.client ), blocksToMine );
    cerr << "Balance after: " << fixture.client->balanceAt( senderAddress ) << endl;
    BOOST_CHECK_EQUAL( blockReward, fixture.client->balanceAt( senderAddress ) );

    auto signedTx = fixture.rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( !signedTx["raw"].empty() );

    auto txHash = fixture.rpcClient->eth_sendRawTransaction( signedTx["raw"].asString() );
    BOOST_REQUIRE( !txHash.empty() );
}

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_errorZeroBalance ) {
    JsonRpcFixture fixture;
    auto senderAddress = fixture.coinbase.address();
    auto receiver = KeyPair::create();

    BOOST_CHECK_EQUAL( 0, fixture.client->balanceAt( senderAddress ) );

    Json::Value t;
    t["from"] = toJS( senderAddress );
    t["to"] = toJS( receiver.address() );
    t["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    auto signedTx = fixture.rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( signedTx["raw"] );

    BOOST_CHECK_EQUAL( fixture.sendingRawShouldFail( signedTx["raw"].asString() ),
        "Account balance is too low (balance < value + gas * gas price)." );
}

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_errorInvalidNonce,

    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    JsonRpcFixture fixture;
    auto senderAddress = fixture.coinbase.address();
    auto receiver = KeyPair::create();

    // Mine to generate a non-zero account balance
    const size_t blocksToMine = 1;
    const u256 blockReward = 3 * dev::eth::ether;
    dev::eth::simulateMining( *( fixture.client ), blocksToMine );
    BOOST_CHECK_EQUAL( blockReward, fixture.client->balanceAt( senderAddress ) );

    Json::Value t;
    t["from"] = toJS( senderAddress );
    t["to"] = toJS( receiver.address() );
    t["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    auto signedTx = fixture.rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( !signedTx["raw"].empty() );

    auto txHash = fixture.rpcClient->eth_sendRawTransaction( signedTx["raw"].asString() );
    BOOST_REQUIRE( !txHash.empty() );

    mineTransaction( *fixture.client, 1 );

    auto invalidNonce =
        jsToU256( fixture.rpcClient->eth_getTransactionCount( toJS( senderAddress ), "latest" ) ) -
        1;
    t["nonce"] = jsToDecimal( toJS( invalidNonce ) );

    signedTx = fixture.rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( !signedTx["raw"].empty() );

    BOOST_CHECK_EQUAL(
        fixture.sendingRawShouldFail( signedTx["raw"].asString() ), "Invalid transaction nonce." );
}

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_errorInsufficientGas ) {
    JsonRpcFixture fixture;
    auto senderAddress = fixture.coinbase.address();
    auto receiver = KeyPair::create();

    // Mine to generate a non-zero account balance
    const int blocksToMine = 1;
    const u256 blockReward = 3 * dev::eth::ether;
    dev::eth::simulateMining( *( fixture.client ), blocksToMine );
    BOOST_CHECK_EQUAL( blockReward, fixture.client->balanceAt( senderAddress ) );

    Json::Value t;
    t["from"] = toJS( senderAddress );
    t["to"] = toJS( receiver.address() );
    t["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    const int minGasForValueTransferTx = 21000;
    t["gas"] = jsToDecimal( toJS( minGasForValueTransferTx - 1 ) );

    auto signedTx = fixture.rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( !signedTx["raw"].empty() );

    BOOST_CHECK_EQUAL( fixture.sendingRawShouldFail( signedTx["raw"].asString() ),
        "Transaction gas amount is less than the intrinsic gas amount for this transaction type." );
}

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_errorDuplicateTransaction ) {
    JsonRpcFixture fixture;
    auto senderAddress = fixture.coinbase.address();
    auto receiver = KeyPair::create();

    // Mine to generate a non-zero account balance
    const int blocksToMine = 1;
    const u256 blockReward = 3 * dev::eth::ether;
    dev::eth::simulateMining( *( fixture.client ), blocksToMine );
    BOOST_CHECK_EQUAL( blockReward, fixture.client->balanceAt( senderAddress ) );

    Json::Value t;
    t["from"] = toJS( senderAddress );
    t["to"] = toJS( receiver.address() );
    t["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );

    auto signedTx = fixture.rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( !signedTx["raw"].empty() );

    auto txHash = fixture.rpcClient->eth_sendRawTransaction( signedTx["raw"].asString() );
    BOOST_REQUIRE( !txHash.empty() );

    auto txNonce =
        jsToU256( fixture.rpcClient->eth_getTransactionCount( toJS( senderAddress ), "latest" ) );
    t["nonce"] = jsToDecimal( toJS( txNonce ) );

    signedTx = fixture.rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( !signedTx["raw"].empty() );

    BOOST_CHECK_EQUAL( fixture.sendingRawShouldFail( signedTx["raw"].asString() ),
        "Same transaction already exists in the pending transaction queue." );
}

BOOST_AUTO_TEST_CASE( eth_signTransaction ) {
    JsonRpcFixture fixture;
    auto address = fixture.coinbase.address();
    auto countAtBeforeSign =
        jsToU256( fixture.rpcClient->eth_getTransactionCount( toJS( address ), "latest" ) );
    auto receiver = KeyPair::create();

    Json::Value t;
    t["from"] = toJS( address );
    t["value"] = jsToDecimal( toJS( 1 ) );
    t["to"] = toJS( receiver.address() );

    Json::Value res = fixture.rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( res["raw"] );
    BOOST_REQUIRE( res["tx"] );

    fixture.accountHolder->setAccounts( {} );
    dev::eth::mine( *( fixture.client ), 1 );

    auto countAtAfterSign =
        jsToU256( fixture.rpcClient->eth_getTransactionCount( toJS( address ), "latest" ) );

    BOOST_CHECK_EQUAL( countAtBeforeSign, countAtAfterSign );
}

BOOST_AUTO_TEST_CASE( simple_contract ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );


    // contract test {
    //  function f(uint a) returns(uint d) { return a * 7; }
    // }

    string compiled =
        "6080604052341561000f57600080fd5b60b98061001d6000396000f300"
        "608060405260043610603f576000357c01000000000000000000000000"
        "00000000000000000000000000000000900463ffffffff168063b3de64"
        "8b146044575b600080fd5b3415604e57600080fd5b606a600480360381"
        "019080803590602001909291905050506080565b604051808281526020"
        "0191505060405180910390f35b60006007820290509190505600a16562"
        "7a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb9"
        "8fb480406fc2728a960029";

    Json::Value create;
    create["code"] = compiled;
    create["gas"] = "180000";  // TODO or change global default of 90000?

    BOOST_CHECK_EQUAL( jsToU256( fixture.rpcClient->eth_blockNumber() ), 0 );
    BOOST_CHECK_EQUAL( jsToU256( fixture.rpcClient->eth_getTransactionCount(
                           toJS( fixture.coinbase.address() ), "latest" ) ),
        0 );

    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( !receipt["contractAddress"].isNull() );
    string contractAddress = receipt["contractAddress"].asString();
    BOOST_REQUIRE( contractAddress != "null" );

    Json::Value call;
    call["to"] = contractAddress;
    call["data"] = "0xb3de648b0000000000000000000000000000000000000000000000000000000000000001";
    call["gas"] = "1000000";
    call["gasPrice"] = "0";
    string result = fixture.rpcClient->eth_call( call, "latest" );
    BOOST_CHECK_EQUAL(
        result, "0x0000000000000000000000000000000000000000000000000000000000000007" );
}

BOOST_AUTO_TEST_CASE(logs_range) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );

/*
pragma solidity >=0.4.10 <0.7.0;

contract Logger{
    fallback() external payable {
        log2(bytes32(block.number+1), bytes32(block.number), "dimalit");
    }
}
*/
    string bytecode =
        "6080604052348015600f57600080fd5b50607d80601d6000396000f3fe60806040527f64696d616c69740000000000000000000000000000000000000000000000000043600102600143016001026040518082815260200191505060405180910390a200fea2646970667358221220ecafb98cd573366a37976cb7a4489abe5389d1b5989cd7b7136c8eb0c5ba0b5664736f6c63430006000033";

    Json::Value create;
    create["code"] = bytecode;
    create["gas"] = "180000";  // TODO or change global default of 90000?

    string deployHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value deployReceipt = fixture.rpcClient->eth_getTransactionReceipt( deployHash );
    string contractAddress = deployReceipt["contractAddress"].asString();

    // need blockNumber==256 afterwards
    for(int i=0; i<255; ++i){
        Json::Value t;
        t["from"] = toJS( fixture.coinbase.address() );
        t["value"] = jsToDecimal( "0" );
        t["to"] = contractAddress;
        t["gas"] = "99000";

        std::string txHash = fixture.rpcClient->eth_sendTransaction( t );
        BOOST_REQUIRE( !txHash.empty() );

        dev::eth::mineTransaction( *( fixture.client ), 1 );
    }
    BOOST_REQUIRE_EQUAL(fixture.client->number(), 256);

    // ask for logs
    Json::Value t;
    t["fromBlock"] = 0;         // really 2
    t["toBlock"] = 250;
    t["address"] = contractAddress;
    Json::Value logs = fixture.rpcClient->eth_getLogs(t);
    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 249);

    // check logs
    for(size_t i=0; i<logs.size(); ++i){
        u256 block = dev::jsToU256( logs[(int)i]["topics"][0].asString() );
        BOOST_REQUIRE_EQUAL(block, i+2);
    }// for

}

BOOST_AUTO_TEST_CASE( deploy_contract_from_owner ) {
    JsonRpcFixture fixture( c_genesisConfigString );
    Address senderAddress = fixture.coinbase.address();

    fixture.client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    // contract test {
    //  function f(uint a) returns(uint d) { return a * 7; }
    // }

    string compiled =
        "6080604052341561000f57600080fd5b60b98061001d6000396000f300"
        "608060405260043610603f576000357c01000000000000000000000000"
        "00000000000000000000000000000000900463ffffffff168063b3de64"
        "8b146044575b600080fd5b3415604e57600080fd5b606a600480360381"
        "019080803590602001909291905050506080565b604051808281526020"
        "0191505060405180910390f35b60006007820290509190505600a16562"
        "7a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb9"
        "8fb480406fc2728a960029";

    Json::Value create;

    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "1000000";

    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );

    BOOST_REQUIRE_EQUAL( receipt["status"], string( "1" ) );
    BOOST_REQUIRE( !receipt["contractAddress"].isNull() );
    Json::Value code =
        fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString().substr( 2 ) == compiled.substr( 58 ) );
}

BOOST_AUTO_TEST_CASE( deploy_contract_not_from_owner ) {
    JsonRpcFixture fixture( c_genesisConfigString, false );
    auto senderAddress = fixture.coinbase.address();

    fixture.client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    // contract test {
    //  function f(uint a) returns(uint d) { return a * 7; }
    // }

    string compiled =
        "6080604052341561000f57600080fd5b60b98061001d6000396000f300"
        "608060405260043610603f576000357c01000000000000000000000000"
        "00000000000000000000000000000000900463ffffffff168063b3de64"
        "8b146044575b600080fd5b3415604e57600080fd5b606a600480360381"
        "019080803590602001909291905050506080565b604051808281526020"
        "0191505060405180910390f35b60006007820290509190505600a16562"
        "7a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb9"
        "8fb480406fc2728a960029";

    Json::Value create;

    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "1000000";

    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_CHECK_EQUAL( receipt["status"], string( "0" ) );
    Json::Value code =
        fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString() == "0x" );
}

BOOST_AUTO_TEST_CASE( deploy_contract_true_flag ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    ret["skaleConfig"]["sChain"]["freeContractDeployment"] = true;
    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config, false );
    auto senderAddress = fixture.coinbase.address();

    fixture.client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    // contract test {
    //  function f(uint a) returns(uint d) { return a * 7; }
    // }

    string compiled =
            "6080604052341561000f57600080fd5b60b98061001d6000396000f300"
            "608060405260043610603f576000357c01000000000000000000000000"
            "00000000000000000000000000000000900463ffffffff168063b3de64"
            "8b146044575b600080fd5b3415604e57600080fd5b606a600480360381"
            "019080803590602001909291905050506080565b604051808281526020"
            "0191505060405180910390f35b60006007820290509190505600a16562"
            "7a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb9"
            "8fb480406fc2728a960029";

    Json::Value create;

    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "1000000";

    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "1" ) );
    BOOST_REQUIRE( !receipt["contractAddress"].isNull() );
    Json::Value code =
            fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString().substr( 2 ) == compiled.substr( 58 ) );
}

BOOST_AUTO_TEST_CASE( deploy_contract_false_flag ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    ret["skaleConfig"]["sChain"]["freeContractDeployment"] = false;
    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config, false );
    auto senderAddress = fixture.coinbase.address();

    fixture.client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    // contract test {
    //  function f(uint a) returns(uint d) { return a * 7; }
    // }

    string compiled =
            "6080604052341561000f57600080fd5b60b98061001d6000396000f300"
            "608060405260043610603f576000357c01000000000000000000000000"
            "00000000000000000000000000000000900463ffffffff168063b3de64"
            "8b146044575b600080fd5b3415604e57600080fd5b606a600480360381"
            "019080803590602001909291905050506080565b604051808281526020"
            "0191505060405180910390f35b60006007820290509190505600a16562"
            "7a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb9"
            "8fb480406fc2728a960029";

    Json::Value create;

    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "1000000";

    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_CHECK_EQUAL( receipt["status"], string( "0" ) );
    Json::Value code =
            fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString() == "0x" );
}

BOOST_AUTO_TEST_CASE( create_opcode ) {
    JsonRpcFixture fixture( c_genesisConfigString );
    auto senderAddress = fixture.coinbase.address();

    fixture.client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    /*
        pragma solidity ^0.4.25;

        contract test {
            address public a;

            function f() public {
                address _address;
                assembly {
                    let ptr := mload(0x40)
                    _address := create(0x00,ptr,0x20)
                }
                a = _address;
            }
        }
    */

    string compiled =
        "608060405234801561001057600080fd5b50610161806100206000396000f30060806040526004361061004c57"
        "6000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff1680630dbe"
        "671f1461005157806326121ff0146100a8575b600080fd5b34801561005d57600080fd5b506100666100bf565b"
        "604051808273ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffff"
        "ffffff16815260200191505060405180910390f35b3480156100b457600080fd5b506100bd6100e4565b005b60"
        "00809054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b600060405160208160"
        "00f0915050806000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffff"
        "ffffffffffffffffffffffffffffffffffff160217905550505600a165627a7a72305820fc6f465560bc93346a"
        "25f87ff189a58c26f5bf6f2e46570058fd79c1a3c3063a0029";

    Json::Value create;

    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "1000000";

    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress = receipt["contractAddress"].asString();

    fixture.client->setAuthor( fixture.account2.address() );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    Json::Value transactionCallObject;

    transactionCallObject["from"] = toJS( fixture.account2.address() );
    transactionCallObject["to"] = contractAddress;
    transactionCallObject["data"] = "0x26121ff0";

    fixture.rpcClient->eth_sendTransaction( transactionCallObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value checkAddress;
    checkAddress["to"] = contractAddress;
    checkAddress["data"] = "0x0dbe671f";
    string response = fixture.rpcClient->eth_call( checkAddress, "latest" );
    BOOST_CHECK( response == "0x0000000000000000000000000000000000000000000000000000000000000000" );

    fixture.client->setAuthor( senderAddress );
    transactionCallObject["from"] = toJS( senderAddress );
    fixture.rpcClient->eth_sendTransaction( transactionCallObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    response = fixture.rpcClient->eth_call( checkAddress, "latest" );
    BOOST_CHECK( response != "0x0000000000000000000000000000000000000000000000000000000000000000" );
}

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_gasLimitExceeded ) {
    JsonRpcFixture fixture;
    auto senderAddress = fixture.coinbase.address();
    dev::eth::simulateMining( *( fixture.client ), 1 );

    // We change author because coinbase.address() is author address by default
    // and will take all transaction fee after execution so we can't check money spent
    // for senderAddress correctly.
    fixture.client->setAuthor( Address( 5 ) );

    // contract test {
    //  function f(uint a) returns(uint d) { return a * 7; }
    // }

    string compiled =
        "6080604052341561000f57600080fd5b60b98061001d6000396000f300"
        "608060405260043610603f576000357c01000000000000000000000000"
        "00000000000000000000000000000000900463ffffffff168063b3de64"
        "8b146044575b600080fd5b3415604e57600080fd5b606a600480360381"
        "019080803590602001909291905050506080565b604051808281526020"
        "0191505060405180910390f35b60006007820290509190505600a16562"
        "7a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb9"
        "8fb480406fc2728a960029";

    Json::Value create;
    int gas = 82000;                   // not enough but will pass size check
    string gasPrice = "100000000000";  // 100b
    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = gas;
    create["gasPrice"] = gasPrice;

    BOOST_CHECK_EQUAL( jsToU256( fixture.rpcClient->eth_blockNumber() ), 0 );
    BOOST_CHECK_EQUAL( jsToU256( fixture.rpcClient->eth_getTransactionCount(
                           toJS( fixture.coinbase.address() ), "latest" ) ),
        0 );

    u256 balanceBefore = jsToU256(
        fixture.rpcClient->eth_getBalance( toJS( fixture.coinbase.address() ), "latest" ) );

    BOOST_REQUIRE_EQUAL( jsToU256( fixture.rpcClient->eth_getTransactionCount(
                             toJS( fixture.coinbase.address() ), "latest" ) ),
        0 );

    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    u256 balanceAfter = jsToU256(
        fixture.rpcClient->eth_getBalance( toJS( fixture.coinbase.address() ), "latest" ) );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );

    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0" ) );
    BOOST_REQUIRE_EQUAL( balanceBefore - balanceAfter, u256( gas ) * u256( gasPrice ) );
}

BOOST_AUTO_TEST_CASE( contract_storage ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );


    // pragma solidity ^0.4.22;

    // contract test
    // {
    //     uint hello;
    //     function writeHello(uint value) returns(bool d)
    //     {
    //       hello = value;
    //       return true;
    //     }
    // }


    string compiled =
        "6080604052341561000f57600080fd5b60c28061001d6000396000f3006"
        "08060405260043610603f576000357c0100000000000000000000000000"
        "000000000000000000000000000000900463ffffffff16806315b2eec31"
        "46044575b600080fd5b3415604e57600080fd5b606a6004803603810190"
        "80803590602001909291905050506084565b60405180821515151581526"
        "0200191505060405180910390f35b600081600081905550600190509190"
        "505600a165627a7a72305820d8407d9cdaaf82966f3fa7a3e665b8cf4e6"
        "5ee8909b83094a3f856b9051274500029";

    Json::Value create;
    create["code"] = compiled;
    create["gas"] = "180000";  // TODO or change global default of 90000?
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( !receipt["contractAddress"].isNull() );
    string contractAddress = receipt["contractAddress"].asString();
    BOOST_REQUIRE( contractAddress != "null" );

    Json::Value transact;
    transact["to"] = contractAddress;
    transact["data"] = "0x15b2eec30000000000000000000000000000000000000000000000000000000000000003";
    string txHash2 = fixture.rpcClient->eth_sendTransaction( transact );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    string storage = fixture.rpcClient->eth_getStorageAt( contractAddress, "0", "latest" );
    BOOST_CHECK_EQUAL(
        storage, "0x0000000000000000000000000000000000000000000000000000000000000003" );

    Json::Value receipt2 = fixture.rpcClient->eth_getTransactionReceipt( txHash2 );
    string contractAddress2 = receipt2["contractAddress"].asString();
    BOOST_REQUIRE( receipt2["contractAddress"].isNull() );
}

BOOST_AUTO_TEST_CASE( web3_sha3,
    *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    JsonRpcFixture fixture;
    string testString = "multiply(uint256)";
    h256 expected = dev::sha3( testString );

    auto hexValue = fromAscii( testString );
    string result = fixture.rpcClient->web3_sha3( hexValue );
    BOOST_CHECK_EQUAL( toJS( expected ), result );
    BOOST_CHECK_EQUAL(
        "0xc6888fa159d67f77c2f3d7a402e199802766bd7e8d4d1ecd2274fc920265d56a", result );
}

// SKALE disabled
// BOOST_AUTO_TEST_CASE(debugAccountRangeAtFinalBlockState)
//{
//    // mine to get some balance at coinbase
//    dev::eth::mine(*(client), 1);

//    // send transaction to have non-emtpy block
//    Address receiver = Address::random();
//    Json::Value tx;
//    tx["from"] = toJS(coinbase.address());
//    tx["value"] = toJS(10);
//    tx["to"] = toJS(receiver);
//    tx["gas"] = toJS(EVMSchedule().txGas);
//    tx["gasPrice"] = toJS(10 * dev::eth::szabo);
//    string txHash = rpcClient->eth_sendTransaction(tx);
//    BOOST_REQUIRE(!txHash.empty());

//    dev::eth::mineTransaction(*(client), 1);

//    string receiverHash = toString(sha3(receiver));

//    // receiver doesn't exist in the beginning of the 2nd block
//    Json::Value result = rpcClient->debug_accountRangeAt("2", 0, "0", 100);
//    BOOST_CHECK(!result["addressMap"].isMember(receiverHash));

//    // receiver exists in the end of the 2nd block
//    result = rpcClient->debug_accountRangeAt("2", 1, "0", 100);
//    BOOST_CHECK(result["addressMap"].isMember(receiverHash));
//    BOOST_CHECK_EQUAL(result["addressMap"][receiverHash], toString(receiver));
//}

// SKALE disabled
// BOOST_AUTO_TEST_CASE(debugStorageRangeAtFinalBlockState)
//{
//    // mine to get some balance at coinbase
//    dev::eth::mine(*(client), 1);

//    // pragma solidity ^0.4.22;
//    // contract test
//    //{
//    //    uint hello = 7;
//    //}
//    string initCode =
//        "608060405260076000553415601357600080fd5b60358060206000396000"
//        "f3006080604052600080fd00a165627a7a7230582006db0551577963b544"
//        "3e9501b4b10880e186cff876cd360e9ad6e4181731fcdd0029";

//    Json::Value tx;
//    tx["code"] = initCode;
//    tx["from"] = toJS(coinbase.address());
//    string txHash = rpcClient->eth_sendTransaction(tx);

//    dev::eth::mineTransaction(*(client), 1);

//    Json::Value receipt = rpcClient->eth_getTransactionReceipt(txHash);
//    string contractAddress = receipt["contractAddress"].asString();

//    // contract doesn't exist in the beginning of the 2nd block
//    Json::Value result = rpcClient->debug_storageRangeAt("2", 0, contractAddress, "0", 100);
//    BOOST_CHECK(result["storage"].empty());

//    // contracts exists in the end of the 2nd block
//    result = rpcClient->debug_storageRangeAt("2", 1, contractAddress, "0", 100);
//    BOOST_CHECK(!result["storage"].empty());
//    string keyHash = toJS(sha3(u256{0}));
//    BOOST_CHECK(!result["storage"][keyHash].empty());
//    BOOST_CHECK_EQUAL(result["storage"][keyHash]["key"].asString(), "0x00");
//    BOOST_CHECK_EQUAL(result["storage"][keyHash]["value"].asString(), "0x07");
//}

BOOST_AUTO_TEST_CASE( test_importRawBlock ) {
    JsonRpcFixture fixture( c_genesisConfigString );
    string blockHash = fixture.rpcClient->test_importRawBlock(
        "0xf90279f9020ea0"
        //        "c92211c9cd49036c37568feedb8e518a24a77e9f6ca959931a19dcf186a8e1e6"
        // TODO this is our genesis (with stateRoot=1!) hash - just generated from code; need to
        // check it by hands
        "b449751a1ccedfcdae41640170e1712e8100d45061e6945f8fc7f556034d61ea"
        "a01dcc4de8"
        "dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347942adc25665018aa1fe0e6bc666dac8fc2"
        "697ff9baa0328f16ca7b0259d7617b3ddf711c107efe6d5785cbeb11a8ed1614b484a6bc3aa093ca2a18d52e7c"
        "1846f7b104e2fc1e5fdc71ebe38187248f9437d39e74f43aaba0f5a4cad211681b78d25e6fde8dea45961dd1d2"
        "22a43e4d75e3b7733e50889203b901000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00008304000001830f460f82a0348203e897d68094312e342e302b2b62383163726c696e7578676e75a08e2042"
        "e00086a18e2f095bc997dc11d1c93fcf34d0540a428ee95869a4a62264883f8fd3f43a3567c3f865f863800183"
        "061a8094095e7baea6a6c7c4c2dfeb977efac326af552d87830186a0801ca0e94818d1f3b0c69eb37720145a5e"
        "ad7fbf6f8d80139dd53953b4a782301050a3a01fcf46908c01576715411be0857e30027d6be3250a3653f049b3"
        "ff8d74d2540cc0" );

    // blockHash, "0xedef94eddd6002ae14803b91aa5138932f948026310144fc615d52d7d5ff29c7" );
    // TODO again, this was computed just in code - no trust to it
    std::cerr << blockHash << std::endl;
    BOOST_CHECK_EQUAL(
        blockHash, "0x7683f686a7ecf6949d29cab2075b8aa45f061e27338e61ea3c37a7a0bd80f17b" );
}

BOOST_AUTO_TEST_CASE( call_from_parameter ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );


    //    pragma solidity ^0.5.1;

    //    contract Test
    //    {

    //        function whoAmI() public view returns (address) {
    //            return msg.sender;
    //        }
    //    }


    string compiled =
        "608060405234801561001057600080fd5b5060c68061001f6000396000f"
        "3fe6080604052600436106039576000357c010000000000000000000000"
        "000000000000000000000000000000000090048063da91254c14603e575"
        "b600080fd5b348015604957600080fd5b5060506092565b604051808273"
        "ffffffffffffffffffffffffffffffffffffffff1673fffffffffffffff"
        "fffffffffffffffffffffffff16815260200191505060405180910390f3"
        "5b60003390509056fea165627a7a72305820abfa953fead48d8f657bca6"
        "57713501650734d40342585cafcf156a3fe1f41d20029";
    
    auto senderAddress = fixture.coinbase.address();

    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "180000";  // TODO or change global default of 90000?
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress = receipt["contractAddress"].asString();

    Json::Value transactionCallObject;
    transactionCallObject["to"] = contractAddress;
    transactionCallObject["data"] = "0xda91254c";

    fixture.accountHolder->setAccounts( vector< dev::KeyPair >() );

    string responseString = fixture.rpcClient->eth_call( transactionCallObject, "latest" );
    BOOST_CHECK_EQUAL(
        responseString, "0x0000000000000000000000000000000000000000000000000000000000000000" );

    transactionCallObject["from"] = "0x112233445566778899aabbccddeeff0011223344";

    responseString = fixture.rpcClient->eth_call( transactionCallObject, "latest" );
    BOOST_CHECK_EQUAL(
        responseString, "0x000000000000000000000000112233445566778899aabbccddeeff0011223344" );
}

BOOST_AUTO_TEST_CASE( transactionWithoutFunds ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );

    // pragma solidity ^0.4.22;

    // contract test
    // {
    //     uint hello;
    //     function writeHello(uint value) returns(bool d)
    //     {
    //       hello = value;
    //       return true;
    //     }
    // }


    string compiled =
        "6080604052341561000f57600080fd5b60c28061001d6000396000f3006"
        "08060405260043610603f576000357c0100000000000000000000000000"
        "000000000000000000000000000000900463ffffffff16806315b2eec31"
        "46044575b600080fd5b3415604e57600080fd5b606a6004803603810190"
        "80803590602001909291905050506084565b60405180821515151581526"
        "0200191505060405180910390f35b600081600081905550600190509190"
        "505600a165627a7a72305820d8407d9cdaaf82966f3fa7a3e665b8cf4e6"
        "5ee8909b83094a3f856b9051274500029";
    
    auto senderAddress = fixture.coinbase.address();

    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "180000";  // TODO or change global default of 90000?
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress = receipt["contractAddress"].asString();

    Address address2 = fixture.account2.address();
    string balanceString = fixture.rpcClient->eth_getBalance( toJS( address2 ), "latest" );
    BOOST_REQUIRE_EQUAL( toJS( 0 ), balanceString );

    u256 powGasPrice = 0;
    do {
        const u256 GAS_PER_HASH = 1;
        u256 candidate = h256::random();
        h256 hash = dev::sha3( address2 ) ^ dev::sha3( u256( 0 ) ) ^ dev::sha3( candidate );
        u256 externalGas = ~u256( 0 ) / u256( hash ) * GAS_PER_HASH;
        if ( externalGas >= 21000 + 21000 ) {
            powGasPrice = candidate;
        }
    } while ( !powGasPrice );

    Json::Value transact;
    transact["from"] = toJS( address2 );
    transact["to"] = contractAddress;
    transact["gasPrice"] = toJS( powGasPrice );
    transact["data"] = "0x15b2eec30000000000000000000000000000000000000000000000000000000000000003";
    fixture.rpcClient->eth_sendTransaction( transact );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    string storage = fixture.rpcClient->eth_getStorageAt( contractAddress, "0", "latest" );
    BOOST_CHECK_EQUAL(
        storage, "0x0000000000000000000000000000000000000000000000000000000000000003" );

    balanceString = fixture.rpcClient->eth_getBalance( toJS( address2 ), "latest" );
    BOOST_REQUIRE_EQUAL( toJS( 0 ), balanceString );
}

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_gasPriceTooLow ) {
    JsonRpcFixture fixture;
    auto senderAddress = fixture.coinbase.address();
    auto receiver = KeyPair::create();

    // Mine to generate a non-zero account balance
    const int blocksToMine = 1;
    const u256 blockReward = 3 * dev::eth::ether;
    dev::eth::simulateMining( *( fixture.client ), blocksToMine );
    BOOST_CHECK_EQUAL( blockReward, fixture.client->balanceAt( senderAddress ) );

    u256 initial_gasPrice = fixture.client->gasBidPrice();

    Json::Value t;
    t["from"] = toJS( senderAddress );
    t["to"] = toJS( receiver.address() );
    t["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    t["gasPrice"] = jsToDecimal( toJS( initial_gasPrice ) );

    auto signedTx = fixture.rpcClient->eth_signTransaction( t );
    BOOST_REQUIRE( !signedTx["raw"].empty() );

    auto txHash = fixture.rpcClient->eth_sendRawTransaction( signedTx["raw"].asString() );
    BOOST_REQUIRE( !txHash.empty() );

    mineTransaction( *fixture.client, 1 );
    BOOST_REQUIRE_EQUAL(
        fixture.rpcClient->eth_getTransactionCount( toJS( senderAddress ), "latest" ), "0x1" );


    /////////////////////////

    t["nonce"] = "1";
    t["gasPrice"] = jsToDecimal( toJS( initial_gasPrice - 1 ) );
    auto signedTx2 = fixture.rpcClient->eth_signTransaction( t );

    BOOST_CHECK_EQUAL( fixture.sendingRawShouldFail( signedTx2["raw"].asString() ),
        "Transaction gas price lower than current eth_gasPrice." );
}

BOOST_AUTO_TEST_CASE( storage_limit ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 10 );
    
//pragma solidity 0.4.25;

//contract TestStorageLimit {

//    uint[] public storageArray;

//    function store(uint256 num) public {
//        storageArray.push( num );
//    }

//    function erase(uint256 index) public {
//        delete storageArray[index];
//    }

//    function foo() public view {
//        uint len = storageArray.length;
//        storageArray.push(1);
//    }

//    function storeAndCall(uint256 num) public {
//        storageArray.push( num );
//        foo();
//    }
//}
    
    std::string bytecode = "608060405234801561001057600080fd5b5061025f806100206000396000f30060806040526004361061006d576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff1680630e031ab1146100725780631007f753146100b35780636057361d146100e0578063c29855781461010d578063c67cd88414610124575b600080fd5b34801561007e57600080fd5b5061009d60048036038101908080359060200190929190505050610151565b6040518082815260200191505060405180910390f35b3480156100bf57600080fd5b506100de60048036038101908080359060200190929190505050610174565b005b3480156100ec57600080fd5b5061010b60048036038101908080359060200190929190505050610194565b005b34801561011957600080fd5b506101226101c3565b005b34801561013057600080fd5b5061014f600480360381019080803590602001909291905050506101fc565b005b60008181548110151561016057fe5b906000526020600020016000915090505481565b60008181548110151561018357fe5b906000526020600020016000905550565b600081908060018154018082558091505090600182039060005260206000200160009091929091909150555050565b60008080549050905060006001908060018154018082558091505090600182039060005260206000200160009091929091909150555050565b60008190806001815401808255809150509060018203906000526020600020016000909192909190915055506102306101c3565b505600a165627a7a72305820be696e2dc855e7b87426f0fa32eba10bb619ff335830c4465f654520ff8ef3240029";
    
    auto senderAddress = fixture.coinbase.address();
    
    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["data"] = bytecode;
    create["gas"] = "1800000";  // TODO or change global default of 90000?
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress = receipt["contractAddress"].asString();
    dev::Address contract = dev::Address( contractAddress );

    Json::Value txCall;  // call foo()
    txCall["to"] = contractAddress;
    txCall["data"] = "0xc2985578";
    txCall["from"] = toJS( senderAddress );
    txCall["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_call( txCall, "latest" );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 0 );

    Json::Value txPushValueAndCall; // call storeAndCall(1)
    txPushValueAndCall["to"] = contractAddress;
    txPushValueAndCall["data"] = "0xc67cd8840000000000000000000000000000000000000000000000000000000000000001";
    txPushValueAndCall["from"] = toJS( senderAddress );
    txPushValueAndCall["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txPushValueAndCall );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 96 );
    
    Json::Value txPushValue;  // call store(2)
    txPushValue["to"] = contractAddress;
    txPushValue["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000002";
    txPushValue["from"] = toJS( senderAddress );
    txPushValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txPushValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 128 );
    
    Json::Value txThrow;  // trying to call store(3)
    txThrow["to"] = contractAddress;
    txThrow["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000003";
    txThrow["from"] = toJS( senderAddress );
    txThrow["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txThrow );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 128 );
    
    Json::Value txEraseValue;  // call erase(2)
    txEraseValue["to"] = contractAddress;
    txEraseValue["data"] = "0x1007f7530000000000000000000000000000000000000000000000000000000000000002";
    txEraseValue["from"] = toJS( senderAddress );
    txEraseValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txEraseValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 96 );
}

BOOST_FIXTURE_TEST_SUITE( RestrictedAddressSuite, RestrictedAddressFixture )

BOOST_AUTO_TEST_CASE( direct_call ) {
    Json::Value transactionCallObject;
    transactionCallObject["to"] = "0x0000000000000000000000000000000000000005";
    transactionCallObject["data"] = data;

    rpcClient->eth_call( transactionCallObject, "latest" );
    BOOST_REQUIRE( !boost::filesystem::exists( path ) );

    transactionCallObject["from"] = "0xdeadbeef01234567896c27aa97d1a86395877b3a";
    rpcClient->eth_call( transactionCallObject, "latest" );
    BOOST_REQUIRE( !boost::filesystem::exists( path ) );

    transactionCallObject["from"] = "0x692a70d2e424a56d2c6c27aa97d1a86395877b3a";
    rpcClient->eth_call( transactionCallObject, "latest" );
    BOOST_REQUIRE( !boost::filesystem::exists( path ) );
}

BOOST_AUTO_TEST_CASE( transaction_from_restricted_address ) {
    auto senderAddress = coinbase.address();
    client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( client ), 1000 );

    Json::Value transactionCallObject;
    transactionCallObject["from"] = toJS( senderAddress );
    transactionCallObject["to"] = "0x0000000000000000000000000000000000000005";
    transactionCallObject["data"] = data;

    TransactionSkeleton ts = toTransactionSkeleton( transactionCallObject );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );
    auto txHash = rpcClient->eth_sendRawTransaction( toJS( stream.out() ) );
    dev::eth::mineTransaction( *( client ), 1 );

    BOOST_REQUIRE( !boost::filesystem::exists( path ) );
}

BOOST_AUTO_TEST_CASE( transaction_from_allowed_address ) {
    auto senderAddress = coinbase.address();
    client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( client ), 1000 );

    Json::Value transactionCallObject;
    transactionCallObject["from"] = toJS( senderAddress );
    transactionCallObject["to"] = "0x692a70d2e424a56d2c6c27aa97d1a86395877b3a";
    transactionCallObject["data"] = "0x28b5e32b";

    TransactionSkeleton ts = toTransactionSkeleton( transactionCallObject );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );
    auto txHash = rpcClient->eth_sendRawTransaction( toJS( stream.out() ) );
    dev::eth::mineTransaction( *( client ), 1 );

    BOOST_REQUIRE( boost::filesystem::exists( path ) );
}

BOOST_AUTO_TEST_CASE( delegate_call ) {
    auto senderAddress = coinbase.address();
    client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( client ), 1000 );

    // pragma solidity ^0.4.25;
    //
    // contract Caller {
    //     function call() public view {
    //         bool status;
    //         string memory fileName = "test";
    //         address sender = 0x000000000000000000000000000000AA;
    //         assembly{
    //                 let ptr := mload(0x40)
    //                 mstore(ptr, sender)
    //                 mstore(add(ptr, 0x20), 4)
    //                 mstore(add(ptr, 0x40), mload(add(fileName, 0x20)))
    //                 mstore(add(ptr, 0x60), 1)
    //                 status := delegatecall(not(0), 0x05, ptr, 0x80, ptr, 32)
    //         }
    //     }
    // }

    string compiled =
        "6080604052348015600f57600080fd5b5060f88061001e6000396000f300608060405260043610603f57600035"
        "7c0100000000000000000000000000000000000000000000000000000000900463ffffffff16806328b5e32b14"
        "6044575b600080fd5b348015604f57600080fd5b5060566058565b005b60006060600060408051908101604052"
        "80600481526020017f746573740000000000000000000000000000000000000000000000000000000081525091"
        "5060aa905060405181815260046020820152602083015160408201526001606082015260208160808360056000"
        "19f49350505050505600a165627a7a72305820172a27e3e21f45218a47c53133bb33150ee9feac9e9d5d13294b"
        "48b03773099a0029";

    Json::Value create;

    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "1000000";

    TransactionSkeleton ts = toTransactionSkeleton( create );
    ts = client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );
    auto txHash = rpcClient->eth_sendRawTransaction( toJS( stream.out() ) );
    dev::eth::mineTransaction( *( client ), 1 );

    Json::Value receipt = rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress = receipt["contractAddress"].asString();

    Json::Value transactionCallObject;
    transactionCallObject["to"] = contractAddress;
    transactionCallObject["data"] = "0x28b5e32b";

    rpcClient->eth_call( transactionCallObject, "latest" );
    BOOST_REQUIRE( !boost::filesystem::exists( path ) );

    transactionCallObject["from"] = ownerAddress.hex();
    rpcClient->eth_call( transactionCallObject, "latest" );
    BOOST_REQUIRE( !boost::filesystem::exists( path ) );

    transactionCallObject["to"] = "0x692a70d2e424a56d2c6c27aa97d1a86395877b3a";
    rpcClient->eth_call( transactionCallObject, "latest" );
    BOOST_REQUIRE( !boost::filesystem::exists( path ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
