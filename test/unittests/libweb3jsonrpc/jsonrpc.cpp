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
#include <libethereum/SchainPatch.h>
#include <libethereum/TransactionQueue.h>
#include <libp2p/Network.h>
#include <libskale/httpserveroverride.h>
#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/AdminEth.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include "genesisGeneration2Config.h"
#include <libweb3jsonrpc/Debug.h>
#include <libweb3jsonrpc/Eth.h>
#include <libweb3jsonrpc/ModularServer.h>
#include <libweb3jsonrpc/Net.h>
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

static size_t rand_port = ( srand(time(nullptr)), 1024 + rand() % 64000 );

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
         "istanbulForkBlock": "0x00",
         "skaleDisableChainIdCheck": true,
         "externalGasDifficulty": "0x1"
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
            "basePort": )"+std::to_string( rand_port ) + R"(,
            "logLevel": "trace",
            "logLevelProposal": "trace",
            "testSignatures": true
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "contractStorageLimit": 128,
            "emptyBlockIntervalMs": -1,
            "nodeGroups": {},
            "nodes": [
                { "nodeID": 1112, "ip": "127.0.0.1", "basePort": )"+std::to_string( rand_port ) + R"(, "schainIndex" : 1, "publicKey": "0xfa"}
            ],
            "revertableFSPatchTimestamp": 0,
            "contractStorageZeroValuePatchTimestamp": 0,
            "powCheckPatchTimestamp": 1
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

                function revertCall() public {
                    call();
                    revert();
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
            "code" : "0x6080604052600436106049576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff16806328b5e32b14604e578063f38fb65b146062575b600080fd5b348015605957600080fd5b5060606076565b005b348015606d57600080fd5b50607460ec565b005b6000606060006040805190810160405280600481526020017f7465737400000000000000000000000000000000000000000000000000000000815250915060aa905060405181815260046020820152602083015160408201526001606082015260208160808360006005600019f1935050505050565b60f26076565b600080fd00a165627a7a72305820262a5822c4fe6c154b2ef3198c7827d35fc6da59da2cea2c4f2fad9d4a5ccd5e0029",
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
        },
        "0xD2001300000000000000000000000000000000D4": {
            "balance": "0",
            "nonce": "0",
            "storage": {},
            "code":"0x608060405234801561001057600080fd5b506004361061004c5760003560e01c80632098776714610051578063b8bd717f1461007f578063d37165fa146100ad578063fdde8d66146100db575b600080fd5b61007d6004803603602081101561006757600080fd5b8101908080359060200190929190505050610109565b005b6100ab6004803603602081101561009557600080fd5b8101908080359060200190929190505050610136565b005b6100d9600480360360208110156100c357600080fd5b8101908080359060200190929190505050610170565b005b610107600480360360208110156100f157600080fd5b8101908080359060200190929190505050610191565b005b60005a90505b815a8203101561011e5761010f565b600080fd5b815a8203101561013257610123565b5050565b60005a90505b815a8203101561014b5761013c565b600060011461015957600080fd5b5a90505b815a8203101561016c5761015d565b5050565b60005a9050600081830390505b805a8303101561018c5761017d565b505050565b60005a90505b815a820310156101a657610197565b60016101b157600080fd5b5a90505b815a820310156101c4576101b5565b505056fea264697066735822122089b72532621e7d1849e444ee6efaad4fb8771258e6f79755083dce434e5ac94c64736f6c63430006000033"
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

// chain params needs to be a field of JsonRPCFixture
// since references to it are passed to the server
ChainParams chainParams;


JsonRpcFixture( const std::string& _config = "", bool _owner = true,
                    bool _deploymentControl = true, bool _generation2 = false,
                    bool _mtmEnabled = false, bool _isSyncNode = false, int _emptyBlockIntervalMs = -1,
                    const std::map<std::string, std::string>& params = std::map<std::string, std::string>() ) {


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
            chainParams.EIP158ForkBlock = 0;
            chainParams.constantinopleForkBlock = 0;
            chainParams.istanbulForkBlock = 0;
            chainParams.externalGasDifficulty = 1;
            chainParams.sChain.contractStorageLimit = 128;
            // 615 + 1430 is experimentally-derived block size + average extras size
            chainParams.sChain.dbStorageLimit = 320.5*( 615 + 1430 );
            chainParams.sChain._patchTimestamps[static_cast<size_t>(SchainPatchEnum::ContractStoragePatch)] = 1;
            chainParams.sChain._patchTimestamps[static_cast<size_t>(SchainPatchEnum::StorageDestructionPatch)] = 1;
            powPatchActivationTimestamp = time(nullptr) + 60;
            chainParams.sChain._patchTimestamps[static_cast<size_t>(SchainPatchEnum::CorrectForkInPowPatch)] = powPatchActivationTimestamp;
            push0PatchActivationTimestamp = time(nullptr) + 10;
            chainParams.sChain._patchTimestamps[static_cast<size_t>(SchainPatchEnum::PushZeroPatch)] = push0PatchActivationTimestamp;
            chainParams.sChain.emptyBlockIntervalMs = _emptyBlockIntervalMs;
            // add random extra data to randomize genesis hash and get random DB path,
            // so that tests can be run in parallel
            // TODO: better make it use ethemeral in-memory databases
            chainParams.extraData = h256::random().asBytes();
            chainParams.nodeInfo.port = chainParams.nodeInfo.port6 = rand_port;
            chainParams.sChain.nodes[0].port = chainParams.sChain.nodes[0].port6 = rand_port;
            chainParams.skaleDisableChainIdCheck = true;

            if( params.count("getLogsBlocksLimit") && stoi( params.at( "getLogsBlocksLimit" ) ) )
                chainParams.getLogsBlocksLimit = stoi( params.at( "getLogsBlocksLimit" ) );
        }
        chainParams.sChain.multiTransactionMode = _mtmEnabled;
        chainParams.nodeInfo.syncNode = _isSyncNode;

        auto monitor = make_shared< InstanceMonitor >("test");

        setenv("DATA_DIR", tempDir.path().c_str(), 1);
        client.reset( new eth::ClientTest( chainParams, ( int ) chainParams.networkID,
            shared_ptr< GasPricer >(), NULL, monitor, tempDir.path(), WithExisting::Kill ) );

        client->setAuthor( coinbase.address() );

        // wait for 1st block - because it's always empty
        std::promise< void > blockPromise;
        auto importHandler = client->setOnBlockImport(
            [&blockPromise]( BlockHeader const& ) {
                    blockPromise.set_value();
        } );

        client->injectSkaleHost();
        client->startWorking();

        if ( !_isSyncNode )
            blockPromise.get_future().wait();

        if ( !_generation2 )
            client->setAuthor( coinbase.address() );
        else
            client->setAuthor( chainParams.sChain.blockAuthor );

        using FullServer = ModularServer< rpc::EthFace, rpc::NetFace, rpc::Web3Face,
            rpc::AdminEthFace /*, rpc::AdminNetFace*/, rpc::DebugFace, rpc::TestFace >;

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( {coinbase, account2, account3} );

        sessionManager.reset( new rpc::SessionManager() );
        adminSession =
            sessionManager->newSession( rpc::SessionPermissions{{rpc::Privilege::Admin}} );

        auto ethFace = new rpc::Eth( std::string(""), *client, *accountHolder.get() );

        gasPricer = make_shared< eth::TrivialGasPricer >( 0, DefaultGasPrice );

        rpcServer.reset( new FullServer( ethFace , new rpc::Net( chainParams ),
            new rpc::Web3(),  // TODO Add version parameter here?
            new rpc::AdminEth( *client, *gasPricer, keyManager, *sessionManager ),
            new rpc::Debug( *client, nullptr, ""),
            new rpc::Test( *client ) ) );

        //
        SkaleServerOverride::opts_t serverOpts;

        inject_rapidjson_handlers( serverOpts, ethFace );

        serverOpts.netOpts_.bindOptsStandard_.cntServers_ = 1;
        serverOpts.netOpts_.bindOptsStandard_.strAddrHTTP4_ = chainParams.nodeInfo.ip;
        // random port
        // +3 because rand() seems to be called effectively simultaneously here and in "static" section - thus giving same port for consensus
        serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ = std::rand() % 64000 + 1025 + 3;
        std::cout << "PORT: " << serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ << std::endl;
        skale_server_connector = new SkaleServerOverride( chainParams, client.get(), serverOpts );
        rpcServer->addConnector( skale_server_connector );
        skale_server_connector->StartListening();

        sleep(1);

        auto client = new jsonrpc::HttpClient( "http://" + chainParams.nodeInfo.ip + ":" + std::to_string( serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ ) );
        client->SetTimeout(1000000000);

        rpcClient = unique_ptr< WebThreeStubClient >( new WebThreeStubClient( *client ) );


        BOOST_TEST_MESSAGE("Constructed JsonRpcFixture");

    }

    ~JsonRpcFixture() {
        if ( skale_server_connector )
            skale_server_connector->StopListening();
        BOOST_TEST_MESSAGE("Destructed JsonRpcFixture");
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

    string estimateGasShouldFail( Json::Value const& _t ) {
        try {
            rpcClient->eth_estimateGas( _t );
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
    time_t powPatchActivationTimestamp;
    time_t push0PatchActivationTimestamp;
};

struct RestrictedAddressFixture : public JsonRpcFixture {
    RestrictedAddressFixture( const std::string& _config = c_genesisConfigString ) : JsonRpcFixture( _config ) {
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

BOOST_AUTO_TEST_CASE( jsonrpc_netVersion )
{
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 65535
    ret["params"]["chainID"] = "0xffff";

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    auto version = fixture.rpcClient->net_version();
    BOOST_CHECK_EQUAL( version, "65535" );
}

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

BOOST_AUTO_TEST_CASE( send_raw_tx_sync ) {
    // Enable sync mode
    JsonRpcFixture fixture( c_genesisConfigString, true, true, true, false, true );
    Address senderAddress = fixture.coinbase.address();
    fixture.client->setAuthor( senderAddress );

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
    create["gas"] = "180000";

    BOOST_REQUIRE( fixture.client->transactionQueueStatus().current == 0);

    // Sending tx to sync node
    string txHash = fixture.rpcClient->eth_sendTransaction( create );

    auto pendingTransactions = fixture.client->pending();
    BOOST_REQUIRE( pendingTransactions.size() == 1);
    auto txHashFromQueue = "0x" + pendingTransactions[0].sha3().hex();
    BOOST_REQUIRE( txHashFromQueue == txHash );
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

    // pragma solidity 0.8.4;
    // contract test {
    //     uint value;
    //     function f(uint a) public pure returns(uint d) {
    //         return a * 7;
    //     }
    //     function setValue(uint _value) external {
    //         value = _value;
    //     }
    // }

    string compiled =
        "608060405234801561001057600080fd5b506101ef8061002060003"
        "96000f3fe608060405234801561001057600080fd5b506004361061"
        "00365760003560e01c8063552410771461003b578063b3de648b146"
        "10057575b600080fd5b610055600480360381019061005091906100"
        "bc565b610087565b005b610071600480360381019061006c9190610"
        "0bc565b610091565b60405161007e91906100f4565b604051809103"
        "90f35b8060008190555050565b60006007826100a0919061010f565"
        "b9050919050565b6000813590506100b6816101a2565b9291505056"
        "5b6000602082840312156100ce57600080fd5b60006100dc8482850"
        "16100a7565b91505092915050565b6100ee81610169565b82525050"
        "565b600060208201905061010960008301846100e5565b929150505"
        "65b600061011a82610169565b915061012583610169565b9250817f"
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "fffffffff048311821515161561015e5761015d610173565b5b8282"
        "02905092915050565b6000819050919050565b7f4e487b710000000"
        "0000000000000000000000000000000000000000000000000600052"
        "601160045260246000fd5b6101ab81610169565b81146101b657600"
        "080fd5b5056fea26469706673582212200be8156151b5ef7c250fa7"
        "b8c8ed4e2a1c32cd526f9c868223f6838fa1193c9e64736f6c63430"
        "008040033";

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

    Json::Value inputCall;
    inputCall["to"] = contractAddress;
    inputCall["input"] = "0xb3de648b0000000000000000000000000000000000000000000000000000000000000001";
    inputCall["gas"] = "1000000";
    inputCall["gasPrice"] = "0";
    result = fixture.rpcClient->eth_call( inputCall, "latest" );
    BOOST_CHECK_EQUAL(
        result, "0x0000000000000000000000000000000000000000000000000000000000000007" );

    Json::Value transact;
    transact["to"] = contractAddress;
    transact["data"] = "0x552410770000000000000000000000000000000000000000000000000000000000000001";
    txHash = fixture.rpcClient->eth_sendTransaction( transact );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    auto res = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( res["status"], string( "0x1" ) );

    Json::Value inputTx;
    inputTx["to"] = contractAddress;
    inputTx["input"] = "0x552410770000000000000000000000000000000000000000000000000000000000000002";
    txHash = fixture.rpcClient->eth_sendTransaction( inputTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    res = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( res["status"], string( "0x1" ) );
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

BOOST_AUTO_TEST_CASE( push0_patch_activation ) {
    JsonRpcFixture fixture;
    auto senderAddress = fixture.coinbase.address();

    fixture.client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    fixture.client->setAuthor( fixture.account2.address() );
    dev::eth::simulateMining( *( fixture.client ), 1 );

/*
// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.8.2;

contract Push0Test {
    fallback() external payable {
        assembly {
            let t := add(9, 10)
        }
    }
}

then convert to yul: solc --ir p0test.sol >p0test.yul

then change code:
                {
                    let r := add(88,99)
                    let tmp := verbatim_0i_1o(hex"5f")
                }

then compile!

*/
    string compiled =
        "608060405234156100135761001261003b565b5b61001b610040565b610023610031565b6101b761004382396101b781f35b6000604051905090565b600080fd5b56fe608060405261000f36600061015b565b805160208201f35b60006060905090565b6000604051905090565b6000601f19601f8301169050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b6100738261002a565b810181811067ffffffffffffffff821117156100925761009161003b565b5b80604052505050565b60006100a5610020565b90506100b1828261006a565b919050565b600067ffffffffffffffff8211156100d1576100d061003b565b5b6100da8261002a565b9050602081019050919050565b60006100f2826100b6565b6100fb8161009b565b915082825250919050565b7f7375636365737300000000000000000000000000000000000000000000000000600082015250565b600061013b60076100e7565b905061014960208201610106565b90565b600061015661012f565b905090565b6000610165610017565b809150600a6009015f505061017861014c565b9150509291505056fea2646970667358221220b3871ed09fbcbb1dac74c3cd48dafa5d097bea7c808b5ff2c16a996cf108d3c664736f6c63430008190033";
//        "60806040523415601057600f6031565b5b60166036565b601c6027565b604c60398239604c81f35b6000604051905090565b600080fd5b56fe6080604052600a600c565b005b60636058015f505056fea2646970667358221220ee9861b869ceda6de64f2ec7ccbebf2babce54b35502a866a4193e05ae595e1f64736f6c63430008130033";

    Json::Value create;

    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "1000000";

    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );      // deploy should succeed
    string contractAddress = receipt["contractAddress"].asString();

    Json::Value callObject;

    callObject["from"] = toJS( fixture.account2.address() );
    callObject["to"] = contractAddress;

    // first try without PushZeroPatch

    txHash = fixture.rpcClient->eth_sendTransaction( callObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x0" ) );      // exec should fail

    string callResult = fixture.rpcClient->eth_call(callObject, "latest");
    BOOST_REQUIRE_EQUAL( callResult, string( "0x" ) );              // call too

    // wait for block after timestamp
    BOOST_REQUIRE_LT( fixture.client->blockInfo(LatestBlock).timestamp(), fixture.push0PatchActivationTimestamp );
    while( time(nullptr) < fixture.push0PatchActivationTimestamp )
        sleep(1);

    // 1st timestamp-crossing block
    txHash = fixture.rpcClient->eth_sendTransaction( callObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE_GE( fixture.client->blockInfo(LatestBlock).timestamp(), fixture.push0PatchActivationTimestamp );

    uint64_t crossingBlockNumber = fixture.client->number();
    (void) crossingBlockNumber;

    // in the "corssing" block tx still should fail
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x0" ) );

    // in 1st block with patch call should succeed
    callResult = fixture.rpcClient->eth_call(callObject, "latest");
    BOOST_REQUIRE_NE( callResult, string( "0x" ) );

    // tx should succeed too
    txHash = fixture.rpcClient->eth_sendTransaction( callObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );

#ifdef HISTORIC_STATE
    // histoic call should fail before activation and succees after it

    callResult = fixture.rpcClient->eth_call(callObject, toJS(crossingBlockNumber-1));
    BOOST_REQUIRE_EQUAL( callResult, string( "0x" ) );

    callResult = fixture.rpcClient->eth_call(callObject, toJS(crossingBlockNumber));
    BOOST_REQUIRE_NE( callResult, string( "0x" ) );
#endif
}

BOOST_AUTO_TEST_CASE( eth_estimateGas ) {
    JsonRpcFixture fixture( c_genesisConfigString );

    //    This contract is predeployed on SKALE test network
    //    on address 0xD2001300000000000000000000000000000000D4

    //    pragma solidity 0.6.0;
    //    contract Test {
    //            ...
    //            function testRequire(uint gasConsumed) public {
    //                uint initialGas = gasleft();
    //                while (initialGas - gasleft() < gasConsumed) {}
    //                require(1 == 0);
    //                initialGas = gasleft();
    //                while (initialGas - gasleft() < gasConsumed) {}
    //            }
    //
    //            function testRevert(uint gasConsumed) public {
    //                uint initialGas = gasleft();
    //                while (initialGas - gasleft() < gasConsumed) {}
    //                revert();
    //                initialGas = gasleft();
    //                while (initialGas - gasleft() < gasConsumed) {}
    //            }
    //
    //            function testRequireOff(uint gasConsumed) public {
    //                uint initialGas = gasleft();
    //                while (initialGas - gasleft() < gasConsumed) {}
    //                require(true);
    //                initialGas = gasleft();
    //                while (initialGas - gasleft() < gasConsumed) {}
    //            }
    //    }

    // data to call method testRevert(50000)

    Json::Value testRevert;
    testRevert["to"] = "0xD2001300000000000000000000000000000000D4";
    testRevert["data"] = "0x20987767000000000000000000000000000000000000000000000000000000000000c350";
    string response = fixture.estimateGasShouldFail( testRevert );
    BOOST_CHECK( response.find("EVM revert instruction without description message") != string::npos );

    Json::Value testPositive;
    testPositive["to"] = "0xD2001300000000000000000000000000000000D4";
    testPositive["data"] = "0xfdde8d66000000000000000000000000000000000000000000000000000000000000c350";
    response = fixture.rpcClient->eth_estimateGas( testPositive );
    string response2 = fixture.rpcClient->eth_estimateGas( testPositive, "latest" );
    string response3 = fixture.rpcClient->eth_estimateGas( testPositive, "1" );
    BOOST_CHECK_EQUAL( response, "0x1db20" );
    BOOST_CHECK_EQUAL( response2, "0x1db20" );
    BOOST_CHECK_EQUAL( response3, "0x1db20" );
}

BOOST_AUTO_TEST_CASE( eth_estimateGas_chainId ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 65535
    ret["params"]["chainID"] = "0xffff"; 

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    // pragma solidity ^0.8.13;

    // contract Counter {
    //     error BlockNumber(uint256 blockNumber);

    //     constructor() {
    //         revert BlockNumber(block.chainid);
    //     }
    // }

    Json::Value testRevert;
    testRevert["data"] = "0x6080604052348015600f57600080fd5b50604051633013bad360e21b815246600482015260240160405180910390fdfe";

    try {
        fixture.rpcClient->eth_estimateGas( testRevert, "latest" );
    } catch ( jsonrpc::JsonRpcException& ex) {
        BOOST_CHECK_EQUAL(ex.GetCode(), 3);
        BOOST_CHECK_EQUAL(ex.GetData().asString(), "0xc04eeb4c000000000000000000000000000000000000000000000000000000000000ffff");
        BOOST_CHECK_EQUAL(ex.GetMessage(), "EVM revert instruction without description message");
    } 
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


BOOST_AUTO_TEST_CASE( call_with_error ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );

    // pragma solidity ^0.8.0;
    // contract BasicCustomErrorContract {
    //     // Define custom errors
    //     error InsufficientBalance();
    //     error Unauthorized();
    //     address public owner;
    //     // Function only callable by the owner
    //     function ownerOnlyFunction() external {
    //         revert Unauthorized();
    //     }
    // }

    string compiled =
        "608060405234801561001057600080fd5b50610168806100206000396000f3fe608060"
        "405234801561001057600080fd5b5060043610610053576000357c0100000000000000"
        "000000000000000000000000000000000000000000900480638da5cb5b146100585780"
        "63e021c20614610076575b600080fd5b610060610080565b60405161006d9190610117"
        "565b60405180910390f35b61007e6100a4565b005b60008054906101000a900473ffff"
        "ffffffffffffffffffffffffffffffffffff1681565b6040517f82b429000000000000"
        "0000000000000000000000000000000000000000000000815260040160405180910390"
        "fd5b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b60"
        "00610101826100d6565b9050919050565b610111816100f6565b82525050565b600060"
        "208201905061012c6000830184610108565b9291505056fea264697066735822122013"
        "2ca0f4158a0540a7e67f304c94305f81bbe52de2314e2b9cee92a2c74e103a64736f6c"
        "63430008120033";

    auto senderAddress = fixture.coinbase.address();

    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "180000"; 
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress = receipt["contractAddress"].asString();

    Json::Value transactionCallObject;
    transactionCallObject["to"] = contractAddress;
    transactionCallObject["data"] = "0xe021c206";

    try {
        fixture.rpcClient->eth_call( transactionCallObject, "latest" );
    } catch ( jsonrpc::JsonRpcException& ex) {
        BOOST_CHECK_EQUAL(ex.GetCode(), 3);
        BOOST_CHECK_EQUAL(ex.GetData().asString(), "0x82b42900");
        BOOST_CHECK_EQUAL(ex.GetMessage(), "EVM revert instruction without description message");
    } 
}

BOOST_AUTO_TEST_CASE( estimate_gas_with_error ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );

    // pragma solidity ^0.8.0;
    // contract BasicCustomErrorContract {
    //     // Define custom errors
    //     error InsufficientBalance();
    //     error Unauthorized();
    //     address public owner;
    //     // Function only callable by the owner
    //     function ownerOnlyFunction() external {
    //         revert Unauthorized();
    //     }
    // }

    string compiled =
        "608060405234801561001057600080fd5b50610168806100206000396000f3fe608060"
        "405234801561001057600080fd5b5060043610610053576000357c0100000000000000"
        "000000000000000000000000000000000000000000900480638da5cb5b146100585780"
        "63e021c20614610076575b600080fd5b610060610080565b60405161006d9190610117"
        "565b60405180910390f35b61007e6100a4565b005b60008054906101000a900473ffff"
        "ffffffffffffffffffffffffffffffffffff1681565b6040517f82b429000000000000"
        "0000000000000000000000000000000000000000000000815260040160405180910390"
        "fd5b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b60"
        "00610101826100d6565b9050919050565b610111816100f6565b82525050565b600060"
        "208201905061012c6000830184610108565b9291505056fea264697066735822122013"
        "2ca0f4158a0540a7e67f304c94305f81bbe52de2314e2b9cee92a2c74e103a64736f6c"
        "63430008120033";
    
    auto senderAddress = fixture.coinbase.address();

    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "180000"; 
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress = receipt["contractAddress"].asString();

    Json::Value transactionCallObject;
    transactionCallObject["to"] = contractAddress;
    transactionCallObject["data"] = "0xe021c206";

    try {
        fixture.rpcClient->eth_estimateGas( transactionCallObject, "latest" );
    } catch ( jsonrpc::JsonRpcException& ex) {
        BOOST_CHECK_EQUAL(ex.GetCode(), 3);
        BOOST_CHECK_EQUAL(ex.GetData().asString(), "0x82b42900");
        BOOST_CHECK_EQUAL(ex.GetMessage(), "EVM revert instruction without description message");
    } 
}

BOOST_AUTO_TEST_CASE( simplePoWTransaction ) {
    // 1s empty block interval
    JsonRpcFixture fixture( "", true, true, false, false, false, 1000 );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    auto senderAddress = fixture.coinbase.address();

    Json::Value transact;
    transact["from"] = toJS( senderAddress );
    transact["to"] = toJS( senderAddress );
    // 1k
    ostringstream ss("0x");
    for(int i=0; i<1024/16; ++i)
        ss << "112233445566778899aabbccddeeff11";
    transact["data"] = ss.str();

    string gasEstimateStr = fixture.rpcClient->eth_estimateGas(transact);
    u256 gasEstimate = jsToU256(gasEstimateStr);

    // old estimate before patch
    BOOST_REQUIRE_EQUAL(gasEstimate, u256(21000+1024*68));

    u256 powGasPrice = 0;
    u256 correctEstimate = u256(21000+1024*16);
    do {
        const u256 GAS_PER_HASH = 1;
        u256 candidate = h256::random();
        h256 hash = dev::sha3( senderAddress ) ^ dev::sha3( u256( 0 ) ) ^ dev::sha3( candidate );
        u256 externalGas = ~u256( 0 ) / u256( hash ) * GAS_PER_HASH;
        if ( externalGas >= correctEstimate && externalGas < correctEstimate + correctEstimate/10 ) {
            powGasPrice = candidate;
        }
    } while ( !powGasPrice );
    // Account balance is too low will mean that PoW didn't work out
    transact["gasPrice"] = toJS( powGasPrice );

    // wait for patch turning on and see how it happens
    string txHash;
    BlockHeader badInfo, goodInfo;
    for(;;) {
        string gasEstimateStr = fixture.rpcClient->eth_estimateGas(transact);
        u256 gasEstimate = jsToU256(gasEstimateStr);
        // old
        if(gasEstimate == u256(21000+1024*68)){
            try{
                fixture.rpcClient->eth_sendTransaction( transact );
                BOOST_REQUIRE(false);
            } catch(const std::exception& ex) {
                assert(string(ex.what()).find("balance is too low") != string::npos);
                badInfo = fixture.client->blockInfo(fixture.client->hashFromNumber(LatestBlock));
                dev::eth::mineTransaction( *( fixture.client ), 1 ); // empty block
            } // catch
        }
        // new
        else {
            BOOST_REQUIRE_EQUAL(gasEstimate, correctEstimate);
            txHash = fixture.rpcClient->eth_sendTransaction( transact );
            goodInfo = fixture.client->blockInfo(fixture.client->hashFromNumber(LatestBlock));
            break;
        } // else
    } // for

    BOOST_REQUIRE_LT(badInfo.timestamp(), fixture.powPatchActivationTimestamp);
    BOOST_REQUIRE_GE(goodInfo.timestamp(), fixture.powPatchActivationTimestamp);
    BOOST_REQUIRE_EQUAL(badInfo.number()+1, goodInfo.number());

    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL(receipt["status"], "0x1");
}

BOOST_AUTO_TEST_CASE( recalculateExternalGas ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 21
    std::string chainID = "0x15";
    ret["params"]["chainID"] = chainID;

    // remove deployment control
    auto accounts = ret["accounts"];
    accounts.removeMember( "0xD2002000000000000000000000000000000000D2" );
    ret["accounts"] = accounts;

    // setup patch
    time_t externalGasPatchActivationTimestamp = time(nullptr) + 10;
    ret["skaleConfig"]["sChain"]["ExternalGasPatchTimestamp"] = externalGasPatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config, true, false );
    dev::eth::simulateMining( *( fixture.client ), 20 );

    auto senderAddress = fixture.coinbase.address().hex();

//    // SPDX-License-Identifier: GPL-3.0

//    pragma solidity >=0.8.2 <0.9.0;

//    /**
//     * @title Storage
//     * @dev Store & retrieve value in a variable
//     * @custom:dev-run-script ./scripts/deploy_with_ethers.ts
//     */
//    contract Storage {

//        uint256 number;
//        uint256 number1;
//        uint256 number2;

//        /**
//         * @dev Store value in variable
//         * @param num value to store
//         */
//        function store(uint256 num) public {
//            number = num;
//            number1 = num;
//            number2 = num;
//        }

//        /**
//         * @dev Return value
//         * @return value of 'number'
//         */
//        function retrieve() public view returns (uint256){
//            return number;
//        }
//    }
    std::string bytecode = "608060405234801561001057600080fd5b5061015e806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100e7565b60405180910390f35b610073600480360381019061006e91906100ab565b61007e565b005b60008054905090565b80600081905550806001819055508060028190555050565b6000813590506100a581610111565b92915050565b6000602082840312156100c1576100c061010c565b5b60006100cf84828501610096565b91505092915050565b6100e181610102565b82525050565b60006020820190506100fc60008301846100d8565b92915050565b6000819050919050565b600080fd5b61011a81610102565b811461012557600080fd5b5056fea2646970667358221220780703bb6ac2eec922a510d57edcae39b852b578e7f63a263ddb936758dc9c4264736f6c63430008070033";

    // deploy contact
    Json::Value create;
    create["from"] = senderAddress;
    create["code"] = bytecode;
    create["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    create["gas"] = 140000;
    create["nonce"] = 0;

    std::string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"].asString() == "0x1" );
    std::string contractAddress = receipt["contractAddress"].asString();

    // send txn to a contract from the suspicious account
    // store( 4 )
    Json::Value txn;
    txn["from"] = "0x40797bb29d12FC0dFD04277D16a3Dd4FAc3a6e5B";
    txn["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000004";
    txn["gasPrice"] = "0xdffe55527a88d3775c23ecd3ae38ff1e90caf12b5beb4f7ea3ad998a990a895c";
    txn["gas"] = 140000;
    txn["chainId"] = "0x15";
    txn["nonce"] = 0;
    txn["to"] = contractAddress;

    auto ts = toTransactionSkeleton( txn );
    auto t = dev::eth::Transaction( ts, dev::Secret( "7be24de049f2d0d4ecaeaa81564aecf647fa7a4c86264243d77e01da25d859a0" ) );

    txHash = fixture.rpcClient->eth_sendRawTransaction( dev::toHex( t.toBytes() ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );

    BOOST_REQUIRE( receipt["status"].asString() == "0x0" );
    BOOST_REQUIRE( receipt["gasUsed"].asString() == "0x61cb" );

    sleep(10);

    // push new block to update timestamp
    Json::Value refill;
    refill["from"] = senderAddress;
    refill["to"] = dev::Address::random().hex();
    refill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    refill["value"] = 100;
    refill["nonce"] = 1;

    txHash = fixture.rpcClient->eth_sendTransaction( refill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // send txn to a contract from another suspicious account
    // store( 4 )
    txn["from"] = "0x5cdb7527ec85022991D4e27F254C438E8337ad7E";
    txn["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000004";
    txn["gasPrice"] = "0x974749a06d5cd0dba6a4e1f3d14d5f480db716dcbc9a34ec5496b8d86e99f898";
    txn["gas"] = 140000;
    txn["chainId"] = "0x15";
    txn["nonce"] = 0;
    txn["to"] = contractAddress;

    ts = toTransactionSkeleton( txn );
    t = dev::eth::Transaction( ts, dev::Secret( "8df08814fcfc169aad0015654114be06c28b27bdcdef286cf4dbd5e2950a3ffc" ) );

    txHash = fixture.rpcClient->eth_sendRawTransaction( dev::toHex( t.toBytes() ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );

    BOOST_REQUIRE( receipt["status"].asString() == "0x1" );
    BOOST_REQUIRE( receipt["gasUsed"].asString() == "0x13ef4" );
}

BOOST_AUTO_TEST_CASE( skipTransactionExecution ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 21
    std::string chainID = "0x15";
    ret["params"]["chainID"] = chainID;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );
    dev::eth::simulateMining( *( fixture.client ), 20 );

    auto senderAddress = fixture.coinbase.address().hex();

    Json::Value refill;
    refill["from"] = senderAddress;
    refill["to"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    refill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    refill["value"] = 1000000000000000;
    refill["nonce"] = 0;

    std::string txHash = fixture.rpcClient->eth_sendTransaction( refill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // send txn and verify that gas used is correct
    // gas used value is hardcoded in State::txnsToSkipExecution
    Json::Value txn;
    txn["from"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    txn["gasPrice"] = "0x4a817c800";
    txn["gas"] = 40000;
    txn["chainId"] = "0x15";
    txn["nonce"] = 0;
    txn["value"] = 1;
    txn["to"] = "0x5cdb7527ec85022991D4e27F254C438E8337ad7E";

    auto ts = toTransactionSkeleton( txn );
    auto t = dev::eth::Transaction( ts, dev::Secret( "08cee1f4bc8c37f88124bb3fc64566ccd35dbeeac84c62300f6b8809cab9ea2f" ) );

    txHash = fixture.rpcClient->eth_sendRawTransaction( dev::toHex( t.toBytes() ) );
    BOOST_REQUIRE( txHash == "0x95fb5557db8cc6de0aff3a64c18a6d9378b0d312b24f5d77e8dbf5cc0612d74f" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["gasUsed"].asString() == "0x5ac0" );
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

    Json::Value transact;
    transact["from"] = toJS( address2 );
    transact["to"] = contractAddress;
    transact["data"] = "0x15b2eec30000000000000000000000000000000000000000000000000000000000000003";

    string gasEstimateStr = fixture.rpcClient->eth_estimateGas(transact);
    u256 gasEstimate = jsToU256(gasEstimateStr);

    u256 powGasPrice = 0;
    do {
        const u256 GAS_PER_HASH = 1;
        u256 candidate = h256::random();
        h256 hash = dev::sha3( address2 ) ^ dev::sha3( u256( 0 ) ) ^ dev::sha3( candidate );
        u256 externalGas = ~u256( 0 ) / u256( hash ) * GAS_PER_HASH;
        if ( externalGas >= gasEstimate && externalGas < gasEstimate + gasEstimate/10 ) {
            powGasPrice = candidate;
        }
    } while ( !powGasPrice );
    transact["gasPrice"] = toJS( powGasPrice );

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

// limit on getLogs output
BOOST_AUTO_TEST_CASE( getLogs_limit ) {
    JsonRpcFixture fixture( "", true, true, false, false, false, -1,
                           {{"getLogsBlocksLimit", "10"}} );

    dev::eth::simulateMining( *( fixture.client ), 1 );

    /*
 // SPDX-License-Identifier: None
pragma solidity ^0.8;
contract Logger{
    event DummyEvent(uint256, uint256);
    fallback() external payable {
        for(uint i=0; i<100; ++i)
            emit DummyEvent(block.number, i);
    }
}
*/

    string bytecode = "6080604052348015600e575f80fd5b5060c080601a5f395ff3fe60806040525f5b6064811015604f577f90778767414a5c844b9d35a8745f67697ee3b8c2c3f4feafe5d9a3e234a5a3654382604051603d9291906067565b60405180910390a18060010190506006565b005b5f819050919050565b6061816051565b82525050565b5f60408201905060785f830185605a565b60836020830184605a565b939250505056fea264697066735822122040208e35f2706dd92c17579466ab671c308efec51f558a755ea2cf81105ab22964736f6c63430008190033";

    Json::Value create;
    create["code"] = bytecode;
    create["gas"] = "180000";  // TODO or change global default of 90000?

    string deployHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value deployReceipt = fixture.rpcClient->eth_getTransactionReceipt( deployHash );
    string contractAddress = deployReceipt["contractAddress"].asString();

    // generate 10 blocks 10 logs each

    Json::Value t;
    t["from"] = toJS( fixture.coinbase.address() );
    t["value"] = jsToDecimal( "0" );
    t["to"] = contractAddress;
    t["gas"] = "99000";

    for(int i=0; i<11; ++i){

        std::string txHash = fixture.rpcClient->eth_sendTransaction( t );
        BOOST_REQUIRE( !txHash.empty() );
        dev::eth::mineTransaction( *( fixture.client ), 1 );
        Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
        BOOST_REQUIRE_EQUAL(receipt["status"], "0x1");
    }

    // ask for logs
    Json::Value req;
    req["fromBlock"] = 1;
    req["toBlock"] = 11;
    req["topics"] = Json::Value(Json::arrayValue);

    // 1 10 blocks
    BOOST_REQUIRE_NO_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req) );

    // 2 with topics
    req["address"] = contractAddress;
    BOOST_REQUIRE_NO_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req) );

    // 3 11 blocks
    req["toBlock"] = 12;
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req), std::exception );

    // 4 filter
    string filterId = fixture.rpcClient->eth_newFilter( req );
    BOOST_REQUIRE_THROW( Json::Value res = fixture.rpcClient->eth_getFilterLogs(filterId), std::exception );
    BOOST_REQUIRE_NO_THROW( Json::Value res = fixture.rpcClient->eth_getFilterChanges(filterId) );
}

// test blockHash parameter
BOOST_AUTO_TEST_CASE( getLogs_blockHash ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );

    string latestHash = fixture.rpcClient->eth_getBlockByNumber("latest", false)["hash"].asString();

    Json::Value req;
    req["blockHash"] = "xyz";
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req), std::exception );

    req["blockHash"] = Json::Value(Json::arrayValue);
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req), std::exception );

    req["fromBlock"] = 1;
    req["toBlock"] = 1;
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req), std::exception );

    req["blockHash"] = latestHash;
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req), std::exception );

    req.removeMember("fromBlock");
    req.removeMember("toBlock");
    BOOST_REQUIRE_NO_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req) );

    req["blockHash"] = "0x88df016429689c079f3b2f6ad39fa052532c56795b733da78a91ebe6a713944b";
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req), std::exception );

    req["blockHash"] = "";
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs(req), std::exception );
}

