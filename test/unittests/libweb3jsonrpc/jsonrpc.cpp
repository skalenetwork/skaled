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
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/TransientDirectory.h>
#include <libethcore/CommonJS.h>
#include <libethcore/KeyManager.h>
#include <libethereum/ChainParams.h>
#include <libethereum/ClientTest.h>
#include <libethereum/TransactionQueue.h>
#include <libp2p/Network.h>
#include <libskale/httpserveroverride.h>
#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/AdminEth.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include "genesisGeneration2Config.h"
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
#include <libweb3jsonrpc/rapidjson_handlers.h>

#include <libconsensus/SkaleCommon.h>
#include <libconsensus/oracle/OracleRequestSpec.h>

#include <cstdlib>

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
         "byzantiumForkBlock": "0x00",
         "constantinopleForkBlock": "0x00",
         "skaleDisableChainIdCheck": true
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
            "logLevelProposal": "trace",
            "ecdsaKeyName": "NEK:fa112"
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "contractStorageLimit": 128,
            "emptyBlockIntervalMs": -1,
            "nodeGroups": {},
            "nodes": [
                { "nodeID": 1112, "ip": "127.0.0.1", "basePort": 1231, "schainIndex" : 1, "publicKey": "0xfa"}
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
    R"("0000000000000000000000000000000000000006": {
            "precompiled": {
                "name": "addBalance",
                "linear": {
                    "base": 15,
                    "word": 0
                },
                "restrictAccess": ["5c4e11842e8be09264dc1976943571d7af6d00f9"]
            }
        },
        "0000000000000000000000000000000000000007": {
            "precompiled": {
                "name": "getIMABLSPublicKey",
                "linear": {
                    "base": 15,
                    "word": 0
                }
            }
        },
        "0x5c4e11842e8be09264dc1976943571d7af6d00f9" : {
            "balance" : "1000000000000000000000000000000"
        },
        "0x692a70d2e424a56d2c6c27aa97d1a86395877b3a" : {
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
        "0xC2002000000000000000000000000000000000C2": {
            "balance": "0",
            "code": "0x6080604052348015600f57600080fd5b506004361060325760003560e01c80639b063104146037578063cd16ecbf146062575b600080fd5b606060048036036020811015604b57600080fd5b8101908080359060200190929190505050608d565b005b608b60048036036020811015607657600080fd5b81019080803590602001909291905050506097565b005b8060018190555050565b806000819055505056fea265627a7a7231582029df540a7555533ef4b3f66bc4f9abe138b00117d1496efbfd9d035a48cd595e64736f6c634300050d0032",
            "storage": {
				"0x0": "0x01"
			},
            "nonce": "0"
        },
        "0xD2002000000000000000000000000000000000D2": {
            "balance": "0",
            "code": "0x608060405234801561001057600080fd5b50600436106100455760003560e01c806313f44d101461005557806338eada1c146100af5780634ba79dfe146100f357610046565b5b6002801461005357600080fd5b005b6100976004803603602081101561006b57600080fd5b81019080803573ffffffffffffffffffffffffffffffffffffffff169060200190929190505050610137565b60405180821515815260200191505060405180910390f35b6100f1600480360360208110156100c557600080fd5b81019080803573ffffffffffffffffffffffffffffffffffffffff1690602001909291905050506101f4565b005b6101356004803603602081101561010957600080fd5b81019080803573ffffffffffffffffffffffffffffffffffffffff16906020019092919050505061030f565b005b60008060009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff168273ffffffffffffffffffffffffffffffffffffffff16148061019957506101988261042b565b5b806101ed5750600160008373ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200190815260200160002060009054906101000a900460ff165b9050919050565b60008054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff163373ffffffffffffffffffffffffffffffffffffffff16146102b5576040517f08c379a00000000000000000000000000000000000000000000000000000000081526004018080602001828103825260178152602001807f43616c6c6572206973206e6f7420746865206f776e657200000000000000000081525060200191505060405180910390fd5b60018060008373ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200190815260200160002060006101000a81548160ff02191690831515021790555050565b60008054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff163373ffffffffffffffffffffffffffffffffffffffff16146103d0576040517f08c379a00000000000000000000000000000000000000000000000000000000081526004018080602001828103825260178152602001807f43616c6c6572206973206e6f7420746865206f776e657200000000000000000081525060200191505060405180910390fd5b6000600160008373ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200190815260200160002060006101000a81548160ff02191690831515021790555050565b600080823b90506000811191505091905056fea26469706673582212202aca1f7abb7d02061b58de9b559eabe1607c880fda3932bbdb2b74fa553e537c64736f6c634300060c0033",
            "storage": {
			},
            "nonce": "0"
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
    JsonRpcFixture( const std::string& _config = "", bool _owner = true, bool _deploymentControl = true, bool _generation2 = false, bool _mtmEnabled = false ) {
        dev::p2p::NetworkPreferences nprefs;
        ChainParams chainParams;

        if ( _config != "" ) {
            if ( !_generation2 ) {
                Json::Value ret;
                Json::Reader().parse( _config, ret );
                if ( _owner ) {
                    ret["skaleConfig"]["sChain"]["schainOwner"] = toJS( coinbase.address() );
                    if (_deploymentControl)
                        ret["accounts"]["0xD2002000000000000000000000000000000000D2"]["storage"]["0x0"] = toJS( coinbase.address() );
                } else {
                    ret["skaleConfig"]["sChain"]["schainOwner"] = toJS( account2.address() );
                    if (_deploymentControl)
                        ret["accounts"]["0xD2002000000000000000000000000000000000D2"]["storage"]["0x0"] = toJS( account2.address() );
                }
                Json::FastWriter fastWriter;
                std::string output = fastWriter.write( ret );
                chainParams = chainParams.loadConfig( output );
            } else {
                chainParams = chainParams.loadConfig( _config );
                // insecure schain owner(originator) private key
                // address is 0x5C4e11842E8be09264dc1976943571d7Af6d00F9
                coinbase = dev::KeyPair(dev::Secret("0x1c2cd4b70c2b8c6cd7144bbbfbd1e5c6eacb4a5efd9c86d0e29cbbec4e8483b9"));
                account3 = dev::KeyPair(dev::Secret("0x23ABDBD3C61B5330AF61EBE8BEF582F4E5CC08E554053A718BDCE7813B9DC1FC"));
            }
        } else {
            chainParams.sealEngineName = NoProof::name();
            chainParams.allowFutureBlocks = true;
            chainParams.difficulty = chainParams.minimumDifficulty;
            chainParams.gasLimit = chainParams.maxGasLimit;
            chainParams.byzantiumForkBlock = 0;
            chainParams.constantinopleForkBlock = 0;
            chainParams.EIP158ForkBlock = 0;
            chainParams.externalGasDifficulty = 1;
            chainParams.sChain.contractStorageLimit = 128;
            // 615 + 1430 is experimentally-derived block size + average extras size
            chainParams.sChain.dbStorageLimit = 320.5*( 615 + 1430 );
            // add random extra data to randomize genesis hash and get random DB path,
            // so that tests can be run in parallel
            // TODO: better make it use ethemeral in-memory databases
            chainParams.extraData = h256::random().asBytes();
        }
        chainParams.sChain.multiTransactionMode = _mtmEnabled;

        //        web3.reset( new WebThreeDirect(
        //            "eth tests", tempDir.path(), "", chainParams, WithExisting::Kill, {"eth"},
        //            true ) );

        auto monitor = make_shared< InstanceMonitor >("test");

        setenv("DATA_DIR", tempDir.path().c_str(), 1);
        client.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
            shared_ptr< GasPricer >(), NULL, monitor, tempDir.path(), WithExisting::Kill ) );

        //        client.reset(
        //            new eth::Client( chainParams, ( int ) chainParams.networkID, shared_ptr<
        //            GasPricer >(),
        //                tempDir.path(), "", WithExisting::Kill, TransactionQueue::Limits{100000,
        //                1024} ) );

        client->setAuthor( coinbase.address() );

        // wait for 1st block - because it's always empty
        std::promise< void > block_promise;
        auto importHandler = client->setOnBlockImport(
            [&block_promise]( BlockHeader const& ) {
                    block_promise.set_value();
        } );

        client->injectSkaleHost();
        client->startWorking();

        block_promise.get_future().wait();

        if ( !_generation2 )
            client->setAuthor( coinbase.address() );
        else
            client->setAuthor( chainParams.sChain.blockAuthor );

        using FullServer = ModularServer< rpc::EthFace, /* rpc::NetFace,*/ rpc::Web3Face,
            rpc::AdminEthFace /*, rpc::AdminNetFace*/, rpc::DebugFace, rpc::TestFace >;

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( {coinbase, account2, account3} );

        sessionManager.reset( new rpc::SessionManager() );
        adminSession =
            sessionManager->newSession( rpc::SessionPermissions{{rpc::Privilege::Admin}} );

        auto ethFace = new rpc::Eth( std::string(""), *client, *accountHolder.get() );

        gasPricer = make_shared< eth::TrivialGasPricer >( 0, DefaultGasPrice );

        rpcServer.reset( new FullServer( ethFace /*, new rpc::Net(*web3)*/,
            new rpc::Web3( /*web3->clientVersion()*/ ),  // TODO Add real version?
            new rpc::AdminEth( *client, *gasPricer, keyManager, *sessionManager.get() ),
            /*new rpc::AdminNet(*web3, *sessionManager), */ new rpc::Debug( *client ),
            new rpc::Test( *client ) ) );

        //
        SkaleServerOverride::opts_t serverOpts;

        inject_rapidjson_handlers( serverOpts, ethFace );

        serverOpts.netOpts_.bindOptsStandard_.cntServers_ = 1;
        serverOpts.netOpts_.bindOptsStandard_.strAddrHTTP4_ = chainParams.nodeInfo.ip;
        // random port
        std::srand(std::time(nullptr));
        serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ = std::rand() % 64000 + 1025;
        std::cout << "PORT: " << serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ << std::endl;
        skale_server_connector = new SkaleServerOverride( chainParams, client.get(), serverOpts );
        rpcServer->addConnector( skale_server_connector );
        skale_server_connector->StartListening();

        sleep(1);

        auto client = new jsonrpc::HttpClient( "http://" + chainParams.nodeInfo.ip + ":" + std::to_string( serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ ) );
        client->SetTimeout(1000000000);

        rpcClient = unique_ptr< WebThreeStubClient >( new WebThreeStubClient( *client ) );
    }

    ~JsonRpcFixture() {
        if ( skale_server_connector )
            skale_server_connector->StopListening();
    }

    string sendingRawShouldFail( string const& _t ) {
        try {
            rpcClient->eth_sendRawTransaction( _t );
            BOOST_FAIL( "Exception expected." );
        } catch ( jsonrpc::JsonRpcException const& _e ) {
            return _e.GetMessage();
        }
        return string();
    }

    TransientDirectory tempDir; // ! should exist before client!
    unique_ptr< Client > client;

    dev::KeyPair coinbase{KeyPair::create()};
    dev::KeyPair account2{KeyPair::create()};
    dev::KeyPair account3{KeyPair::create()};
    unique_ptr< FixedAccountHolder > accountHolder;
    unique_ptr< rpc::SessionManager > sessionManager;
    std::shared_ptr< eth::TrivialGasPricer > gasPricer;
    KeyManager keyManager{KeyManager::defaultPath(), SecretStore::defaultPath()};
    unique_ptr< ModularServer<> > rpcServer;
    unique_ptr< WebThreeStubClient > rpcClient;
    std::string adminSession;
    SkaleServerOverride* skale_server_connector;
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
    const u256 blockReward = 2 * dev::eth::ether;
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
    const u256 blockReward = 2 * dev::eth::ether;
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
    const u256 blockReward = 2 * dev::eth::ether;
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
    const u256 blockReward = 2 * dev::eth::ether;
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

    BOOST_CHECK_EQUAL( jsToU256( fixture.rpcClient->eth_blockNumber() ), 1 );
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