BOOST_AUTO_TEST_CASE( estimate_gas_low_gas_txn ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 10 );

    auto senderAddress = fixture.coinbase.address();

/*
// SPDX-License-Identifier: None
pragma solidity ^0.6.0;

contract TestEstimateGas {
    uint256[256] number;
    uint256 counter = 0;

    function store(uint256 x) public {
        number[counter] = x;
        counter += 1;
    }

    function clear(uint256 pos) public {
        number[pos] = 0;
    }
}
*/

    string bytecode = "608060405260006101005534801561001657600080fd5b50610104806100266000396000f3fe6080604052348015600f57600080fd5b506004361060325760003560e01c80636057361d146037578063c0fe1af8146062575b600080fd5b606060048036036020811015604b57600080fd5b8101908080359060200190929190505050608d565b005b608b60048036036020811015607657600080fd5b810190808035906020019092919050505060b8565b005b806000610100546101008110609e57fe5b018190555060016101006000828254019250508190555050565b60008082610100811060c657fe5b01819055505056fea26469706673582212206c8da972693a5b8c9bf59c197c4a0c554e9f51abd20047572c9c19125b533d2964736f6c634300060c0033";

    Json::Value create;
    create["code"] = bytecode;
    create["gas"] = "180000";  // TODO or change global default of 90000?

    string deployHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value deployReceipt = fixture.rpcClient->eth_getTransactionReceipt( deployHash );
    string contractAddress = deployReceipt["contractAddress"].asString();

    Json::Value txStore1;  // call store(1)
    txStore1["to"] = contractAddress;
    txStore1["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000001";
    txStore1["from"] = toJS( senderAddress );
    txStore1["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    fixture.rpcClient->eth_sendTransaction( txStore1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value estimateGasCall;  // call clear(0)
    estimateGasCall["to"] = contractAddress;
    estimateGasCall["data"] = "0xc0fe1af80000000000000000000000000000000000000000000000000000000000000000";
    estimateGasCall["from"] = toJS( senderAddress );
    estimateGasCall["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    string estimatedGas = fixture.rpcClient->eth_estimateGas( estimateGasCall );

    dev::bytes data = dev::jsToBytes( estimateGasCall["data"].asString() );
    BOOST_REQUIRE( dev::jsToU256( estimatedGas ) > dev::eth::TransactionBase::baseGasRequired(
                       false, &data, fixture.client->chainParams().makeEvmSchedule(
                       fixture.client->latestBlock().info().timestamp(), fixture.client->number() ) ) );

    // try to send with this gas
    estimateGasCall["gas"] = toJS( jsToInt( estimatedGas ) );
    string clearHash = fixture.rpcClient->eth_sendTransaction( estimateGasCall );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    Json::Value clearReceipt = fixture.rpcClient->eth_getTransactionReceipt( clearHash );
    BOOST_REQUIRE_EQUAL(clearReceipt["status"], "0x1");
    BOOST_REQUIRE_LT(jsToInt(clearReceipt["gasUsed"].asString()), 21000);

    // try to lower gas
    estimateGasCall["gas"] = toJS( jsToInt( estimatedGas ) - 1 );
    clearHash = fixture.rpcClient->eth_sendTransaction( estimateGasCall );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    clearReceipt = fixture.rpcClient->eth_getTransactionReceipt( clearHash );
    BOOST_REQUIRE_EQUAL(clearReceipt["status"], "0x0");
    BOOST_REQUIRE_GT(jsToInt(clearReceipt["gasUsed"].asString()), 21000);
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

BOOST_AUTO_TEST_CASE( storage_limit_reverted ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1000 );
//    pragma solidity >=0.7.0 <0.9.0;

//    contract Storage {

//        uint256[10] number;

//        /**
//         * @dev Store value in variable
//         * @param num value to store
//         */
//        function store(uint256 num, uint256 pos) public {
//            number[pos] = num;
//        }
//    }
    std::string bytecode1 = "0x608060405234801561001057600080fd5b50610134806100206000396000f3fe6080604052348015600f57600080fd5b506004361060285760003560e01c80636ed28ed014602d575b600080fd5b60436004803603810190603f91906096565b6045565b005b81600082600a8110605757605660cf565b5b01819055505050565b600080fd5b6000819050919050565b6076816065565b8114608057600080fd5b50565b600081359050609081606f565b92915050565b6000806040838503121560aa5760a96060565b5b600060b6858286016083565b925050602060c5858286016083565b9150509250929050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052603260045260246000fdfea2646970667358221220ec8739ad7fc74a76053c683510b3c836d01c7eda3687d89e65380260a97e741b64736f6c63430008120033";
    auto senderAddress = fixture.coinbase.address();

    Json::Value create1;
    create1["from"] = toJS( senderAddress );
    create1["data"] = bytecode1;
    create1["gas"] = "1800000";
    string txHash = fixture.rpcClient->eth_sendTransaction( create1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt1 = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt1["status"] == string( "0x1" ) );
    string contractAddress1 = receipt1["contractAddress"].asString();

//    contract CallTry {

//        bool success;
//        uint256 count;
//        address storageAddress;

//        event Message(string mes);

//        constructor(address newAddress) {
//            storageAddress = newAddress;
//        }

//        function storeTry() public {
//            count = 1;
//            success = true;
//            try Storage(storageAddress).store(10, 10) {
//                emit Message("true");
//            }  catch Error(string memory reason) {
//                emit Message(reason);
//            } catch Panic(uint errorCode) {
//                emit Message(string(abi.encodePacked(errorCode)));
//            } catch (bytes memory revertData) {
//                emit Message(string(revertData));
//            }contractAddress1
//            count = 0;
//            success = false;
//        }
//    }

    std::string bytecode2 = "608060405234801561001057600080fd5b506040516106f93803806106f9833981810160405281019061003291906100dc565b80600260006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff16021790555050610109565b600080fd5b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b60006100a98261007e565b9050919050565b6100b98161009e565b81146100c457600080fd5b50565b6000815190506100d6816100b0565b92915050565b6000602082840312156100f2576100f1610079565b5b6000610100848285016100c7565b91505092915050565b6105e1806101186000396000f3fe608060405234801561001057600080fd5b506004361061002b5760003560e01c8063c18829ca14610030575b600080fd5b61003861003a565b005b6001808190555060016000806101000a81548160ff021916908315150217905550600260009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16636ed28ed0600a806040518363ffffffff1660e01b81526004016100b99291906102de565b600060405180830381600087803b1580156100d357600080fd5b505af19250505080156100e4575060015b610235576100f0610314565b806308c379a00361014c57506101046103b1565b8061010f57506101c5565b7f51a7f65c6325882f237d4aeb43228179cfad48b868511d508e24b4437a8191378160405161013e91906104c0565b60405180910390a150610230565b634e487b71036101c55761015e6104e2565b9061016957506101c5565b7f51a7f65c6325882f237d4aeb43228179cfad48b868511d508e24b4437a8191378160405160200161019b9190610524565b6040516020818303038152906040526040516101b791906104c0565b60405180910390a150610230565b3d80600081146101f1576040519150601f19603f3d011682016040523d82523d6000602084013e6101f6565b606091505b507f51a7f65c6325882f237d4aeb43228179cfad48b868511d508e24b4437a8191378160405161022691906104c0565b60405180910390a1505b61026b565b7f51a7f65c6325882f237d4aeb43228179cfad48b868511d508e24b4437a8191376040516102629061058b565b60405180910390a15b600060018190555060008060006101000a81548160ff021916908315150217905550565b6000819050919050565b6000819050919050565b6000819050919050565b60006102c86102c36102be8461028f565b6102a3565b610299565b9050919050565b6102d8816102ad565b82525050565b60006040820190506102f360008301856102cf565b61030060208301846102cf565b9392505050565b60008160e01c9050919050565b600060033d11156103335760046000803e610330600051610307565b90505b90565b6000604051905090565b6000601f19601f8301169050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b61038982610340565b810181811067ffffffffffffffff821117156103a8576103a7610351565b5b80604052505050565b600060443d1061043e576103c3610336565b60043d036004823e80513d602482011167ffffffffffffffff821117156103eb57505061043e565b808201805167ffffffffffffffff811115610409575050505061043e565b80602083010160043d03850181111561042657505050505061043e565b61043582602001850186610380565b82955050505050505b90565b600081519050919050565b600082825260208201905092915050565b60005b8381101561047b578082015181840152602081019050610460565b60008484015250505050565b600061049282610441565b61049c818561044c565b93506104ac81856020860161045d565b6104b581610340565b840191505092915050565b600060208201905081810360008301526104da8184610487565b905092915050565b60008060233d11156104ff576020600460003e6001915060005190505b9091565b6000819050919050565b61051e61051982610299565b610503565b82525050565b6000610530828461050d565b60208201915081905092915050565b7f7472756500000000000000000000000000000000000000000000000000000000600082015250565b600061057560048361044c565b91506105808261053f565b602082019050919050565b600060208201905081810360008301526105a481610568565b905091905056fea26469706673582212201a522ad11a321603efd182e33e10b59f65b8c9a8b84c8ec3d832ff1d0b726cc564736f6c63430008120033" + std::string("000000000000000000000000") + contractAddress1.substr(2);
    Json::Value create2;
    create2["from"] = toJS( senderAddress );
    create2["data"] = bytecode2;
    create2["gas"] = "1800000";
    txHash = fixture.rpcClient->eth_sendTransaction( create2 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt2 = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt2["status"] == string( "0x1" ) );
    string contractAddress2 = receipt2["contractAddress"].asString();

    auto storageUsed = fixture.client->state().storageUsedTotal();

    Json::Value txStoreTry;
    txStoreTry["to"] = contractAddress2;
    txStoreTry["data"] = "0xc18829ca";
    txStoreTry["from"] = senderAddress.hex();
    txStoreTry["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStoreTry );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == storageUsed );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["gasUsed"] != "0x0" );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
}

BOOST_AUTO_TEST_CASE( setSchainExitTime ) {
    JsonRpcFixture fixture;
    Json::Value requestJson;
    requestJson["finishTime"] = 100;
    BOOST_REQUIRE_THROW(fixture.rpcClient->setSchainExitTime(requestJson), jsonrpc::JsonRpcException);
}

/*
BOOST_AUTO_TEST_CASE( oracle, *boost::unit_test::disabled() ) {

    JsonRpcFixture fixture;
    std::string receipt;
    std::string result;
    std::time_t current = std::time(nullptr);
    std::string request;
    for (int i = 0; i < 1000000; ++i) {
        request = skutils::tools::format("{\"cid\":1,\"uri\":\"http://worldtimeapi.org/api/timezone/Europe/Kiev\",\"jsps\":[\"/unixtime\",\"/day_of_year\",\"/xxx\"],\"trims\":[1,1,1],\"time\":%zu000,\"pow\":%zu}", current, i);
        auto os = make_shared<OracleRequestSpec>(request);
        if ( os->verifyPow() ) {
            break;
        }
    }
    uint64_t status = fixture.client->submitOracleRequest(request, receipt);

    BOOST_REQUIRE_EQUAL(status, 0);
    BOOST_CHECK(receipt != "");

    sleep(5);

    uint64_t resultStatus = fixture.client->checkOracleResult(receipt, result);
    BOOST_REQUIRE_EQUAL(resultStatus, 0);
    BOOST_CHECK(result != "");



}*/

BOOST_AUTO_TEST_CASE( doDbCompactionDebugCall ) {
    JsonRpcFixture fixture;

    fixture.rpcClient->debug_doStateDbCompaction();

    fixture.rpcClient->debug_doBlocksDbCompaction();
}

BOOST_AUTO_TEST_CASE( powTxnGasLimit ) {
    JsonRpcFixture fixture(c_genesisConfigString, false, false, true, false);

    // mine blocks without transactions
    dev::eth::simulateMining( *( fixture.client ), 2000000 );

    string senderAddress = toJS(fixture.coinbase.address());

    Json::Value txPOW1;
    txPOW1["to"] = "0x0000000000000000000000000000000000000033";
    txPOW1["from"] = senderAddress;
    txPOW1["gas"] = "100000";
    txPOW1["gasPrice"] = "0xa449dcaf2bca14e6bd0ac650eed9555008363002b2fc3a4c8422b7a9525a8135"; // gas 200k
    txPOW1["value"] = 1;
    string txHash = fixture.rpcClient->eth_sendTransaction( txPOW1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt1 = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt1["status"] == string( "0x1" ) );

    Json::Value txPOW2;
    txPOW2["to"] = "0x0000000000000000000000000000000000000033";
    txPOW2["from"] = senderAddress;
    txPOW2["gas"] = "100000";
    txPOW2["gasPrice"] = "0xc5002ab03e1e7e196b3d0ffa9801e783fcd48d4c6d972f1389ab63f4e2d0bef0"; // gas 1m
    txPOW2["value"] = 100;
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendTransaction( txPOW2 ), jsonrpc::JsonRpcException ); // block gas limit reached
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

BOOST_AUTO_TEST_CASE( eip2930Transactions ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 151
    std::string chainID = "0x97";
    ret["params"]["chainID"] = chainID;
    time_t eip1559PatchActivationTimestamp = time(nullptr) + 10;
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] = eip1559PatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS(fixture.coinbase.address());

    Json::Value txRefill;
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x0" );

    auto result = fixture.rpcClient->eth_getTransactionByHash( txHash );
    BOOST_REQUIRE( result["type"] == "0x0" );
    BOOST_REQUIRE( !result.isMember( "yParity" ) );
    BOOST_REQUIRE( !result.isMember( "accessList" ) );

    BOOST_REQUIRE( fixture.rpcClient->eth_getBalance( "0xc868AF52a6549c773082A334E5AE232e0Ea3B513", "latest" ) == "0x16345785d8a0000" );

    // try sending type1 txn before patchTimestmap
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180c001a01ebdc546c8b85511b7ba831f47c4981069d7af972d10b7dce2c57225cb5df6a7a055ae1e84fea41d37589eb740a0a93017a5cd0e9f10ee50f165bf4b1b4c78ddae" ), jsonrpc::JsonRpcException ); // INVALID_PARAMS
    sleep( 10 );

    // force 1 block to update timestamp
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 0;
    txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x0" );

    // send 1 WEI from 0xc868AF52a6549c773082A334E5AE232e0Ea3B513 to 0x7D36aF85A184E220A656525fcBb9A63B9ab3C12b
    // encoded type 1 txn
    txHash = fixture.rpcClient->eth_sendRawTransaction( "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180c001a01ebdc546c8b85511b7ba831f47c4981069d7af972d10b7dce2c57225cb5df6a7a055ae1e84fea41d37589eb740a0a93017a5cd0e9f10ee50f165bf4b1b4c78ddae" );
    auto pendingTransactions = fixture.rpcClient->eth_pendingTransactions();
    BOOST_REQUIRE( pendingTransactions.isArray() && pendingTransactions.size() == 1);
    BOOST_REQUIRE( pendingTransactions[0]["type"] == "0x1" );
    BOOST_REQUIRE( pendingTransactions[0].isMember( "yParity" ) && pendingTransactions[0].isMember( "accessList" ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // compare with txn hash from geth
    BOOST_REQUIRE( txHash == "0xc843560015a655b8f81f65a458be9019bdb5cd8e416b6329ca18f36de0b8244d" );

    BOOST_REQUIRE( dev::toHexPrefixed( fixture.client->transactions( 4 )[0].toBytes() ) == "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180c001a01ebdc546c8b85511b7ba831f47c4981069d7af972d10b7dce2c57225cb5df6a7a055ae1e84fea41d37589eb740a0a93017a5cd0e9f10ee50f165bf4b1b4c78ddae" );

    BOOST_REQUIRE( fixture.rpcClient->eth_getBalance( "0x7D36aF85A184E220A656525fcBb9A63B9ab3C12b", "latest" ) == "0x1" );

    auto block = fixture.rpcClient->eth_getBlockByNumber( "4", false );
    BOOST_REQUIRE( block["transactions"].size() == 1 );
    BOOST_REQUIRE( block["transactions"][0].asString() == txHash );

    block = fixture.rpcClient->eth_getBlockByNumber( "4", true );
    BOOST_REQUIRE( block["transactions"].size() == 1 );
    BOOST_REQUIRE( block["transactions"][0]["hash"].asString() == txHash );
    BOOST_REQUIRE( block["transactions"][0]["type"] == "0x1" );
    BOOST_REQUIRE( block["transactions"][0]["yParity"].asString() == block["transactions"][0]["v"].asString() );
    BOOST_REQUIRE( block["transactions"][0]["accessList"].isArray() );
    BOOST_REQUIRE( block["transactions"][0]["accessList"].size() == 0 );
    BOOST_REQUIRE( block["transactions"][0].isMember( "chainId" ) );
    BOOST_REQUIRE( block["transactions"][0]["chainId"].asString() == chainID );

    std::string blockHash = block["hash"].asString();
    BOOST_REQUIRE( fixture.client->transactionHashes( dev::h256( blockHash ) )[0] == dev::h256( "0xc843560015a655b8f81f65a458be9019bdb5cd8e416b6329ca18f36de0b8244d") );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x1" );
    BOOST_REQUIRE( receipt["effectiveGasPrice"] == "0x4a817c800" );

    result = fixture.rpcClient->eth_getTransactionByHash( txHash );
    BOOST_REQUIRE( result["hash"].asString() == txHash );
    BOOST_REQUIRE( result["type"] == "0x1" );
    BOOST_REQUIRE( result["yParity"].asString() == result["v"].asString() );
    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["accessList"].size() == 0 );

    result = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex( blockHash, "0x0" );
    BOOST_REQUIRE( result["hash"].asString() == txHash );
    BOOST_REQUIRE( result["type"] == "0x1" );
    BOOST_REQUIRE( result["yParity"].asString() == result["v"].asString() );
    BOOST_REQUIRE( result["accessList"].isArray() );

    result = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex( "0x4", "0x0" );
    BOOST_REQUIRE( result["hash"].asString() == txHash );
    BOOST_REQUIRE( result["type"] == "0x1" );
    BOOST_REQUIRE( result["yParity"].asString() == result["v"].asString() );
    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["accessList"].size() == 0 );

    // now the same txn with accessList and increased nonce
    // [ { 'address': HexBytes( "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae" ), 'storageKeys': ( "0x0000000000000000000000000000000000000000000000000000000000000003", "0x0000000000000000000000000000000000000000000000000000000000000007" ) } ]
    txHash = fixture.rpcClient->eth_sendRawTransaction( "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b03eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c07b6ff742311b04dab760bb3bc206054332879" );
    pendingTransactions = fixture.rpcClient->eth_pendingTransactions();
    BOOST_REQUIRE( pendingTransactions.isArray() && pendingTransactions.size() == 1);
    BOOST_REQUIRE( pendingTransactions[0]["type"] == "0x1" );
    BOOST_REQUIRE( pendingTransactions[0].isMember( "yParity" ) && pendingTransactions[0].isMember( "accessList" ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // compare with txn hash from geth
    BOOST_REQUIRE( txHash == "0xa6d3541e06dff71fb8344a4db2a4ad4e0b45024eb23a8f568982b70a5f50f94d" );
    BOOST_REQUIRE( dev::toHexPrefixed( fixture.client->transactions( 5 )[0].toBytes() ) == "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b03eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c07b6ff742311b04dab760bb3bc206054332879" );

    result = fixture.rpcClient->eth_getTransactionByHash( txHash );
    BOOST_REQUIRE( result["type"] == "0x1" );
    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["accessList"].size() == 1 );
    BOOST_REQUIRE( result["accessList"][0].isObject() && result["accessList"][0].getMemberNames().size() == 2 );
    BOOST_REQUIRE( result["accessList"][0].isMember( "address" ) && result["accessList"][0].isMember( "storageKeys" ) );
    BOOST_REQUIRE( result["accessList"][0]["address"].asString() == "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae" );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"].isArray() && result["accessList"][0]["storageKeys"].size() == 2 );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"][0].asString() == "0x0000000000000000000000000000000000000000000000000000000000000003" );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"][1].asString() == "0x0000000000000000000000000000000000000000000000000000000000000007" );

    block = fixture.rpcClient->eth_getBlockByNumber( "5", true );
    result = block["transactions"][0];
    BOOST_REQUIRE( result["type"] == "0x1" );
    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["accessList"].size() == 1 );
    BOOST_REQUIRE( result["accessList"][0].isObject() && result["accessList"][0].getMemberNames().size() == 2 );
    BOOST_REQUIRE( result["accessList"][0].isMember( "address" ) && result["accessList"][0].isMember( "storageKeys" ) );
    BOOST_REQUIRE( result["accessList"][0]["address"].asString() == "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae" );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"].isArray() && result["accessList"][0]["storageKeys"].size() == 2 );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"][0].asString() == "0x0000000000000000000000000000000000000000000000000000000000000003" );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"][1].asString() == "0x0000000000000000000000000000000000000000000000000000000000000007" );
}

BOOST_AUTO_TEST_CASE( eip1559Transactions ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 151
    std::string chainID = "0x97";
    ret["params"]["chainID"] = chainID;
    time_t eip1559PatchActivationTimestamp = time(nullptr) + 10;
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] = eip1559PatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS(fixture.coinbase.address());

    Json::Value txRefill;
    txRefill["to"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x0" );

    auto result = fixture.rpcClient->eth_getTransactionByHash( txHash );
    BOOST_REQUIRE( result["type"] == "0x0" );
    BOOST_REQUIRE( !result.isMember( "yParity" ) );
    BOOST_REQUIRE( !result.isMember( "accessList" ) );

    BOOST_REQUIRE( fixture.rpcClient->eth_getBalance( "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251", "latest" ) == "0x16345785d8a0000" );

    // try sending type2 txn before patchTimestmap
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( "0x02f8c98197808504a817c8008504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0f1a407dfc1a9f782001d89f617e9b3a2f295378533784fb39960dea60beea2d0a05ac3da2946554ba3d5721850f4f89ee7a0c38e4acab7130908e7904d13174388" ), jsonrpc::JsonRpcException ); // INVALID_PARAMS
    sleep( 10 );

    // force 1 block to update timestamp
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 0;
    txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x0" );
    BOOST_REQUIRE( receipt["effectiveGasPrice"] == "0x4a817c800" );

    // send 1 WEI from 0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251 to 0x7D36aF85A184E220A656525fcBb9A63B9ab3C12b
    // encoded type 2 txn
    txHash = fixture.rpcClient->eth_sendRawTransaction( "0x02f8c98197808504a817c8018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a0000000000000000000000000000000000000000000000000000000000000000701a005bd1eedc509a8e94cfcfc84d0b5fd53a0888a475274cbeee321047da5d139f8a00e7f0dd8b5277766d447ea51b7d8f571dc8bb57ff95c068c58f5b6fe9089dde8" );
    auto pendingTransactions = fixture.rpcClient->eth_pendingTransactions();
    BOOST_REQUIRE( pendingTransactions.isArray() && pendingTransactions.size() == 1);
    BOOST_REQUIRE( pendingTransactions[0]["type"] == "0x2" );
    BOOST_REQUIRE( pendingTransactions[0].isMember( "yParity" ) && pendingTransactions[0].isMember( "accessList" ) );
    BOOST_REQUIRE( pendingTransactions[0].isMember( "maxFeePerGas" ) && pendingTransactions[0].isMember( "maxPriorityFeePerGas" ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // compare with txn hash from geth
    BOOST_REQUIRE( txHash == "0xde30b1c26b89e20f6426a87b9427381f9e79e2bb80f992a6f2e1b4dccfa345de" );
    BOOST_REQUIRE( dev::toHexPrefixed( fixture.client->transactions( 4 )[0].toBytes() ) == "0x02f8c98197808504a817c8018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a0000000000000000000000000000000000000000000000000000000000000000701a005bd1eedc509a8e94cfcfc84d0b5fd53a0888a475274cbeee321047da5d139f8a00e7f0dd8b5277766d447ea51b7d8f571dc8bb57ff95c068c58f5b6fe9089dde8" );

    BOOST_REQUIRE( fixture.rpcClient->eth_getBalance( "0x7D36aF85A184E220A656525fcBb9A63B9ab3C12b", "latest" ) == "0x1" );

    auto block = fixture.rpcClient->eth_getBlockByNumber( "4", false );
    BOOST_REQUIRE( block["transactions"].size() == 1 );
    BOOST_REQUIRE( block["transactions"][0].asString() == txHash );

    block = fixture.rpcClient->eth_getBlockByNumber( "4", true );
    BOOST_REQUIRE( !block["baseFeePerGas"].asString().empty() );
    BOOST_REQUIRE( block["transactions"].size() == 1 );
    BOOST_REQUIRE( block["transactions"][0]["hash"].asString() == txHash );
    BOOST_REQUIRE( block["transactions"][0]["type"] == "0x2" );
    BOOST_REQUIRE( block["transactions"][0]["yParity"].asString() == block["transactions"][0]["v"].asString() );
    BOOST_REQUIRE( block["transactions"][0]["accessList"].isArray() );
    BOOST_REQUIRE( block["transactions"][0].isMember( "chainId" ) );
    BOOST_REQUIRE( block["transactions"][0]["chainId"].asString() == chainID );

    std::string blockHash = block["hash"].asString();

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x2" );
    BOOST_REQUIRE( receipt["effectiveGasPrice"] == "0x4a817c800" );

    result = fixture.rpcClient->eth_getTransactionByHash( txHash );
    BOOST_REQUIRE( result["hash"].asString() == txHash );
    BOOST_REQUIRE( result["type"] == "0x2" );
    BOOST_REQUIRE( result["yParity"].asString() == result["v"].asString() );
    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result.isMember( "maxPriorityFeePerGas" ) && result["maxPriorityFeePerGas"].isString() );
    BOOST_REQUIRE( result.isMember( "maxFeePerGas" ) && result["maxFeePerGas"].isString() );
    BOOST_REQUIRE( result["maxPriorityFeePerGas"] == "0x4a817c801" );
    BOOST_REQUIRE( result["maxFeePerGas"] == "0x4a817c800" );

    result = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex( blockHash, "0x0" );
    BOOST_REQUIRE( result["hash"].asString() == txHash );
    BOOST_REQUIRE( result["type"] == "0x2" );
    BOOST_REQUIRE( result["yParity"].asString() == result["v"].asString() );
    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["maxPriorityFeePerGas"] == "0x4a817c801" );
    BOOST_REQUIRE( result["maxFeePerGas"] == "0x4a817c800" );

    result = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex( "0x4", "0x0" );
    BOOST_REQUIRE( result["hash"].asString() == txHash );
    BOOST_REQUIRE( result["type"] == "0x2" );
    BOOST_REQUIRE( result["yParity"].asString() == result["v"].asString() );
    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["maxPriorityFeePerGas"] == "0x4a817c801" );
    BOOST_REQUIRE( result["maxFeePerGas"] == "0x4a817c800" );

    BOOST_REQUIRE_NO_THROW( fixture.rpcClient->eth_getBlockByNumber( "0x0", false ) );
}