// As block rotation is not exact now - let's use approximate comparisons
#define REQUIRE_APPROX_EQUAL(a, b) BOOST_REQUIRE(4*(a) > 3*(b) && 4*(a) < 5*(b))

BOOST_AUTO_TEST_CASE( logs_range ) {
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

    // -> blockNumber = 2 (1 for bootstrapAll, 1 for deploy)

    Json::Value deployReceipt = fixture.rpcClient->eth_getTransactionReceipt( deployHash );
    string contractAddress = deployReceipt["contractAddress"].asString();

    Json::Value filterObj;
    filterObj["address"] = contractAddress;
    filterObj["fromBlock"] = "0x1";
    string filterId = fixture.rpcClient->eth_newFilter( filterObj );

    Json::Value res = fixture.rpcClient->eth_getFilterLogs(filterId);
    BOOST_REQUIRE(res.isArray());
    BOOST_REQUIRE_EQUAL(res.size(), 0);
    res = fixture.rpcClient->eth_getFilterChanges(filterId);
    BOOST_REQUIRE(res.isArray());
    BOOST_REQUIRE_EQUAL(res.size(), 0);

    // need blockNumber==2+255 afterwards
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
    BOOST_REQUIRE_EQUAL(fixture.client->number(), 2 + 255);

    // ask for logs
    Json::Value t;
    t["fromBlock"] = 0;         // really 3
    t["toBlock"] = 251;
    t["address"] = contractAddress;
    Json::Value logs = fixture.rpcClient->eth_getLogs(t);
    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 249);

    // check logs
    for(size_t i=0; i<logs.size(); ++i){
        u256 block = dev::jsToU256( logs[(int)i]["topics"][0].asString() );
        BOOST_REQUIRE_EQUAL(block, i+3);
    }// for

    string nonexisting = "0x20"; string nonexisting_hash = logs[0x20-1]["blockHash"].asString();

    // add 255 more blocks
    string lastHash;
    for(int i=0; i<255; ++i){
        Json::Value t;
        t["from"] = toJS( fixture.coinbase.address() );
        t["value"] = jsToDecimal( "0" );
        t["to"] = contractAddress;
        t["gas"] = "99000";

        lastHash = fixture.rpcClient->eth_sendTransaction( t );
        BOOST_REQUIRE( !lastHash.empty() );

        dev::eth::mineTransaction( *( fixture.client ), 1 );
    }
    BOOST_REQUIRE_EQUAL(fixture.client->number(), 512);

    // ask for logs
    t["toBlock"] = 512;
    logs = fixture.rpcClient->eth_getLogs(t);
    BOOST_REQUIRE(logs.isArray());
    REQUIRE_APPROX_EQUAL(logs.size(), 256+64);

    // and filter
    res = fixture.rpcClient->eth_getFilterChanges(filterId);
    BOOST_REQUIRE_EQUAL(res.size(), 255+255);     // NB!! we had pending here, but then they disappeared!
    res = fixture.rpcClient->eth_getFilterLogs(filterId);
    REQUIRE_APPROX_EQUAL(res.size(), 256+64);

    ///////////////// OTHER CALLS //////////////////
    // HACK this may return DIFFERENT block! because of undeterministic block rotation!
    string existing = "0x1df"; string existing_hash = logs[256+64-1-1-32]["blockHash"].asString();
    cerr << logs << endl;

    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getBlockByNumber(existing, true));
    BOOST_REQUIRE_EQUAL(res["number"], existing);
    BOOST_REQUIRE(res["transactions"].isArray() && res["transactions"].size() == 1);
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getBlockByNumber(nonexisting, true), jsonrpc::JsonRpcException);

    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getBlockByHash(existing_hash, false));
    REQUIRE_APPROX_EQUAL(dev::eth::jsToBlockNumber(res["number"].asCString()), dev::eth::jsToBlockNumber(existing));
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getBlockByHash(nonexisting_hash, true), jsonrpc::JsonRpcException);

    //

    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getBlockTransactionCountByNumber(existing));
    BOOST_REQUIRE_EQUAL(res.asString(), "0x1");
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getBlockTransactionCountByNumber(nonexisting), jsonrpc::JsonRpcException);

    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getBlockTransactionCountByHash(existing_hash));
    BOOST_REQUIRE_EQUAL(res.asString(), "0x1");
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getBlockTransactionCountByHash(nonexisting_hash), jsonrpc::JsonRpcException);

    //

    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getUncleCountByBlockNumber(existing));
    BOOST_REQUIRE_EQUAL(res.asString(), "0x0");
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getUncleCountByBlockNumber(nonexisting), jsonrpc::JsonRpcException);

    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getUncleCountByBlockHash(existing_hash));
    BOOST_REQUIRE_EQUAL(res.asString(), "0x0");
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getUncleCountByBlockHash(nonexisting_hash), jsonrpc::JsonRpcException);

    //

    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex(existing, "0x0"));
    BOOST_REQUIRE_EQUAL(res["blockNumber"], existing);
    // HACK disabled for undeterminism BOOST_REQUIRE_EQUAL(res["blockHash"], existing_hash);
    BOOST_REQUIRE_EQUAL(res["to"], contractAddress);
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex(nonexisting, "0x0"), jsonrpc::JsonRpcException);

    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex(existing_hash, "0x0"));
    // HACK disabled for undeterminism BOOST_REQUIRE_EQUAL(res["blockNumber"], existing);
    BOOST_REQUIRE_EQUAL(res["blockHash"], existing_hash);
    BOOST_REQUIRE_EQUAL(res["to"], contractAddress);
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getTransactionByBlockHashAndIndex(nonexisting_hash, "0x0"), jsonrpc::JsonRpcException);

    //

    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getUncleByBlockNumberAndIndex(existing, "0x0"), jsonrpc::JsonRpcException);
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getUncleByBlockNumberAndIndex(nonexisting, "0x0"), jsonrpc::JsonRpcException);
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getUncleByBlockHashAndIndex(existing_hash, "0x0"), jsonrpc::JsonRpcException);
    BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getUncleByBlockHashAndIndex(nonexisting_hash, "0x0"), jsonrpc::JsonRpcException);

    //

    BOOST_REQUIRE_THROW(res = fixture.rpcClient->eth_getTransactionByHash(deployHash), jsonrpc::JsonRpcException);
    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getTransactionByHash(lastHash));
    BOOST_REQUIRE_EQUAL(res["blockNumber"], "0x200");

    BOOST_REQUIRE_THROW(res = fixture.rpcClient->eth_getTransactionReceipt(deployHash), jsonrpc::JsonRpcException);
    BOOST_REQUIRE_NO_THROW(res = fixture.rpcClient->eth_getTransactionReceipt(lastHash));
    BOOST_REQUIRE_EQUAL(res["transactionHash"], lastHash);
    BOOST_REQUIRE_EQUAL(res["blockNumber"], "0x200");
    BOOST_REQUIRE_EQUAL(res["to"], contractAddress);
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

    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );
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
    BOOST_CHECK_EQUAL( receipt["status"], string( "0x0" ) );
    Json::Value code =
        fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString() == "0x" );
}