BOOST_AUTO_TEST_CASE( eip2930RpcMethods ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 151
    ret["params"]["chainID"] = "0x97";

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS(fixture.coinbase.address());

    Json::Value txRefill;
    txRefill["to"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 1000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x0" );

    auto accessList = fixture.rpcClient->eth_createAccessList( txRefill, "latest" );
    BOOST_REQUIRE( accessList.isMember( "accessList" ) && accessList.isMember( "gasUsed" ) );
    BOOST_REQUIRE( accessList["accessList"].isArray() && accessList["accessList"].size() == 0 );
    BOOST_REQUIRE( accessList["gasUsed"].isString() );
}

BOOST_AUTO_TEST_CASE( eip1559RpcMethods ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 151
    ret["params"]["chainID"] = "0x97";
    time_t eip1559PatchActivationTimestamp = time(nullptr) + 5;
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] = eip1559PatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS(fixture.coinbase.address());

    Json::Value txRefill;
    txRefill["to"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    for (size_t i = 0; i < 10; ++i) {
        // mine 10 blocks
        string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
        dev::eth::mineTransaction( *( fixture.client ), 1 );
    }

    BOOST_REQUIRE( fixture.rpcClient->eth_maxPriorityFeePerGas() == "0x0" );

    auto bn = fixture.client->number();

    Json::Value percentiles = Json::Value( Json::arrayValue );
    percentiles.resize( 2 );
    percentiles[0] = 20;
    percentiles[1] = 80;

    size_t blockCnt = 9;
    auto feeHistory = fixture.rpcClient->eth_feeHistory( toJS( blockCnt ), "latest", percentiles );

    BOOST_REQUIRE( feeHistory["oldestBlock"] == toJS( bn - blockCnt + 1 ) );

    BOOST_REQUIRE( feeHistory.isMember( "baseFeePerGas" ) );
    BOOST_REQUIRE( feeHistory["baseFeePerGas"].isArray() );

    for (Json::Value::ArrayIndex i = 0; i < blockCnt; ++i) {
        BOOST_REQUIRE( feeHistory["baseFeePerGas"][i].isString() );
        std::string estimatedBaseFeePerGas = EIP1559TransactionsPatch::isEnabledWhen(
                    fixture.client->blockInfo( bn - i - 1 ).timestamp() ) ? toJS( fixture.client->gasBidPrice( bn - i - 1 ) ) : toJS( 0 );
        BOOST_REQUIRE( feeHistory["baseFeePerGas"][i].asString() == estimatedBaseFeePerGas );
        BOOST_REQUIRE_GT( feeHistory["gasUsedRatio"][i].asDouble(), 0 );
        BOOST_REQUIRE_GT( 1, feeHistory["gasUsedRatio"][i].asDouble() );
        for ( Json::Value::ArrayIndex j = 0; j < percentiles.size(); ++j ) {
            BOOST_REQUIRE_EQUAL( feeHistory["reward"][i][j].asString(), toJS( 0 ) );
        }
    }

    BOOST_REQUIRE_NO_THROW( fixture.rpcClient->eth_feeHistory( blockCnt, "latest", percentiles ) );
}

BOOST_AUTO_TEST_CASE( vInTxnSignature ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 151
    ret["params"]["chainID"] = "0x97";
    time_t eip1559PatchActivationTimestamp = time(nullptr);
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] = eip1559PatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS(fixture.coinbase.address());

    Json::Value txRefill;
    txRefill["to"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // send non replay protected txn
    txHash = fixture.rpcClient->eth_sendRawTransaction( "0xf864808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b01801ba0171c7f31feaa0fd7825a5a28d7b535d0b0ee200b27792f66eb7796e7a6a555d7a0081790244f21cefa563b55a7a68ee78f8466738b5827be19faaeff0586fd71be" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value txn = fixture.rpcClient->eth_getTransactionByHash( txHash );
    dev::u256 v = dev::jsToU256( txn["v"].asString() );
    BOOST_REQUIRE( v < 29 && v > 26 );

    // send replay protected legacy txn
    txHash = fixture.rpcClient->eth_sendRawTransaction( "0xf866018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180820151a018b400fc56bc3568e4f23f6f93d538745a5b18054252d6030791c294c9aea9d4a00930492125784fad0a8b38b915e8621f54c53f0878a77f21920c751ec5fd220a" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txn = fixture.rpcClient->eth_getTransactionByHash( txHash );
    v = dev::jsToU256( txn["v"].asString() );
    BOOST_REQUIRE( v < 339 && v > 336 ); // 2 * 151 + 35

    // send type1 txn
    txHash = fixture.rpcClient->eth_sendRawTransaction( "0x01f8c38197028504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a0000000000000000000000000000000000000000000000000000000000000000701a0ee608b7c5df843b4a1988a3e9c24d53019fa674e06a6b2ae0c347a00601c1a84a06ed451f9cc0f4334a180458605ecaa212e58f8436e1a4318e75ae417c72eba2b" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txn = fixture.rpcClient->eth_getTransactionByHash( txHash );
    v = dev::jsToU256( txn["v"].asString() );
    BOOST_REQUIRE( v < 2 && v >= 0 );

    // send type2 txn
    txHash = fixture.rpcClient->eth_sendRawTransaction( "0x02f8c98197038504a817c8018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000000000000000000003a0000000000000000000000000000000000000000000000000000000000000000701a0c16ec291a6f4e91476f39e624baf42730b21a805e570fe52334df13d69b63d3fa01c7e9662635512a3bc47d479b17af2df59491e6663823ca13789a86da6dff1a5" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txn = fixture.rpcClient->eth_getTransactionByHash( txHash );
    v = dev::jsToU256( txn["v"].asString() );
    BOOST_REQUIRE( v < 2 && v >= 0 );
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

BOOST_AUTO_TEST_CASE( deployment_control_v2 ) {

    // Inserting ConfigController mockup into config and enabling flexibleDeploymentPatch.
    // ConfigController mockup contract:

    // pragma solidity ^0.8.9;
    // contract ConfigController {
    //     bool public freeContractDeployment = false;
    //     function isAddressWhitelisted(address addr) external view returns (bool) {
    //         return false;
    //     }
    //     function isDeploymentAllowed(address origin, address sender)
    //         external view returns (bool) {
    //         return freeContractDeployment;
    //     }
    //     function setFreeContractDeployment() external {
    //         freeContractDeployment = true;
    //     }
    // }

    string configControllerV2 =
            "0x608060405234801561001057600080fd5b506004361061004c576000"
            "3560e01c806313f44d1014610051578063a2306c4f14610081578063d0"
            "f557f41461009f578063f7e2a91b146100cf575b600080fd5b61006b60"
            "048036038101906100669190610189565b6100d9565b60405161007891"
            "906101d1565b60405180910390f35b6100896100e0565b604051610096"
            "91906101d1565b60405180910390f35b6100b960048036038101906100"
            "b491906101ec565b6100f1565b6040516100c691906101d1565b604051"
            "80910390f35b6100d761010a565b005b6000919050565b600080549061"
            "01000a900460ff1681565b60008060009054906101000a900460ff1690"
            "5092915050565b60016000806101000a81548160ff0219169083151502"
            "17905550565b600080fd5b600073ffffffffffffffffffffffffffffff"
            "ffffffffff82169050919050565b60006101568261012b565b90509190"
            "50565b6101668161014b565b811461017157600080fd5b50565b600081"
            "3590506101838161015d565b92915050565b6000602082840312156101"
            "9f5761019e610126565b5b60006101ad84828501610174565b91505092"
            "915050565b60008115159050919050565b6101cb816101b6565b825250"
            "50565b60006020820190506101e660008301846101c2565b9291505056"
            "5b6000806040838503121561020357610202610126565b5b6000610211"
            "85828601610174565b925050602061022285828601610174565b915050"
            "925092905056fea2646970667358221220b5f971b16f7bbba22272b220"
            "7e02f10abf1682c17fe636c7bf6406c5cae5716064736f6c63430008090033";

    std::string _config = c_genesisGeneration2ConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    ret["accounts"]["0xD2002000000000000000000000000000000000d2"]["code"] = configControllerV2;
    ret["skaleConfig"]["sChain"]["flexibleDeploymentPatchTimestamp"] = 1;
    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );

    JsonRpcFixture fixture(config, false, false, true );
    Address senderAddress = fixture.coinbase.address();
    fixture.client->setAuthor( senderAddress );

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


    // Trying to deploy contract without permission
    Json::Value deployContractWithoutRoleTx;
    deployContractWithoutRoleTx["from"] = senderAddress.hex();
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

    // Allow to deploy by calling setFreeContractDeployment()
    Json::Value grantDeployerRoleTx;
    grantDeployerRoleTx["data"] = "0xf7e2a91b";
    grantDeployerRoleTx["from"] = senderAddress.hex();
    grantDeployerRoleTx["to"] = "0xD2002000000000000000000000000000000000D2";
    grantDeployerRoleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    grantDeployerRoleTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( grantDeployerRoleTx );
    BOOST_REQUIRE( !txHash.empty() );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // Deploying with permission
    Json::Value deployContractTx;
    deployContractTx["from"] = senderAddress.hex();
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

    auto txHash = fixture.rpcClient->eth_sendRawTransaction( toJS( tx.toBytes() ) );
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

    Json::Value call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 1);

    h256 h2 = fixture.client->importTransaction( tx3 );
    BOOST_REQUIRE( h2 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 2);

    call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 2);
    BOOST_REQUIRE_EQUAL( call[0]["from"], string("0x")+txJson["from"].asString() );

    h256 h3 = fixture.client->importTransaction( tx2 );
    BOOST_REQUIRE( h3 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 3);

    call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 3);

    h256 h4 = fixture.client->importTransaction( tx1 );
    BOOST_REQUIRE( h4 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 1);
    BOOST_REQUIRE_EQUAL( tq->status().current, 3);

    call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 1);

    h256 h5 = fixture.client->importTransaction( tx4 );
    BOOST_REQUIRE( h5 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 0);
    BOOST_REQUIRE_EQUAL( tq->status().current, 5);

    call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 0);

    fixture.client->skaleHost()->pauseConsensus( false );
}