BOOST_AUTO_TEST_CASE( deploy_contract_without_controller ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    ret["accounts"].removeMember("0xD2002000000000000000000000000000000000D2");
    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config, false, false );
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
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );
    BOOST_REQUIRE( !receipt["contractAddress"].isNull() );
    Json::Value code =
            fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString().substr( 2 ) == compiled.substr( 58 ) );
}

BOOST_AUTO_TEST_CASE( deploy_contract_with_controller ) {
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
    BOOST_CHECK_EQUAL( receipt["status"], string( "0x0" ) );
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
    string response1 = fixture.rpcClient->eth_call( checkAddress, "latest" );
    BOOST_CHECK( response1 != "0x0000000000000000000000000000000000000000000000000000000000000000" );

    fixture.client->setAuthor( senderAddress );
    transactionCallObject["from"] = toJS( senderAddress );
    fixture.rpcClient->eth_sendTransaction( transactionCallObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    string response2 = fixture.rpcClient->eth_call( checkAddress, "latest" );
    BOOST_CHECK( response2 != "0x0000000000000000000000000000000000000000000000000000000000000000" );
    BOOST_CHECK( response2 != response1 );
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

    BOOST_CHECK_EQUAL( jsToU256( fixture.rpcClient->eth_blockNumber() ), 1 );
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

    cerr << receipt << endl;

    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x0" ) );
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

    std::cout << cc::now2string() << " eth_getTransactionReceipt" << std::endl;
    Json::Value receipt;
    try{
        receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    }catch(...){
        std::cout << cc::now2string() << " /eth_getTransactionReceipt" << std::endl;
        throw;
    }
    std::cout << cc::now2string() << " /eth_getTransactionReceipt" << std::endl;
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
    const u256 blockReward = 2 * dev::eth::ether;
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
    BOOST_CHECK_EQUAL( fixture.sendingRawShouldFail( signedTx2["raw"].asString() ), "Transaction gas price lower than current eth_gasPrice." );
}

// different ways to ask for topic(s)
BOOST_AUTO_TEST_CASE( logs ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );

    // will generate topics [0xxxx, 0, 0], [0xxx, 0, 1] etc
/*
pragma solidity >=0.4.10 <0.7.0;

contract Logger{

    uint256 i;
    uint256 j;

    fallback() external payable {
        log3(bytes32(block.number), bytes32(block.number), bytes32(i), bytes32(j));
        j++;
        if(j==10){
            j = 0;
            i++;
        }// j overflow
    }
}
*/    

    string bytecode = "6080604052348015600f57600080fd5b50609b8061001e6000396000f3fe608060405260015460001b60005460001b4360001b4360001b6040518082815260200191505060405180910390a3600160008154809291906001019190505550600a6001541415606357600060018190555060008081548092919060010191905055505b00fea2646970667358221220fdf2f98961b803b6b32dfc9be766990cbdb17559d9a03724d12fc672e33804b164736f6c634300060c0033";

    Json::Value create;
    create["code"] = bytecode;
    create["gas"] = "180000";  // TODO or change global default of 90000?

    string deployHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value deployReceipt = fixture.rpcClient->eth_getTransactionReceipt( deployHash );
    string contractAddress = deployReceipt["contractAddress"].asString();

    for(int i=0; i<=23; ++i){
        Json::Value t;
        t["from"] = toJS( fixture.coinbase.address() );
        t["value"] = jsToDecimal( "0" );
        t["to"] = contractAddress;
        t["gas"] = "99000";

        std::string txHash = fixture.rpcClient->eth_sendTransaction( t );
        BOOST_REQUIRE( !txHash.empty() );

        dev::eth::mineTransaction( *( fixture.client ), 1 );
        Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    }
    BOOST_REQUIRE_EQUAL(fixture.client->number(), 26);     // block 1 - bootstrapAll, block 2 - deploy

    // ask for logs
    Json::Value t;
    t["fromBlock"] = 3;
    t["toBlock"] = 26;
    t["address"] = contractAddress;
    t["topics"] = Json::Value(Json::arrayValue);

    // 1 topics = []
    Json::Value logs = fixture.rpcClient->eth_getLogs(t);

    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 24);
    u256 t1 = dev::jsToU256( logs[12]["topics"][1].asString() );
    BOOST_REQUIRE_EQUAL(t1, 1);
    u256 t2 = dev::jsToU256( logs[12]["topics"][2].asString() );
    BOOST_REQUIRE_EQUAL(t2, 2);

    // 2 topics = [a]
    t["topics"] = Json::Value(Json::arrayValue);
    t["topics"][1] = u256_to_js(dev::u256(2));

    logs = fixture.rpcClient->eth_getLogs(t);

    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 4);
    t1 = dev::jsToU256( logs[0]["topics"][1].asString() );
    BOOST_REQUIRE_EQUAL(t1, 2);
    t2 = dev::jsToU256( logs[0]["topics"][2].asString() );
    BOOST_REQUIRE_EQUAL(t2, 0);

    // 3 topics = [null, a]
    t["topics"] = Json::Value(Json::arrayValue);
    t["topics"][2] = u256_to_js(dev::u256(1));      // 01,11,21 but not 1x

    logs = fixture.rpcClient->eth_getLogs(t);

    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 3);

    // 4 topics = [a,b]
    t["topics"] = Json::Value(Json::arrayValue);
    t["topics"][1] = u256_to_js(dev::u256(1));
    t["topics"][2] = u256_to_js(dev::u256(2));

    logs = fixture.rpcClient->eth_getLogs(t);

    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 1);

    // 5 topics = [[a,b]]
    t["topics"] = Json::Value(Json::arrayValue);
    t["topics"][1] = Json::Value(Json::arrayValue);
    t["topics"][1][0] = u256_to_js(dev::u256(1));
    t["topics"][1][1] = u256_to_js(dev::u256(2));

    logs = fixture.rpcClient->eth_getLogs(t);

    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 10+4);

    // 6 topics = [a,a]
    t["topics"] = Json::Value(Json::arrayValue);
    t["topics"][1] = u256_to_js(dev::u256(1));
    t["topics"][2] = u256_to_js(dev::u256(1));

    logs = fixture.rpcClient->eth_getLogs(t);

    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 1);

    // 7 topics = [[a,b], c]
    t["topics"] = Json::Value(Json::arrayValue);
    t["topics"][1] = Json::Value(Json::arrayValue);
    t["topics"][1][0] = u256_to_js(dev::u256(1));
    t["topics"][1][1] = u256_to_js(dev::u256(2));
    t["topics"][2] = u256_to_js(dev::u256(1));           // 11, 21

    logs = fixture.rpcClient->eth_getLogs(t);

    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 2);
    t1 = dev::jsToU256( logs[0]["topics"][1].asString() );
    BOOST_REQUIRE_EQUAL(t1, 1);
    t2 = dev::jsToU256( logs[0]["topics"][2].asString() );
    BOOST_REQUIRE_EQUAL(t2, 1);
    t1 = dev::jsToU256( logs[1]["topics"][1].asString() );
    BOOST_REQUIRE_EQUAL(t1, 2);
    t2 = dev::jsToU256( logs[1]["topics"][2].asString() );
    BOOST_REQUIRE_EQUAL(t2, 1);

    // 8 repeat #7 without address
    auto logs7 = logs;
    t["address"] = Json::Value(Json::arrayValue);
    logs = fixture.rpcClient->eth_getLogs(t);
    BOOST_REQUIRE_EQUAL(logs7, logs);

    // 9 repeat #7 with 2 addresses
    t["address"] = Json::Value(Json::arrayValue);
    t["address"][0] = contractAddress;
    t["address"][1] = "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba";         // dummy
    logs = fixture.rpcClient->eth_getLogs(t);
    BOOST_REQUIRE_EQUAL(logs7, logs);

    // 10 request address only
    t["topics"] = Json::Value(Json::arrayValue);
    logs = fixture.rpcClient->eth_getLogs(t);
    BOOST_REQUIRE(logs.isArray());
    BOOST_REQUIRE_EQUAL(logs.size(), 24);
}

BOOST_AUTO_TEST_CASE( storage_limit_contract ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 10 );
    
// pragma solidity 0.4.25;

// contract TestStorageLimit {

//     uint[] public storageArray;

//     function store(uint256 num) public {
//         storageArray.push( num );
//     }

//     function erase(uint256 index) public {
//         delete storageArray[index];
//     }

//     function foo() public view {
//         uint len = storageArray.length;
//         storageArray.push(1);
//     }

//     function storeAndCall(uint256 num) public {
//         storageArray.push( num );
//         foo();
//     }

//     function zero(uint256 index) public {
//         storageArray[index] = 0;
//     }
    
//     function strangeFunction(uint256 index) public {
//         storageArray[index] = 1;
//         storageArray[index] = 0;
//         storageArray[index] = 2;
//     }
// }
    
    std::string bytecode = "0x608060405234801561001057600080fd5b5061034f806100206000396000f300608060405260043610610083576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff1680630e031ab1146100885780631007f753146100c95780636057361d146100f6578063c298557814610123578063c67cd8841461013a578063d269ad4e14610167578063e0353e5914610194575b600080fd5b34801561009457600080fd5b506100b3600480360381019080803590602001909291905050506101c1565b6040518082815260200191505060405180910390f35b3480156100d557600080fd5b506100f4600480360381019080803590602001909291905050506101e4565b005b34801561010257600080fd5b5061012160048036038101908080359060200190929190505050610204565b005b34801561012f57600080fd5b50610138610233565b005b34801561014657600080fd5b506101656004803603810190808035906020019092919050505061026c565b005b34801561017357600080fd5b50610192600480360381019080803590602001909291905050506102a3565b005b3480156101a057600080fd5b506101bf60048036038101908080359060200190929190505050610302565b005b6000818154811015156101d057fe5b906000526020600020016000915090505481565b6000818154811015156101f357fe5b906000526020600020016000905550565b600081908060018154018082558091505090600182039060005260206000200160009091929091909150555050565b60008080549050905060006001908060018154018082558091505090600182039060005260206000200160009091929091909150555050565b60008190806001815401808255809150509060018203906000526020600020016000909192909190915055506102a0610233565b50565b60016000828154811015156102b457fe5b9060005260206000200181905550600080828154811015156102d257fe5b906000526020600020018190555060026000828154811015156102f157fe5b906000526020600020018190555050565b6000808281548110151561031257fe5b9060005260206000200181905550505600a165627a7a723058201ed095336772c55688864a6b45ca6ab89311c5533f8d38cdf931f1ce38be78080029";
    
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

    Json::Value txZeroValue;  // call zero(1)
    txZeroValue["to"] = contractAddress;
    txZeroValue["data"] = "0xe0353e590000000000000000000000000000000000000000000000000000000000000001";
    txZeroValue["from"] = toJS( senderAddress );
    txZeroValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txZeroValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 64 );

    Json::Value txZeroValue1;  // call zero(1)
    txZeroValue1["to"] = contractAddress;
    txZeroValue1["data"] = "0xe0353e590000000000000000000000000000000000000000000000000000000000000001";
    txZeroValue1["from"] = toJS( senderAddress );
    txZeroValue1["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txZeroValue1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 64 );

    Json::Value txValueChanged;  // call strangeFunction(1)
    txValueChanged["to"] = contractAddress;
    txValueChanged["data"] = "0xd269ad4e0000000000000000000000000000000000000000000000000000000000000001";
    txValueChanged["from"] = toJS( senderAddress );
    txValueChanged["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txValueChanged );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 96 );

    Json::Value txValueChanged1;  // call strangeFunction(0)
    txValueChanged1["to"] = contractAddress;
    txValueChanged1["data"] = "0xd269ad4e0000000000000000000000000000000000000000000000000000000000000000";
    txValueChanged1["from"] = toJS( senderAddress );
    txValueChanged1["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txValueChanged1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 96 );

    Json::Value txValueChanged2;  // call strangeFunction(2)
    txValueChanged2["to"] = contractAddress;
    txValueChanged2["data"] = "0xd269ad4e0000000000000000000000000000000000000000000000000000000000000002";
    txValueChanged2["from"] = toJS( senderAddress );
    txValueChanged2["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txValueChanged2 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 128 );

    Json::Value txValueChanged3;  // try call strangeFunction(3)
    txValueChanged3["to"] = contractAddress;
    txValueChanged3["data"] = "0xd269ad4e0000000000000000000000000000000000000000000000000000000000000003";
    txValueChanged3["from"] = toJS( senderAddress );
    txValueChanged3["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txValueChanged3 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsed( contract ) == 128 );
}

BOOST_AUTO_TEST_CASE( storage_limit_chain ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 20 );
//    pragma solidity >=0.4.22 <0.7.0;

//    contract TestStorage1 {

//        uint[] array;

//        function store(uint256 num) public {
//            array.push( num + 2 );
//        }

//        function erase(uint idx) public {
//            delete array[idx];
//        }
//    }
    std::string bytecode1 = "608060405234801561001057600080fd5b5061012c806100206000396000f3fe6080604052348015600f57600080fd5b5060043610604f576000357c0100000000000000000000000000000000000000000000000000000000900480631007f7531460545780636057361d14607f575b600080fd5b607d60048036036020811015606857600080fd5b810190808035906020019092919050505060aa565b005b60a860048036036020811015609357600080fd5b810190808035906020019092919050505060c7565b005b6000818154811060b657fe5b906000526020600020016000905550565b60006002820190806001815401808255809150506001900390600052602060002001600090919091909150555056fea264697066735822122055c65b9e093cdb44864dac3fb79ec15a542db86c2f897b938043d8e15468ca4464736f6c63430006060033";

    auto senderAddress = fixture.coinbase.address();

    Json::Value create1;
    create1["from"] = toJS( senderAddress );
    create1["data"] = bytecode1;
    create1["gas"] = "1800000";
    string txHash = fixture.rpcClient->eth_sendTransaction( create1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt1 = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress1 = receipt1["contractAddress"].asString();

//    pragma solidity >=0.4.22 <0.7.0;

//    contract TestStorage2 {

//        uint[] array;

//        function store(uint256 num) public {
//            array.push( num );
//        }

//        function erase(uint idx) public {
//            delete array[idx];
//        }
//    }
    Json::Value txStore;  // call store(1)
    txStore["to"] = contractAddress1;
    txStore["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000001";
    txStore["from"] = toJS( senderAddress );
    txStore["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStore );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64);

    // call store(2)
    txStore["to"] = contractAddress1;
    txStore["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000002";
    txStore["from"] = toJS( senderAddress );
    txStore["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStore );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 96 );

    Json::Value txErase;  // call erase(1)
    txErase["to"] = contractAddress1;
    txErase["data"] = "0x1007f7530000000000000000000000000000000000000000000000000000000000000001";
    txErase["from"] = toJS( senderAddress );
    txErase["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txErase );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64);

//    pragma solidity >=0.4.22 <0.7.0;

//    contract TestStorage2 {

//        uint[] array;

//        function store(uint256 num) public {
//            array.push( num );
//        }

//        function erase(uint idx) public {
//            delete array[idx];
//        }
//    }
    std::string bytecode2 = "608060405234801561001057600080fd5b50610129806100206000396000f3fe6080604052348015600f57600080fd5b5060043610604f576000357c0100000000000000000000000000000000000000000000000000000000900480631007f7531460545780636057361d14607f575b600080fd5b607d60048036036020811015606857600080fd5b810190808035906020019092919050505060aa565b005b60a860048036036020811015609357600080fd5b810190808035906020019092919050505060c7565b005b6000818154811060b657fe5b906000526020600020016000905550565b60008190806001815401808255809150506001900390600052602060002001600090919091909150555056fea26469706673582212202bfb5f6fb63ae4f1c9a362ed3f7de7aa5514029db925efa368e711e35d9ebc0a64736f6c63430006060033";

    Json::Value create2;
    create2["from"] = toJS( senderAddress );
    create2["data"] = bytecode2;
    create2["gas"] = "1800000";
    txHash = fixture.rpcClient->eth_sendTransaction( create2 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt2 = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress2 = receipt2["contractAddress"].asString();

    Json::Value txStoreSecondContract; // call store(1)
    txStoreSecondContract["to"] = contractAddress2;
    txStoreSecondContract["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000001";
    txStoreSecondContract["from"] = toJS( senderAddress );
    txStoreSecondContract["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStoreSecondContract );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 128 );

    // try call store(2) to second contract
    txStoreSecondContract["to"] = contractAddress2;
    txStoreSecondContract["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000002";
    txStoreSecondContract["from"] = toJS( senderAddress );
    txStoreSecondContract["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStoreSecondContract );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 128 );

    // try call store(3) to first contract
    txStore["to"] = contractAddress1;
    txStore["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000001";
    txStore["from"] = toJS( senderAddress );
    txStore["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStore );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 128 );

    Json::Value txZeroValue;  // call zero(1)
    txZeroValue["to"] = contractAddress1;
    txZeroValue["data"] = "0x1007f7530000000000000000000000000000000000000000000000000000000000000000";
    txZeroValue["from"] = toJS( senderAddress );
    txZeroValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txZeroValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 96 );
}

BOOST_AUTO_TEST_CASE( storage_limit_predeployed ) {
    JsonRpcFixture fixture( c_genesisConfigString );
    dev::eth::simulateMining( *( fixture.client ), 20 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64 );
    
    string contractAddress = "0xC2002000000000000000000000000000000000C2";
    string senderAddress = toJS(fixture.coinbase.address());

    Json::Value txChangeInt;
    txChangeInt["to"] = contractAddress;
    txChangeInt["data"] = "0xcd16ecbf0000000000000000000000000000000000000000000000000000000000000002";
    txChangeInt["from"] = senderAddress;
    txChangeInt["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    string txHash = fixture.rpcClient->eth_sendTransaction( txChangeInt );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64 );

    Json::Value txZeroValue;
    txZeroValue["to"] = contractAddress;
    txZeroValue["data"] = "0xcd16ecbf0000000000000000000000000000000000000000000000000000000000000000";
    txZeroValue["from"] = senderAddress;
    txZeroValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txZeroValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    std::cout << fixture.client->state().storageUsedTotal() << std::endl;
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 32 );

    Json::Value txChangeInt1;
    txChangeInt["to"] = contractAddress;
    txChangeInt["data"] = "0x9b0631040000000000000000000000000000000000000000000000000000000000000001";
    txChangeInt["from"] = senderAddress;
    txChangeInt["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txChangeInt );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    std::cout << fixture.client->state().storageUsedTotal() << std::endl;
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64 );
}

BOOST_AUTO_TEST_CASE( setSchainExitTime ) {
    JsonRpcFixture fixture;
    Json::Value requestJson;
    requestJson["finishTime"] = 100;
    BOOST_REQUIRE_THROW(fixture.rpcClient->setSchainExitTime(requestJson), jsonrpc::JsonRpcException);
}

BOOST_AUTO_TEST_CASE( oracle ) {
    JsonRpcFixture fixture;
    std::string receipt;
    std::string result;
    std::time_t current = std::time(nullptr);
    std::string request;
    for (int i = 0; i < 1000000; ++i) {
        request = skutils::tools::format("{\"cid\":1,\"uri\":\"http://worldtimeapi.org/api/timezone/Europe/Kiev\",\"jsps\":[\"/unixtime\",\"/day_of_year\",\"/xxx\"],\"trims\":[1,1,1],\"time\":%zu000,\"pow\":%zu}", current, i);
        u256 h = sha3( request );
        if( ~u256(0) / h > 10000)
            break;
    }

    uint64_t status = fixture.client->submitOracleRequest(request, receipt);

    BOOST_REQUIRE_EQUAL(status, 0);
    BOOST_CHECK(receipt != "");

    sleep(5);

    uint64_t resultStatus = fixture.client->checkOracleResult(receipt, result);
    BOOST_REQUIRE_EQUAL(resultStatus, 0);
    BOOST_CHECK(result != "");
}

BOOST_AUTO_TEST_CASE( EIP1898Calls ) {
    JsonRpcFixture fixture;

    Json::Value eip1898WellFormed;
    eip1898WellFormed["blockHash"] = dev::h256::random().hex();
    eip1898WellFormed["requireCanonical"] = true;

    Json::Value eip1898WellFormed1;
    eip1898WellFormed1["blockHash"] = dev::h256::random().hex();

    Json::Value eip1898WellFormed2;
    eip1898WellFormed2["blockHash"] = dev::h256::random().hex();
    eip1898WellFormed2["requireCanonical"] = false;

    Json::Value eip1898WellFormed3;
    eip1898WellFormed3["blockNumber"] = dev::h256::random().hex();

    Json::Value eip1898BadFormed;
    eip1898BadFormed["blockHashxxx"] = dev::h256::random().hex();
    eip1898BadFormed["requireCanonical"] = false;

    Json::Value eip1898BadFormed1;
    eip1898BadFormed1["blockHash"] = dev::h256::random().hex();
    eip1898BadFormed1["requireCanonical"] = false;
    eip1898BadFormed1["smth"] = 1;

    Json::Value eip1898BadFormed2;
    eip1898BadFormed2["blockHash"] = 228;

    Json::Value eip1898BadFormed3;
    eip1898BadFormed3["blockHash"] = dev::h256::random().hex();
    eip1898BadFormed3["requireCanonical"] = 228;
    
    Json::Value eip1898BadFormed4;
    eip1898BadFormed4["blockNumber"] = dev::h256::random().hex();
    eip1898BadFormed4["requireCanonical"] = true;

    Json::Value eip1898BadFormed5;
    eip1898BadFormed5["blockNumber"] = dev::h256::random().hex();
    eip1898BadFormed5["requireCanonical"] = 228;

    std::array<Json::Value, 4> wellFormedCalls = { eip1898WellFormed, eip1898WellFormed1, eip1898WellFormed2, eip1898WellFormed3 };
    std::array<Json::Value, 6> badFormedCalls = { eip1898BadFormed, eip1898BadFormed1, eip1898BadFormed2, eip1898BadFormed3, eip1898BadFormed4, eip1898BadFormed5 };
    
    auto address = fixture.coinbase.address();

    std::string response;
    for (const auto& call: wellFormedCalls) {
        BOOST_REQUIRE_NO_THROW(fixture.rpcClient->eth_getBalanceEIP1898( toJS( address ), call ));
    }

    for (const auto& call: badFormedCalls) {
        BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getBalanceEIP1898( toJS( address ), call ), jsonrpc::JsonRpcException);
    }
    
    for (const auto& call: wellFormedCalls) {
        Json::Value transactionCallObject;
        transactionCallObject["to"] = "0x0000000000000000000000000000000000000005";
        transactionCallObject["data"] = "0x0000000000000000000000000000000000000005";
        BOOST_REQUIRE_NO_THROW(fixture.rpcClient->eth_callEIP1898( transactionCallObject, call ));
    }

    for (const auto& call: badFormedCalls) {
        Json::Value transactionCallObject;
        transactionCallObject["to"] = "0x0000000000000000000000000000000000000005";
        transactionCallObject["data"] = "0x0000000000000000000000000000000000000005";
        BOOST_REQUIRE_THROW(fixture.rpcClient->eth_callEIP1898( transactionCallObject, call ), jsonrpc::JsonRpcException);
    }

    for (const auto& call: wellFormedCalls) {
        BOOST_REQUIRE_NO_THROW(fixture.rpcClient->eth_getCodeEIP1898( toJS( address ), call ));
    }

    for (const auto& call: badFormedCalls) {
        BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getCodeEIP1898( toJS( address ), call ), jsonrpc::JsonRpcException);
    }

    for (const auto& call: wellFormedCalls) {
        BOOST_REQUIRE_NO_THROW(fixture.rpcClient->eth_getStorageAtEIP1898( toJS( address ), toJS( address ), call ));
    }

    for (const auto& call: badFormedCalls) {
        BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getStorageAtEIP1898( toJS( address ), toJS( address ), call ), jsonrpc::JsonRpcException);
    }

    for (const auto& call: wellFormedCalls) {
        BOOST_REQUIRE_NO_THROW(fixture.rpcClient->eth_getTransactionCountEIP1898( toJS( address ), call ));
    }

    for (const auto& call: badFormedCalls) {
        BOOST_REQUIRE_THROW(fixture.rpcClient->eth_getTransactionCountEIP1898( toJS( address ), call ), jsonrpc::JsonRpcException);
    }
}

BOOST_AUTO_TEST_CASE( etherbase_generation2 ) {
    JsonRpcFixture fixture(c_genesisGeneration2ConfigString, false, false, true);
    string etherbase = fixture.rpcClient->eth_coinbase();

    // before mining
    u256 etherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );
    BOOST_REQUIRE_EQUAL( etherbaseBalance, 0 );

    // mine block without transactions
    dev::eth::simulateMining( *( fixture.client ), 1 );
    etherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );
    BOOST_REQUIRE_GT( etherbaseBalance, 0 );

    // mine transaction
    Json::Value sampleTx;
    sampleTx["value"] = 1000000;
    sampleTx["data"] = toJS( bytes() );
    sampleTx["from"] = fixture.coinbase.address().hex();
    sampleTx["to"] = fixture.account2.address().hex();
    sampleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    std::string txHash = fixture.rpcClient->eth_sendTransaction( sampleTx );
    BOOST_REQUIRE( !txHash.empty() );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( fixture.account2.address() ), u256( 1000000 ) );

    // partially retrieve 1000000
    etherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );
    u256 balance = fixture.client->balanceAt( jsToAddress( "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F" ) );

    Json::Value partiallyRetrieveTx;
    partiallyRetrieveTx["data"] = "0xc6427474000000000000000000000000d2c0deface0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006000000000000000000000000000000000000000000000000000000000000000e4b61d27f6000000000000000000000000d2ba3e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000600000000000000000000000000000000000000000000000000000000000000044204a3e930000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f00000000000000000000000000000000000000000000000000000000000f42400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
    partiallyRetrieveTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    partiallyRetrieveTx["to"] = "0xD244519000000000000000000000000000000000";
    partiallyRetrieveTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    partiallyRetrieveTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( partiallyRetrieveTx );
    BOOST_REQUIRE( !txHash.empty() );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    auto t = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( jsToAddress( etherbase ) ), etherbaseBalance + jsToU256( t["gasUsed"].asString() ) * jsToU256( partiallyRetrieveTx["gasPrice"].asString() )- u256( 1000000 ) );
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( jsToAddress( "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F" ) ), balance + u256( 1000000 ) );

    // retrieve all
    u256 oldEtherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );
    balance = fixture.client->balanceAt( jsToAddress( "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F" ) );

    Json::Value retrieveTx;
    retrieveTx["data"] = "0xc6427474000000000000000000000000d2c0deface0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006000000000000000000000000000000000000000000000000000000000000000c4b61d27f6000000000000000000000000d2ba3e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006000000000000000000000000000000000000000000000000000000000000000240a79309b0000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
    retrieveTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    retrieveTx["to"] = "0xD244519000000000000000000000000000000000";
    retrieveTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    retrieveTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( retrieveTx );
    BOOST_REQUIRE( !txHash.empty() );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    t = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    etherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );
    BOOST_REQUIRE_EQUAL( jsToU256( t["gasUsed"].asString() ) * jsToU256( partiallyRetrieveTx["gasPrice"].asString() ), etherbaseBalance );
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( jsToAddress( "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F" ) ), balance + oldEtherbaseBalance );
}

BOOST_AUTO_TEST_CASE( deploy_controller_generation2 ) {
    JsonRpcFixture fixture(c_genesisGeneration2ConfigString, false, false, true);

    Json::Value hasRoleBeforeGrantingCall;
    hasRoleBeforeGrantingCall["data"] = "0x91d14854fc425f2263d0df187444b70e47283d622c70181c5baebb1306a01edba1ce184c0000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    hasRoleBeforeGrantingCall["to"] = "0xD2002000000000000000000000000000000000d2";
    BOOST_REQUIRE( jsToInt( fixture.rpcClient->eth_call( hasRoleBeforeGrantingCall, "latest" ) ) == 0 );

    string compiled =
            "6080604052341561000f57600080fd5b60b98061001d6000396000f300"
            "608060405260043610603f576000357c01000000000000000000000000"
            "00000000000000000000000000000000900463ffffffff168063b3de64"
            "8b146044575b600080fd5b3415604e57600080fd5b606a600480360381"
            "019080803590602001909291905050506080565b604051808281526020"
            "0191505060405180910390f35b60006007820290509190505600a16562"
            "7a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb9"
            "8fb480406fc2728a960029";

    Json::Value deployContractWithoutRoleTx;
    deployContractWithoutRoleTx["from"] = "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    deployContractWithoutRoleTx["code"] = compiled;
    deployContractWithoutRoleTx["gas"] = "1000000";
    deployContractWithoutRoleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();

    string txHash = fixture.rpcClient->eth_sendTransaction( deployContractWithoutRoleTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x0" ) );
    Json::Value code =
            fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString() == "0x" );

    // grant deployer role to 0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f
    Json::Value grantDeployerRoleTx;
    grantDeployerRoleTx["data"] = "0xc6427474000000000000000000000000d2c0deface0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006000000000000000000000000000000000000000000000000000000000000000c4b61d27f6000000000000000000000000d2002000000000000000000000000000000000d2000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000600000000000000000000000000000000000000000000000000000000000000024e43252d70000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
    grantDeployerRoleTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    grantDeployerRoleTx["to"] = "0xD244519000000000000000000000000000000000";
    grantDeployerRoleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    grantDeployerRoleTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( grantDeployerRoleTx );
    BOOST_REQUIRE( !txHash.empty() );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value hasRoleCall;
    hasRoleCall["data"] = "0x91d14854fc425f2263d0df187444b70e47283d622c70181c5baebb1306a01edba1ce184c0000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    hasRoleCall["to"] = "0xD2002000000000000000000000000000000000d2";
    BOOST_REQUIRE( jsToInt( fixture.rpcClient->eth_call( hasRoleCall, "latest" ) ) == 1 );

    // contract test {
    //  function f(uint a) returns(uint d) { return a * 7; }
    // }

    Json::Value deployContractTx;
    deployContractTx["from"] = "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    deployContractTx["code"] = compiled;
    deployContractTx["gas"] = "1000000";
    deployContractTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();

    txHash = fixture.rpcClient->eth_sendTransaction( deployContractTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );
    BOOST_REQUIRE( !receipt["contractAddress"].isNull() );
    code = fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString().substr( 2 ) == compiled.substr( 58 ) );
}

BOOST_AUTO_TEST_CASE( filestorage_generation2 ) {
    JsonRpcFixture fixture(c_genesisGeneration2ConfigString, false, false, true);

    Json::Value hasRoleBeforeGrantingCall;
    hasRoleBeforeGrantingCall["data"] = "0x91d1485468bf109b95a5c15fb2bb99041323c27d15f8675e11bf7420a1cd6ad64c394f460000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    hasRoleBeforeGrantingCall["to"] = "0xD3002000000000000000000000000000000000d3";
    BOOST_REQUIRE( jsToInt( fixture.rpcClient->eth_call( hasRoleBeforeGrantingCall, "latest" ) ) == 0 );

    Json::Value reserveSpaceBeforeGrantRoleTx;
    reserveSpaceBeforeGrantRoleTx["data"] = "0x1cfe4e3b0000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000000000000000000000000000000000000064";
    reserveSpaceBeforeGrantRoleTx["from"] = "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    reserveSpaceBeforeGrantRoleTx["to"] = "0xD3002000000000000000000000000000000000d3";
    reserveSpaceBeforeGrantRoleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    reserveSpaceBeforeGrantRoleTx["gas"] = toJS( "1000000" );
    std::string txHash = fixture.rpcClient->eth_sendTransaction( reserveSpaceBeforeGrantRoleTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x0" ) );

    Json::Value grantRoleTx;
    grantRoleTx["data"] = "0xc6427474000000000000000000000000d2c0deface0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006000000000000000000000000000000000000000000000000000000000000000e4b61d27f6000000000000000000000000d3002000000000000000000000000000000000d30000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006000000000000000000000000000000000000000000000000000000000000000442f2ff15d68bf109b95a5c15fb2bb99041323c27d15f8675e11bf7420a1cd6ad64c394f460000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
    grantRoleTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    grantRoleTx["to"] = "0xD244519000000000000000000000000000000000";
    grantRoleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    grantRoleTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( grantRoleTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );

    Json::Value hasRoleCall;
    hasRoleCall["data"] = "0x91d1485468bf109b95a5c15fb2bb99041323c27d15f8675e11bf7420a1cd6ad64c394f460000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    hasRoleCall["to"] = "0xD3002000000000000000000000000000000000d3";
    BOOST_REQUIRE( jsToInt( fixture.rpcClient->eth_call( hasRoleCall, "latest" ) ) == 1 );

    Json::Value reserveSpaceTx;
    reserveSpaceTx["data"] = "0x1cfe4e3b0000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000000000000000000000000000000000000064";
    reserveSpaceTx["from"] = "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    reserveSpaceTx["to"] = "0xD3002000000000000000000000000000000000d3";
    reserveSpaceTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    reserveSpaceTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( reserveSpaceTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );

    Json::Value getReservedSpaceCall;
    hasRoleCall["data"] = "0xbb559d160000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    hasRoleCall["to"] = "0xD3002000000000000000000000000000000000d3";
    BOOST_REQUIRE( jsToInt( fixture.rpcClient->eth_call( hasRoleCall, "latest" ) ) == 100 );
}

BOOST_AUTO_TEST_CASE( PrecompiledPrintFakeEth, *boost::unit_test::precondition( []( unsigned long ) -> bool { return false; } ) ) {
    JsonRpcFixture fixture(c_genesisConfigString, false, false);
    dev::eth::simulateMining( *( fixture.client ), 20 );

    fixture.accountHolder->setAccounts( {fixture.coinbase, fixture.account2, dev::KeyPair(dev::Secret("0x1c2cd4b70c2b8c6cd7144bbbfbd1e5c6eacb4a5efd9c86d0e29cbbec4e8483b9"))} );

    u256 balance = fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
    BOOST_REQUIRE_EQUAL( balance, 0 );

    Json::Value printFakeEthFromDisallowedAddressTx;
    printFakeEthFromDisallowedAddressTx["data"] = "0x5C4e11842E8Be09264DC1976943571D7AF6d00f80000000000000000000000000000000000000000000000000000000000000010";
    printFakeEthFromDisallowedAddressTx["from"] = fixture.coinbase.address().hex();
    printFakeEthFromDisallowedAddressTx["to"] = "0000000000000000000000000000000000000006";
    printFakeEthFromDisallowedAddressTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    fixture.rpcClient->eth_sendTransaction( printFakeEthFromDisallowedAddressTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    balance = fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
    BOOST_REQUIRE_EQUAL( balance, 0 );

    Json::Value printFakeEthTx;
    printFakeEthTx["data"] = "0x5C4e11842E8Be09264DC1976943571D7AF6d00f80000000000000000000000000000000000000000000000000000000000000010";
    printFakeEthTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    printFakeEthTx["to"] = "0000000000000000000000000000000000000006";
    printFakeEthTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    fixture.rpcClient->eth_sendTransaction( printFakeEthTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    balance = fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
    BOOST_REQUIRE_EQUAL( balance, 16 );

    Json::Value printFakeEthCall;    
    printFakeEthCall["data"] = "0x5C4e11842E8Be09264DC1976943571D7AF6d00f80000000000000000000000000000000000000000000000000000000000000010";
    printFakeEthCall["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    printFakeEthCall["to"] = "0000000000000000000000000000000000000006";
    printFakeEthCall["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    fixture.rpcClient->eth_call( printFakeEthCall, "latest" );

    balance = fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
    BOOST_REQUIRE_EQUAL( balance, 16 );

    // pragma solidity ^0.4.25;
    
    // contract Caller {
    //     function call() public view {
    //         bool status;
    //         uint amount = 16;
    //         address to = 0x5C4e11842E8Be09264DC1976943571D7AF6d00f8;
    //         assembly{
    //                 let ptr := mload(0x40)
    //                 mstore(ptr, to)
    //                 mstore(add(ptr, 0x20), amount)
    //                 status := delegatecall(not(0), 0x06, ptr, 0x40, ptr, 32)
    //         }
    //     }
    // }

    string compiled = "0x6080604052348015600f57600080fd5b5060a78061001e6000396000f30060806040526004361060225760003560e01c63ffffffff16806328b5e32b146027575b600080fd5b348015603257600080fd5b506039603b565b005b600080600060109150735c4e11842e8be09264dc1976943571d7af6d00f890506040518181528260208201526020816040836006600019f49350505050505600a165627a7a72305820c99b5f7e9e41fb0fee1724d382ca0f2c003087f66b3b46037ca6c7d452b076f20029";

    Json::Value create;
    create["from"] = fixture.coinbase.address().hex();
    create["code"] = compiled;
    create["gas"] = "1000000";

    TransactionSkeleton ts = toTransactionSkeleton( create );
    ts = fixture.client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = fixture.accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    RLPStream stream;
    tx.streamRLP( stream );
    auto txHash = fixture.rpcClient->eth_sendRawTransaction( toJS( stream.out() ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress = receipt["contractAddress"].asString();

    Json::Value transactionCallObject;
    transactionCallObject["to"] = contractAddress;
    transactionCallObject["data"] = "0x28b5e32b";

    fixture.rpcClient->eth_call( transactionCallObject, "latest" );
    balance = fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
    BOOST_REQUIRE_EQUAL( balance, 16 );
}

BOOST_AUTO_TEST_CASE( mtm_import_sequential_txs ) {
    JsonRpcFixture fixture( c_genesisConfigString, true, true, false, true );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    Json::Value txJson;
    txJson["from"] = fixture.coinbase.address().hex();
    txJson["gas"] = "100000";

    txJson["nonce"] = "0";
    TransactionSkeleton ts1 = toTransactionSkeleton( txJson );
    ts1 = fixture.client->populateTransactionWithDefaults( ts1 );
    pair< bool, Secret > ar1 = fixture.accountHolder->authenticate( ts1 );
    Transaction tx1( ts1, ar1.second );

    txJson["nonce"] = "1";
    TransactionSkeleton ts2 = toTransactionSkeleton( txJson );
    ts2 = fixture.client->populateTransactionWithDefaults( ts2 );
    pair< bool, Secret > ar2 = fixture.accountHolder->authenticate( ts2 );
    Transaction tx2( ts2, ar2.second );

    txJson["nonce"] = "2";
    TransactionSkeleton ts3 = toTransactionSkeleton( txJson );
    ts3 = fixture.client->populateTransactionWithDefaults( ts3 );
    pair< bool, Secret > ar3 = fixture.accountHolder->authenticate( ts3 );
    Transaction tx3( ts3, ar3.second );

    h256 h1 = fixture.client->importTransaction( tx1 );
    h256 h2 = fixture.client->importTransaction( tx2 );
    h256 h3 = fixture.client->importTransaction( tx3 );
    BOOST_REQUIRE( h1 );
    BOOST_REQUIRE( h2 );
    BOOST_REQUIRE( h3 );
    BOOST_REQUIRE( fixture.client->transactionQueueStatus().current == 3);
}

BOOST_AUTO_TEST_CASE( mtm_import_future_txs ) {
    JsonRpcFixture fixture( c_genesisConfigString, true, true, false, true );
    dev::eth::simulateMining( *( fixture.client ), 1 );
    auto tq = fixture.client->debugGetTransactionQueue();
    fixture.client->skaleHost()->pauseConsensus( true );

    Json::Value txJson;
    txJson["from"] = fixture.coinbase.address().hex();
    txJson["gas"] = "100000";

    txJson["nonce"] = "0";
    TransactionSkeleton ts1 = toTransactionSkeleton( txJson );
    ts1 = fixture.client->populateTransactionWithDefaults( ts1 );
    pair< bool, Secret > ar1 = fixture.accountHolder->authenticate( ts1 );
    Transaction tx1( ts1, ar1.second );

    txJson["nonce"] = "1";
    TransactionSkeleton ts2 = toTransactionSkeleton( txJson );
    ts2 = fixture.client->populateTransactionWithDefaults( ts2 );
    pair< bool, Secret > ar2 = fixture.accountHolder->authenticate( ts2 );
    Transaction tx2( ts2, ar2.second );

    txJson["nonce"] = "2";
    TransactionSkeleton ts3 = toTransactionSkeleton( txJson );
    ts3 = fixture.client->populateTransactionWithDefaults( ts3 );
    pair< bool, Secret > ar3 = fixture.accountHolder->authenticate( ts3 );
    Transaction tx3( ts3, ar3.second );

    txJson["nonce"] = "3";
    TransactionSkeleton ts4 = toTransactionSkeleton( txJson );
    ts4 = fixture.client->populateTransactionWithDefaults( ts4 );
    pair< bool, Secret > ar4 = fixture.accountHolder->authenticate( ts4 );
    Transaction tx4( ts4, ar4.second );

    txJson["nonce"] = "4";
    TransactionSkeleton ts5 = toTransactionSkeleton( txJson );
    ts5 = fixture.client->populateTransactionWithDefaults( ts5 );
    pair< bool, Secret > ar5 = fixture.accountHolder->authenticate( ts5 );
    Transaction tx5( ts5, ar5.second );

    h256 h1 = fixture.client->importTransaction( tx5 );
    BOOST_REQUIRE( h1 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 1);

    h256 h2 = fixture.client->importTransaction( tx3 );
    BOOST_REQUIRE( h2 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 2);
    h256 h3 = fixture.client->importTransaction( tx2 );
    BOOST_REQUIRE( h3 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 3);

    h256 h4 = fixture.client->importTransaction( tx1 );
    BOOST_REQUIRE( h4 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 1);
    BOOST_REQUIRE_EQUAL( tq->status().current, 3);

    h256 h5 = fixture.client->importTransaction( tx4 );
    BOOST_REQUIRE( h5 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 0);
    BOOST_REQUIRE_EQUAL( tq->status().current, 5);

    fixture.client->skaleHost()->pauseConsensus( false );
}

// TODO: Enable for multitransaction mode checking

// BOOST_AUTO_TEST_CASE( check_multitransaction_mode ) {
//     auto _config = c_genesisConfigString;
//     Json::Value ret;
//     Json::Reader().parse( _config, ret );
//     /* Test contract
//         pragma solidity ^0.8.9;
//         contract Test {
//             function isMTMEnabled() external pure returns (bool) {
//                 return true;
//             }
//         }
//     */
//     ret["accounts"]["0xD2002000000000000000000000000000000000D2"]["code"] = "0x6080604052348015600f57600080fd5b506004361060285760003560e01c8063bad0396e14602d575b600080fd5b60336047565b604051603e91906069565b60405180910390f35b60006001905090565b60008115159050919050565b6063816050565b82525050565b6000602082019050607c6000830184605c565b9291505056fea26469706673582212208d89ce57f69b9b53e8f0808cbaa6fa8fd21a495ab92d0b48b6e47d903989835464736f6c63430008090033"; 
//     Json::FastWriter fastWriter;
//     std::string config = fastWriter.write( ret );
//     JsonRpcFixture fixture( config );
//     bool mtm = fixture.client->checkMultitransactionMode(fixture.client->state(), fixture.client->gasBidPrice());
//     BOOST_REQUIRE( mtm );
// }

// BOOST_AUTO_TEST_CASE( check_multitransaction_mode_false ) {
//     auto _config = c_genesisConfigString;
//     Json::Value ret;
//     Json::Reader().parse( _config, ret );
//     /* Test contract
//         pragma solidity ^0.8.9;
//         contract Test {
//             function isMTMEnabled() external pure returns (bool) {
//                 return false;
//             }
//         }
//     */
//     ret["accounts"]["0xD2002000000000000000000000000000000000D2"]["code"] = "0x6080604052348015600f57600080fd5b506004361060285760003560e01c8063bad0396e14602d575b600080fd5b60336047565b604051603e91906065565b60405180910390f35b600090565b60008115159050919050565b605f81604c565b82525050565b6000602082019050607860008301846058565b9291505056fea2646970667358221220c88541a65627d63d4b0cc04094bc5b2154a2700c97677dcd5de2ee2a27bed58564736f6c63430008090033"; 
//     Json::FastWriter fastWriter;
//     std::string config = fastWriter.write( ret );
//     JsonRpcFixture fixture( config );
//     bool mtm = fixture.client->checkMultitransactionMode(fixture.client->state(), fixture.client->gasBidPrice());
//     BOOST_REQUIRE( !mtm );
// }

// BOOST_AUTO_TEST_CASE( check_multitransaction_mode_empty ) {
//     JsonRpcFixture fixture( c_genesisConfigString );
//     bool mtm = fixture.client->checkMultitransactionMode(fixture.client->state(), fixture.client->gasBidPrice());
//     BOOST_REQUIRE( !mtm );
// }

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