// TODO: Enable for multitransaction mode checking

// historic node shall ignore invalid transactions in block
BOOST_AUTO_TEST_CASE( skip_invalid_transactions ) {
    JsonRpcFixture fixture( c_genesisConfigString, true, true, false, true );
    dev::eth::simulateMining( *( fixture.client ), 1 ); // 2 Ether

    cout << "Balance: " << fixture.rpcClient->eth_getBalance(fixture.accountHolder->allAccounts()[0].hex(), "latest") << endl;

    // 1 import 1 transaction to increase block number
    // also send 1 eth to account2
    // TODO repair mineMoney function! (it asserts)
    Json::Value txJson;
    txJson["from"] = fixture.coinbase.address().hex();
    txJson["gas"] = "200000";
    txJson["gasPrice"] = "5000000000000";
    txJson["to"] = fixture.account2.address().hex();
    txJson["value"] = "1000000000000000000";

    txJson["nonce"] = "0";
    TransactionSkeleton ts1 = toTransactionSkeleton( txJson );
    ts1 = fixture.client->populateTransactionWithDefaults( ts1 );
    pair< bool, Secret > ar1 = fixture.accountHolder->authenticate( ts1 );
    Transaction tx1( ts1, ar1.second );
    fixture.client->importTransaction( tx1 );

    // 1 eth left (returned to author)
    dev::eth::mineTransaction(*(fixture.client), 1);
    cout << "Balance2: " << fixture.rpcClient->eth_getBalance(fixture.accountHolder->allAccounts()[0].hex(), "latest") << endl;

    // 2 import 4 transactions with money for 1st, 2nd, and 3rd

    // require full 1 Ether for gas+value
    txJson["gas"] = "100000";
    txJson["nonce"] = "1";
    txJson["value"] = "500000000000000000";// take 0.5 eth out
    ts1 = toTransactionSkeleton( txJson );
    ts1 = fixture.client->populateTransactionWithDefaults( ts1 );
    ar1 = fixture.accountHolder->authenticate( ts1 );
    tx1 = Transaction( ts1, ar1.second );

    txJson["nonce"] = "2";
    TransactionSkeleton ts2 = toTransactionSkeleton( txJson );
    ts2 = fixture.client->populateTransactionWithDefaults( ts2 );
    pair< bool, Secret > ar2 = fixture.accountHolder->authenticate( ts2 );
    Transaction tx2( ts2, ar2.second );

    txJson["from"] = fixture.account2.address().hex();
    txJson["nonce"] = "0";
    txJson["value"] = "0";
    txJson["gasPrice"] = "20000000000";
    txJson["gas"] = "53000";
    TransactionSkeleton ts3 = toTransactionSkeleton( txJson );
    ts3 = fixture.client->populateTransactionWithDefaults( ts3 );
    pair< bool, Secret > ar3 = fixture.accountHolder->authenticate( ts3 );
    Transaction tx3( ts3, ar3.second );

    txJson["nonce"] = "1";
    TransactionSkeleton ts4 = toTransactionSkeleton( txJson );
    ts3 = fixture.client->populateTransactionWithDefaults( ts4 );
    pair< bool, Secret > ar4 = fixture.accountHolder->authenticate( ts4 );
    Transaction tx4( ts3, ar3.second );

    h256 h4 = fixture.client->importTransaction( tx4 ); // ok
    h256 h2 = fixture.client->importTransaction( tx2 ); // invalid
    h256 h3 = fixture.client->importTransaction( tx3 ); // ok
    h256 h1 = fixture.client->importTransaction( tx1 ); // ok

    dev::eth::mineTransaction(*(fixture.client), 1);
    cout << "Balance3: " << fixture.rpcClient->eth_getBalance(fixture.accountHolder->allAccounts()[0].hex(), "latest") << endl;

    (void)h1;
    (void)h2;
    (void)h3;
    (void)h4;

#ifdef HISTORIC_STATE
    // 3 check that historic node sees only 3 txns

    string explicitNumberStr = to_string(fixture.client->number());

    // 1 Block
    Json::Value block = fixture.rpcClient->eth_getBlockByNumber("latest", "false");

    string bh = block["hash"].asString();

    // 2 transaction count
    Json::Value cnt = fixture.rpcClient->eth_getBlockTransactionCountByNumber("latest");
    BOOST_REQUIRE_EQUAL(cnt.asString(), "0x3");
    cnt = fixture.rpcClient->eth_getBlockTransactionCountByNumber(explicitNumberStr);
    BOOST_REQUIRE_EQUAL(cnt.asString(), "0x3");
    cnt = fixture.rpcClient->eth_getBlockTransactionCountByHash(bh);
    BOOST_REQUIRE_EQUAL(cnt.asString(), "0x3");


    BOOST_REQUIRE_EQUAL(block["transactions"].size(), 3);
    BOOST_REQUIRE_EQUAL(block["transactions"][0]["transactionIndex"], "0x0");
    BOOST_REQUIRE_EQUAL(block["transactions"][1]["transactionIndex"], "0x1");
    BOOST_REQUIRE_EQUAL(block["transactions"][2]["transactionIndex"], "0x2");

    // same with explicit number
    block = fixture.rpcClient->eth_getBlockByNumber(explicitNumberStr, "false");

    BOOST_REQUIRE_EQUAL(block["transactions"].size(), 3);
    BOOST_REQUIRE_EQUAL(block["transactions"][0]["transactionIndex"], "0x0");
    BOOST_REQUIRE_EQUAL(block["transactions"][1]["transactionIndex"], "0x1");
    BOOST_REQUIRE_EQUAL(block["transactions"][2]["transactionIndex"], "0x2");

    // 3 receipts
    Json::Value r1,r3,r4;
    BOOST_REQUIRE_NO_THROW(r1 = fixture.rpcClient->eth_getTransactionReceipt(toJS(h1)));
    BOOST_REQUIRE_THROW   (fixture.rpcClient->eth_getTransactionReceipt(toJS(h2)), jsonrpc::JsonRpcException);
    BOOST_REQUIRE_NO_THROW(r3 = fixture.rpcClient->eth_getTransactionReceipt(toJS(h3)));
    BOOST_REQUIRE_NO_THROW(r4 = fixture.rpcClient->eth_getTransactionReceipt(toJS(h4)));

    BOOST_REQUIRE_EQUAL(r1["transactionIndex"], "0x0");
    BOOST_REQUIRE_EQUAL(r3["transactionIndex"], "0x1");
    BOOST_REQUIRE_EQUAL(r4["transactionIndex"], "0x2");

    // 4 transaction by index
    Json::Value t0 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex("latest", "0");
    Json::Value t1 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex("latest", "1");
    Json::Value t2 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex("latest", "2");
    BOOST_REQUIRE_EQUAL(jsToFixed<32>(t0["hash"].asString()), h1);
    BOOST_REQUIRE_EQUAL(jsToFixed<32>(t1["hash"].asString()), h3);
    BOOST_REQUIRE_EQUAL(jsToFixed<32>(t2["hash"].asString()), h4);

    // same with explicit block number

    t0 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex(explicitNumberStr, "0");
    t1 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex(explicitNumberStr, "1");
    t2 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex(explicitNumberStr, "2");
    BOOST_REQUIRE_EQUAL(jsToFixed<32>(t0["hash"].asString()), h1);
    BOOST_REQUIRE_EQUAL(jsToFixed<32>(t1["hash"].asString()), h3);
    BOOST_REQUIRE_EQUAL(jsToFixed<32>(t2["hash"].asString()), h4);

    BOOST_REQUIRE_EQUAL(bh, r1["blockHash"].asString());

    t0 = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex(bh, "0");
    t1 = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex(bh, "1");
    t2 = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex(bh, "2");

    BOOST_REQUIRE_EQUAL(jsToFixed<32>(t0["hash"].asString()), h1);
    BOOST_REQUIRE_EQUAL(jsToFixed<32>(t1["hash"].asString()), h3);
    BOOST_REQUIRE_EQUAL(jsToFixed<32>(t2["hash"].asString()), h4);

    // 5 transaction by hash
    BOOST_REQUIRE_THROW   (fixture.rpcClient->eth_getTransactionByHash(toJS(h2)), jsonrpc::JsonRpcException);

    // send it successfully

    // make money
    dev::eth::simulateMining( *fixture.client, 1);

    h2 = fixture.client->importTransaction( tx2 ); // invalid

    dev::eth::mineTransaction(*(fixture.client), 1);

    // checks:
    Json::Value r2;
    BOOST_REQUIRE_NO_THROW(r2 = fixture.rpcClient->eth_getTransactionReceipt(toJS(h2)));
    BOOST_REQUIRE_EQUAL(r2["blockNumber"], toJS(fixture.client->number()));
#endif
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

    auto txHash = rpcClient->eth_sendRawTransaction( toJS( tx.toBytes() ) );
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

    auto txHash = rpcClient->eth_sendRawTransaction( toJS( tx.toBytes() ) );
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

    auto txHash = rpcClient->eth_sendRawTransaction( toJS( tx.toBytes() ) );
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

BOOST_AUTO_TEST_SUITE( FilestorageCacheSuite )

BOOST_AUTO_TEST_CASE( cached_filestorage ) {

    auto _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    ret["skaleConfig"]["sChain"]["revertableFSPatchTimestamp"] = 1;
    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    RestrictedAddressFixture fixture( config );

    auto senderAddress = fixture.coinbase.address();
    fixture.client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( fixture.client ), 1000 );

    Json::Value transactionCallObject;
    transactionCallObject["from"] = toJS( senderAddress );
    transactionCallObject["to"] = "0x692a70d2e424a56d2c6c27aa97d1a86395877b3a";
    transactionCallObject["data"] = "0xf38fb65b";

    TransactionSkeleton ts = toTransactionSkeleton( transactionCallObject );
    ts = fixture.client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = fixture.accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    auto txHash = fixture.rpcClient->eth_sendRawTransaction( toJS( tx.toBytes() ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    BOOST_REQUIRE( !boost::filesystem::exists( fixture.path ) );
}

BOOST_AUTO_TEST_CASE( uncached_filestorage ) {

    auto _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    ret["skaleConfig"]["sChain"]["revertableFSPatchTimestamp"] = 9999999999999;
    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    RestrictedAddressFixture fixture( config );

    auto senderAddress = fixture.coinbase.address();
    fixture.client->setAuthor( senderAddress );
    dev::eth::simulateMining( *( fixture.client ), 1000 );

    Json::Value transactionCallObject;
    transactionCallObject["from"] = toJS( senderAddress );
    transactionCallObject["to"] = "0x692a70d2e424a56d2c6c27aa97d1a86395877b3a";
    transactionCallObject["data"] = "0xf38fb65b";

    TransactionSkeleton ts = toTransactionSkeleton( transactionCallObject );
    ts = fixture.client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = fixture.accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    auto txHash = fixture.rpcClient->eth_sendRawTransaction( toJS( tx.toBytes() ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    BOOST_REQUIRE( boost::filesystem::exists( fixture.path ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( GappedCacheSuite, JsonRpcFixture )

#ifdef HISTORIC_STATE

BOOST_AUTO_TEST_CASE( test_blocks ) {
    dev::rpc::_detail::GappedTransactionIndexCache cache(10, *client);
    BOOST_REQUIRE_EQUAL(cache.realBlockTransactionCount(LatestBlock), 0);
    BOOST_REQUIRE_EQUAL(cache.realBlockTransactionCount(PendingBlock), 0);
    BOOST_REQUIRE_EQUAL(cache.realBlockTransactionCount(999999999), 0);
}

BOOST_AUTO_TEST_CASE( test_transactions ) {

    simulateMining(*client, 1, Address("0xf6c2a4ba2350e58a45916a03d0faa70dcc5dcfbf"));

    dev::rpc::_detail::GappedTransactionIndexCache cache(10, *client);

    Transaction invalid(
        fromHex("0x0011223344556677889900112233445566778899001122334455667788990011223344556677889900112233"
                "445566778899001122334455667788990011223344556677889900112233445566778899001122334455667788"
                "990011223344556677889900112233445566778899" ),
        CheckTransaction::None, true );

    Transaction valid(
        fromHex( "0xf86c808504a817c80083015f90943d7112ee86223baf0a506b9d2a77595cbbba51d1872386f26fc10000801ca0655757fd0650a65a373c48a4dc0f3d6ac5c3831aa0cc2cb863a5909dc6c25f72a071882ee8633466a243c0ea64dadb3120c1ca7a5cc7433c6c0b1c861a85322265" ),
        CheckTransaction::None );
    valid.ignoreExternalGas();

    client->importTransactionsAsBlock(Transactions{invalid, valid}, 1);

    BOOST_REQUIRE_EQUAL(cache.realBlockTransactionCount(LatestBlock), 2);
    BOOST_REQUIRE_EQUAL(cache.gappedBlockTransactionCount(LatestBlock), 1);
    BOOST_REQUIRE_EQUAL(cache.realIndexFromGapped(LatestBlock, 0), 1);
    BOOST_REQUIRE_EQUAL(cache.gappedIndexFromReal(LatestBlock, 1), 0);
    BOOST_REQUIRE_THROW(cache.gappedIndexFromReal(LatestBlock, 0), std::out_of_range);
    BOOST_REQUIRE_EQUAL(cache.transactionPresent(LatestBlock, 0), false);
    BOOST_REQUIRE_EQUAL(cache.transactionPresent(LatestBlock, 1), true);
}

BOOST_AUTO_TEST_CASE( test_exceptions ) {

    simulateMining(*client, 1, Address("0xf6c2a4ba2350e58a45916a03d0faa70dcc5dcfbf"));

    dev::rpc::_detail::GappedTransactionIndexCache cache(10, *client);

    Transaction invalid(
        fromHex("0x0011223344556677889900112233445566778899001122334455667788990011223344556677889900112233"
                "445566778899001122334455667788990011223344556677889900112233445566778899001122334455667788"
                "990011223344556677889900112233445566778899" ),
        CheckTransaction::None, true );

    Transaction valid(
        fromHex( "0xf86c808504a817c80083015f90943d7112ee86223baf0a506b9d2a77595cbbba51d1872386f26fc10000801ca0655757fd0650a65a373c48a4dc0f3d6ac5c3831aa0cc2cb863a5909dc6c25f72a071882ee8633466a243c0ea64dadb3120c1ca7a5cc7433c6c0b1c861a85322265" ),
        CheckTransaction::None );
    valid.ignoreExternalGas();

    client->importTransactionsAsBlock(Transactions{invalid, valid}, 1);

    BOOST_REQUIRE_THROW(cache.realIndexFromGapped(LatestBlock, 1), std::out_of_range);
    BOOST_REQUIRE_THROW(cache.realIndexFromGapped(LatestBlock, 2), std::out_of_range);
    BOOST_REQUIRE_THROW(cache.gappedIndexFromReal(LatestBlock, 2), std::out_of_range);
    BOOST_REQUIRE_THROW(cache.gappedIndexFromReal(LatestBlock, 0), std::out_of_range);
    BOOST_REQUIRE_THROW(cache.transactionPresent(LatestBlock, 2), std::out_of_range);
}

#endif

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
