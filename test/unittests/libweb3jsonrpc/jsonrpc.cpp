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

#include <limits>

#include "WebThreeStubClient.h"


#include "SkaledFixture.h"
#include "genesisGeneration2Config.h"
#include "libweb3jsonrpc/SkaleFace.h"
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/abstractserverconnector.h>
#include <libconsensus/SkaleCommon.h>
#include <libconsensus/oracle/OracleRequestSpec.h>
#include <libskale/OverlayDB.h>
#ifndef FAIR
#include <libskale/OverlayFS.h>
#endif
#include <libdevcore/CommonIO.h>
#include <libdevcore/TransientDirectory.h>
#include <libethcore/CommonJS.h>
#include <libethcore/KeyManager.h>
#include <libethereum/ChainParams.h>
#include <libethereum/ClientTest.h>
#include <libethereum/SchainPatch.h>
#include <libskale/httpserveroverride.h>
#include <libskutils/include/skutils/rest_call.h>
#include <libweb3jsonrpc/AccountHolder.h>
#include <libweb3jsonrpc/AdminEth.h>

#include <libweb3jsonrpc/JsonHelper.h>
#include "SkaledFixture.h"
#include <libconsensus/SkaleCommon.h>
#include <libconsensus/node/ConsensusInterface.h>

#ifndef FAIR
#include <libconsensus/oracle/OracleRequestSpec.h>
#endif

#include "genesisGeneration2Config.h"

#include <libweb3jsonrpc/Debug.h>
#include <libweb3jsonrpc/Eth.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include <libweb3jsonrpc/ModularServer.h>
#include <libweb3jsonrpc/Net.h>
#include <libweb3jsonrpc/Skale.h>
#include <libweb3jsonrpc/Test.h>
#include <libweb3jsonrpc/Web3.h>
#include <libweb3jsonrpc/rapidjson_handlers.h>
#include <test/tools/libtesteth/TestHelper.h>
#include <test/tools/libtesteth/TestOutputHelper.h>
#include <boost/process.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdlib>
#include <thread>

#ifdef BITE
#include <libconsensus/libBLS/threshold_encryption/ThresholdEncryption.h>
#endif

#ifdef BITE2
#include <libethereum/BITEConstants.h>
#endif

#ifdef FAIR
#include <libskale/BlockRewardsActivationPatch.h>
#endif

// This is defined by some weird windows header - workaround for now.
#undef GetMessage


using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::test;

static size_t rand_port = ( srand( time( nullptr ) ), 1024 + rand() % 64000 );

#ifndef FAIR
static std::string const c_genesisConfigString = R"(
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
            "basePort": )" +
    std::to_string( rand_port ) + R"(,
            "logLevel": "trace",
            "logLevelProposal": "trace",
            "testSignatures": true
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "emptyBlockIntervalMs": -1,
            "nodeGroups": {},
            "nodes": [
                { "nodeID": 1112, "owner": "0x0E7d7F1D34a502bD609542576941C3FCc087c588", "ip": "127.0.0.1", "basePort": )" +
    std::to_string( rand_port ) +
    R"(, "schainIndex" : 1, "publicKey": "0xfa"}
            ]
        }
    },
    "accounts": {
        "0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },)" +
        R"( "0000000000000000000000000000000000000005": {
            "precompiled": {
                "name": "createFile",
                "linear": {
                    "base": 15,
                    "word": 0
                },
                "restrictAccess": ["00000000000000000000000000000000000000AA", "692a70d2e424a56d2c6c27aa97d1a86395877b3a"]
            }
        },)" +

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
        "0000000000000000000000000000000000000008": { "precompiled": { "name": "getBlockRandom", "linear": { "base": 15, "word": 0 } } },
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
#else
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
            "basePort": )" +
    std::to_string( rand_port ) + R"(,
            "logLevel": "trace",
            "logLevelProposal": "trace",
            "testSignatures": true
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "emptyBlockIntervalMs": -1,
            "nodeGroups": {},
            "nodes": {
                "1": {
                    "stakingContractAddress": "0x5C60C315985977b7a408eBF4256984Acdf949549",
                    "group": [
                  { "nodeID": 1112, "owner": "0x0E7d7F1D34a502bD609542576941C3FCc087c588", "ip": "127.0.0.1", "basePort": )" +
        std::to_string( rand_port ) +
        R"(, "ip6": "::1", "basePort6": 1231, "schainIndex" : 1, "publicKey" : "0xfa"}
                    ]
                },
                "-1": {}
            }
        }
    },
    "accounts": {
        "0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
        "0000000000000000000000000000000000000008": { "precompiled": { "name": "getBlockRandom", "linear": { "base": 15, "word": 0 } } },
        )"
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
                }
            }
        },
        "0x5c4e11842e8be09264dc1976943571d7af6d00f9" : {
            "balance" : "1000000000000000000000000000000"
        },
        "0x5339Ef05428d1b87f4e2F2db64E782c68E9cDA56": {
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
#endif

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
    explicit TestIpcClient( TestIpcServer& _server ) : m_server{ _server } {}

    void SendRPCMessage( const std::string& _message, std::string& _result ) override {
        m_server.ProcessRequest( _message, _result );
    }

private:
    TestIpcServer& m_server;
};

struct JsonRpcFixture : public TestOutputHelperFixture {
    // chain params needs to be a field of JsonRPCFixture
    // since references to it are passed to the server
    std::shared_ptr< ChainParams > chainParams = std::make_shared< ChainParams >();


    JsonRpcFixture( const std::string& _config = "", bool _owner = true,
        bool _deploymentControl = true, bool _generation2 = false, bool _mtmEnabled = false,
        bool _isSyncNode = false, int _emptyBlockIntervalMs = -1,
        const std::map< std::string, std::string >& params =
            std::map< std::string, std::string >() ) {

        // this fixture is used in al tests to load config. So also init bls library as well
        libBLS::init();

        if ( _config != "" ) {
            if ( !_generation2 ) {
                Json::Value ret;
                Json::Reader().parse( _config, ret );
                if ( _owner ) {
                    ret["skaleConfig"]["sChain"]["schainOwner"] = toJS( coinbase.address() );
                    if ( _deploymentControl )
                        ret["accounts"]["0xD2002000000000000000000000000000000000D2"]["storage"]
                           ["0x0"] = toJS( coinbase.address() );
                } else {
                    ret["skaleConfig"]["sChain"]["schainOwner"] = toJS( account2.address() );
                    if ( _deploymentControl )
                        ret["accounts"]["0xD2002000000000000000000000000000000000D2"]["storage"]
                           ["0x0"] = toJS( account2.address() );
                }
#ifndef FAIR
                ret["skaleConfig"]["sChain"]["contractStorageLimit"] = 128;
#endif
                if ( params.count( "contractStorageLimit" ) )
                    ret["skaleConfig"]["sChain"]["contractStorageLimit"] = std::stoi( params.at( "contractStorageLimit" ) );

                Json::FastWriter fastWriter;
                std::string output = fastWriter.write( ret );
                chainParams->loadConfig( output );
            } else {
                Json::Value ret;
                Json::FastWriter fastWriter;
                Json::Reader().parse( _config, ret );
#ifndef FAIR
                ret["skaleConfig"]["sChain"]["contractStorageLimit"] = 106874910;
                ret["skaleConfig"]["sChain"]["contractStoragePatchTimestamp"] = 1000;
#endif
                std::string output = fastWriter.write( ret );
                chainParams->loadConfig( output );

                // insecure schain owner(originator) private key
                // address is 0x5C4e11842E8be09264dc1976943571d7Af6d00F9
                coinbase = dev::KeyPair( dev::Secret(
                    "0x1c2cd4b70c2b8c6cd7144bbbfbd1e5c6eacb4a5efd9c86d0e29cbbec4e8483b9" ) );
                // address is 0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f
                account3 = dev::KeyPair( dev::Secret(
                    "0x23ABDBD3C61B5330AF61EBE8BEF582F4E5CC08E554053A718BDCE7813B9DC1FC" ) );
            }
        } else {
            chainParams->sealEngineName = NoProof::name();
            chainParams->allowFutureBlocks = true;
            chainParams->difficulty = chainParams->getMinimumDifficulty();
            chainParams->gasLimit = chainParams->getMaxGasLimit();
            chainParams->byzantiumForkBlock = 0;
            chainParams->EIP158ForkBlock = 0;
            chainParams->constantinopleForkBlock = 0;
            chainParams->istanbulForkBlock = 0;
#ifndef FAIR
            chainParams->externalGasDifficulty = 1;
            chainParams->sChain.contractStorageLimit = 128;
#endif
            // 615 + 1430 is experimentally-derived block size + average extras size
            chainParams->sChain.dbStorageLimit = 320.5 * ( 615 + 1430 );
#ifdef FAIR
            chainParams->sChain.nodeGroups =
            {
                        { { GroupNode{ u256( 0 ), u256( 8 ),
                                       "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea",
                                       Address( "0x08151B8F80bfa7dEa760e461412AF24348224edf" )
                          } },
                          uint64_t( -1 ),
                          { "3842742177969966091367527274107524613106077736353521259727282251005583743182",
                            "3497912824016228906558906422247670474553186446469877598411863912329082553081",
                            "8173996886448941320370434854289578123609627835954133538412363037981850950343",
                            "20979370720689475348670582375026949105497642726992863932315517524004804784155" }
                          }
            };
            chainParams->sChain.nodes[0].owner = jsToAddress( "0x0E7d7F1D34a502bD609542576941C3FCc087c588" );
#endif
#ifndef FAIR
            chainParams->sChain
                ._patchTimestamps[static_cast< size_t >( SchainPatchEnum::ContractStoragePatch )] =
                1;
            chainParams->sChain._patchTimestamps[static_cast< size_t >(
                SchainPatchEnum::StorageDestructionPatch )] = 1;
            powPatchActivationTimestamp = time( nullptr ) + 60;
            chainParams->sChain
                ._patchTimestamps[static_cast< size_t >( SchainPatchEnum::CorrectForkInPowPatch )] =
                powPatchActivationTimestamp;
            push0PatchActivationTimestamp = time( nullptr ) + 10;
            chainParams->sChain
                ._patchTimestamps[static_cast< size_t >( SchainPatchEnum::PushZeroPatch )] =
                push0PatchActivationTimestamp;
#endif
            chainParams->sChain.emptyBlockIntervalMs = _emptyBlockIntervalMs;
            // add random extra data to randomize genesis hash and get random DB path,
            // so that tests can be run in parallel
            // TODO: better make it use ethemeral in-memory databases
            chainParams->extraData = h256::random().asBytes();
            chainParams->nodeInfo.port = chainParams->nodeInfo.port6 = rand_port;
            chainParams->sChain.nodes[0].port = chainParams->sChain.nodes[0].port6 = rand_port;
            chainParams->skaleDisableChainIdCheck = true;

            if ( params.count( "getLogsBlocksLimit" ) && stoi( params.at( "getLogsBlocksLimit" ) ) )
                chainParams->logsBlocksLimit = stoi( params.at( "getLogsBlocksLimit" ) );
            if ( params.count( "getResponseLogCountLimit" ) && stoi( params.at( "getResponseLogCountLimit" ) ) )
                chainParams->responseLogCountLimit = stoi( params.at( "getResponseLogCountLimit" ) );
        }
        chainParams->sChain.multiTransactionMode = _mtmEnabled;
        chainParams->nodeInfo.syncNode = _isSyncNode;

        auto monitor = make_shared< InstanceMonitor >( "test" );


        setenv( "DATA_DIR", tempDir.path().c_str(), 1 );
        client.reset( new eth::ClientTest( chainParams, ( int ) chainParams->getNetworkId(),
            shared_ptr< GasPricer >(), NULL, monitor, tempDir.path(), WithExisting::Kill ) );

        if ( !_generation2 )
            client->setAuthor( coinbase.address() );
        else
            client->setAuthor( chainParams->getBlockAuthor() );

        // wait for 1st block - because it's always empty
        std::promise< void > blockPromise;
        auto importHandler = client->setOnBlockImport(
            [&blockPromise]( BlockHeader const& ) { blockPromise.set_value(); } );

        client->injectSkaleHost();
        client->startWorking();

        if ( !_isSyncNode )
            blockPromise.get_future().wait();

        using FullServer = ModularServer< rpc::EthFace, rpc::SkaleFace, rpc::NetFace, rpc::Web3Face,
            rpc::AdminEthFace /*, rpc::AdminNetFace*/, rpc::DebugFace, rpc::TestFace >;

        accountHolder.reset( new FixedAccountHolder( [&]() { return client.get(); }, {} ) );
        accountHolder->setAccounts( { coinbase, account2, account3 } );

        sessionManager.reset( new rpc::SessionManager() );
        adminSession =
            sessionManager->newSession( rpc::SessionPermissions{ { rpc::Privilege::Admin } } );

        auto ethFace = new rpc::Eth(
            _config.empty() ? std::string( "" ) : _config, *client, *accountHolder.get() );

        dev::rpc::Skale* skaleFace = nullptr;
#ifdef BITE
        skaleFace = new rpc::Skale( *client );
#endif

        gasPricer = make_shared< eth::TrivialGasPricer >( 0, DefaultGasPrice );

        rpcServer.reset( new FullServer( ethFace, skaleFace, new rpc::Net( chainParams ),
            new rpc::Web3(),  // TODO Add version parameter here?
            new rpc::AdminEth( *client, *gasPricer, keyManager, *sessionManager ),
            new rpc::Debug( *client, nullptr, "" ), new rpc::Test( *client ) ) );


        //
        SkaleServerOverride::opts_t serverOpts;

        inject_rapidjson_handlers( serverOpts, ethFace );

        serverOpts.netOpts_.bindOptsStandard_.cntServers_ = 1;
        serverOpts.netOpts_.bindOptsStandard_.strAddrHTTP4_ = chainParams->getSelfNodeIp();
        // random port
        // +3 because rand() seems to be called effectively simultaneously here and in "static"
        // section - thus giving same port for consensus
        serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ = std::rand() % 64000 + 1025 + 3;
        skale_server_connector = new SkaleServerOverride( chainParams, client.get(), serverOpts );
        rpcServer->addConnector( skale_server_connector );
        skale_server_connector->StartListening();

        sleep( 1 );

        httpClient = new jsonrpc::HttpClient(
            "http://" + chainParams->getSelfNodeIp() + ":" +
            std::to_string( serverOpts.netOpts_.bindOptsStandard_.nBasePortHTTP4_ ) );
        httpClient->SetTimeout( 1000000000 );

        rpcClient = unique_ptr< WebThreeStubClient >( new WebThreeStubClient( *httpClient ) );

        BOOST_TEST_MESSAGE( "Constructed JsonRpcFixture" );
    }

    ~JsonRpcFixture() {
#ifdef FAIR
        // Make sure safe consensus is properly updated before exit (epoch id in particular).
        // Only related to FAIR
        const auto deadline = chrono::steady_clock::now() + chrono::seconds( 10 );
        while ( client->skaleHost()->ignoreNewBlocksEnabled() && chrono::steady_clock::now() < deadline ) {
            usleep( 10 );
        }
        if ( client->skaleHost()->isConsesusUpdateHappened() )
            client->skaleHost()->handleConsensusUpdate();
#endif

        if ( skale_server_connector )
            skale_server_connector->StopListening();

        if ( httpClient )
            delete httpClient;

        if ( g_skaleHost )
            g_skaleHost.reset();

        BOOST_TEST_MESSAGE( "Destructed JsonRpcFixture" );
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

    TransientDirectory tempDir;  // ! should exist before client!
    unique_ptr< Client > client;

    dev::KeyPair coinbase{ KeyPair::create() };
    dev::KeyPair account2{ KeyPair::create() };
    dev::KeyPair account3{ KeyPair::create() };
    unique_ptr< FixedAccountHolder > accountHolder;
    unique_ptr< rpc::SessionManager > sessionManager;
    std::shared_ptr< eth::TrivialGasPricer > gasPricer;
    KeyManager keyManager{ KeyManager::defaultPath(), SecretStore::defaultPath() };
    unique_ptr< ModularServer<> > rpcServer;
    unique_ptr< WebThreeStubClient > rpcClient;
    std::string adminSession;
    SkaleServerOverride* skale_server_connector;
    jsonrpc::HttpClient* httpClient;
    time_t powPatchActivationTimestamp;
    time_t push0PatchActivationTimestamp;
};

#ifndef FAIR
struct RestrictedAddressFixture : public JsonRpcFixture {
    RestrictedAddressFixture(
        const std::string& _config = c_genesisConfigString, bool _mtmEnabled = false )
        : JsonRpcFixture( _config, true, true, false, _mtmEnabled ) {
        setenv( "HOME", tempDir.path().c_str(), 1 );  // getDataDir() now points to the different
                                                      // directories for different tests
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
#endif

string fromAscii( string _s ) {
    bytes b = asBytes( _s );
    return toHexPrefixed( b );
}
}  // namespace

#ifdef BITE
/// Helper functions

dev::bytes formEncryptedMessageMockup( const dev::bytes& message, const dev::Address& toAddress ) {
    RLPStream biteDataRlp( 2 );

    biteDataRlp << message;
    biteDataRlp << ( dev::Address::Arith ) toAddress;

    auto messageToEncrypt = biteDataRlp.out();
    auto encryptedMessage = libBLS::ThresholdEncryption::mockupEncrypt( messageToEncrypt );

    u256 epochId = 0;

    RLPStream bitePayloadRlp( 2 );

    bitePayloadRlp << epochId;
    bitePayloadRlp << encryptedMessage;

    auto rlpBytes = bitePayloadRlp.out();
    return dev::bytes( rlpBytes.begin(), rlpBytes.end() );
}

std::string formTransactionRlp( const JsonRpcFixture& fixture, const std::string& senderAddress,
    const std::string& data, size_t& nonce,
    const std::string& toAddress = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251" ) {
    Json::Value txEncryptedData;
    txEncryptedData["to"] = toAddress;
    txEncryptedData["from"] = senderAddress;
    txEncryptedData["gas"] = "100000";
    txEncryptedData["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txEncryptedData["data"] = data;
    txEncryptedData["nonce"] = nonce++;

    TransactionSkeleton ts = toTransactionSkeleton( txEncryptedData );
    ts = fixture.client->populateTransactionWithDefaults( ts );
    pair< bool, Secret > ar = fixture.accountHolder->authenticate( ts );
    Transaction tx( ts, ar.second );

    return dev::toHexPrefixed( tx.toBytes() );
}

std::string formBITEPayloadRlp( u256 _epochId, const dev::bytes& _encryptedBITEData ) {
    RLPStream bitePayloadRlp( 2 );

    bitePayloadRlp << _epochId;
    bitePayloadRlp << _encryptedBITEData;

    auto rlpBytes = bitePayloadRlp.out();
    return dev::toHexPrefixed( rlpBytes );
}

#endif // BITE

BOOST_AUTO_TEST_SUITE( JsonRpcSuite )


BOOST_AUTO_TEST_CASE( jsonrpc_gasPrice ) {
    JsonRpcFixture fixture;
    string gasPrice = fixture.rpcClient->eth_gasPrice();
    BOOST_CHECK_EQUAL( gasPrice, toJS( 20 * dev::eth::shannon ) );
}


BOOST_AUTO_TEST_CASE(
    jsonrpc_accounts, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
    JsonRpcFixture fixture;
    std::vector< dev::KeyPair > keys = { KeyPair::create(), KeyPair::create() };
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


BOOST_AUTO_TEST_CASE( jsonrpc_netVersion ) {
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

BOOST_AUTO_TEST_CASE(
    eth_coinbase, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
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
#ifdef FAIR
    const u256 blockReward = fixture.client->chainParams().blockReward(
                fixture.client->blockChain().info().timestamp(),
                fixture.client->blockChain().info().number() );
#else
    const u256 blockReward = 2 * dev::eth::ether;
#endif
    dev::eth::simulateMining( *( fixture.client ), blocksToMine );
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
#ifdef FAIR
    const u256 blockReward = fixture.client->chainParams().blockReward(
                fixture.client->blockChain().info().timestamp(),
                fixture.client->blockChain().info().number() );
#else
    const u256 blockReward = 2 * dev::eth::ether;
#endif
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
#ifdef FAIR
    const u256 blockReward = fixture.client->chainParams().blockReward(
                fixture.client->blockChain().info().timestamp(),
                fixture.client->blockChain().info().number() );
#else
    const u256 blockReward = 2 * dev::eth::ether;
#endif
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
#ifdef FAIR
    const u256 blockReward = fixture.client->chainParams().blockReward(
                fixture.client->blockChain().info().timestamp(),
                fixture.client->blockChain().info().number() );
#else
    const u256 blockReward = 2 * dev::eth::ether;
#endif
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

#ifdef FAIR

/**
 * Auxiliar function to mine a block to gather balance,
 * and build a signed legacy pre-EIP155 transaction.
 * Used to test acceptance vs. rejection of pre-EIP155 transactions
 */
Json::Value buildSignedTransaction( JsonRpcFixture& fixture ) {
    auto receiver = KeyPair::create();
    auto senderAddress = fixture.coinbase.address();

    // Mine to generate a non-zero account balance
    const size_t blocksToMine = 1;
    const u256 blockReward = fixture.client->chainParams().blockReward(
                fixture.client->blockChain().info().timestamp(),
                fixture.client->blockChain().info().number() );
    dev::eth::simulateMining( *( fixture.client ), blocksToMine );
    BOOST_CHECK_EQUAL( blockReward, fixture.client->balanceAt( senderAddress ) );

    Json::Value sampleTx;
    sampleTx["to"] = toJS( receiver.address() );
    sampleTx["from"] = toJS( senderAddress );
    sampleTx["gas"] = "100000";
    sampleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    sampleTx["value"] = jsToDecimal( toJS( 10000 * dev::eth::szabo ) );
    sampleTx["data"] = "0x0";
    sampleTx["nonce"] = 0;

    return fixture.rpcClient->eth_signTransaction( sampleTx );
};

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_acceptPreEIP155Txns_configOmitted ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    Json::FastWriter fastWriter;

    // 'allowPreEIP155Txns' field is not set -> should be allowed by default
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );
    auto signedTx = buildSignedTransaction( fixture );
    std::string txHash = fixture.rpcClient->eth_sendRawTransaction( signedTx["raw"].asString() );
    BOOST_REQUIRE( !txHash.empty() );
}

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_acceptPreEIP155Txns_configTrue ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    Json::FastWriter fastWriter;

    // 'allowPreEIP155Txns' field is set to true -> should be allowed
    ret["params"]["allowPreEIP155Txns"] = true;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );
    auto signedTx = buildSignedTransaction( fixture );
    std::string txHash = fixture.rpcClient->eth_sendRawTransaction( signedTx["raw"].asString() );
    BOOST_REQUIRE( !txHash.empty() );
}

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_rejectPreEIP155Txns_configFalse ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    Json::FastWriter fastWriter;

    // 'allowPreEIP155Txns' field is set to false -> should be rejected
    ret["params"]["allowPreEIP155Txns"] = false;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );
    auto signedTx = buildSignedTransaction( fixture );

    std::cout << "Raw transaction: " << signedTx["raw"].asString() << std::endl;
    BOOST_REQUIRE_THROW(
        fixture.rpcClient->eth_sendRawTransaction( signedTx["raw"].asString() ),
        jsonrpc::JsonRpcException
    );
}

#endif




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

    BOOST_REQUIRE( fixture.client->transactionQueueStatus().current == 0 );

    // Sending tx to sync node
    string txHash = fixture.rpcClient->eth_sendTransaction( create );

    auto pendingTransactions = fixture.client->pending();
    BOOST_REQUIRE( pendingTransactions.size() == 1 );
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


const string skaledConfigFileName = "../../test/historicstate/configs/basic_config.json";


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
    inputCall["input"] =
        "0xb3de648b0000000000000000000000000000000000000000000000000000000000000001";
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
#ifdef FAIR
    BOOST_CHECK_EQUAL( receipt["status"], string( "0x1" ) );
    Json::Value code =
        fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString() == "0x608060405260043610603f576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff168063b3de648b146044575b600080fd5b3415604e57600080fd5b606a600480360381019080803590602001909291905050506080565b6040518082815260200191505060405180910390f35b60006007820290509190505600a165627a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb98fb480406fc2728a960029" );
#else
    BOOST_CHECK_EQUAL( receipt["status"], string( "0x0" ) );
    Json::Value code =
        fixture.rpcClient->eth_getCode( receipt["contractAddress"].asString(), "latest" );
    BOOST_REQUIRE( code.asString() == "0x" );
#endif
}

BOOST_AUTO_TEST_CASE( deploy_contract_without_controller ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    ret["accounts"].removeMember( "0xD2002000000000000000000000000000000000D2" );
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

#ifndef FAIR
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
#endif


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
    BOOST_CHECK(
        response1 != "0x0000000000000000000000000000000000000000000000000000000000000000" );

    fixture.client->setAuthor( senderAddress );
    transactionCallObject["from"] = toJS( senderAddress );
    fixture.rpcClient->eth_sendTransaction( transactionCallObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    string response2 = fixture.rpcClient->eth_call( checkAddress, "latest" );
    BOOST_CHECK(
        response2 != "0x0000000000000000000000000000000000000000000000000000000000000000" );
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

    then convert to yul:  --ir p0test.sol` >p0test.yul

    then change code:
                    {
                        let r := add(88,99)
                        let tmp := verbatim_0i_1o(hex"5f")
                    }

    then compile!

    */
    string compiled =
        "608060405234156100135761001261003b565b5b61001b610040565b610023610031565b6101b7610043823961"
        "01b781f35b6000604051905090565b600080fd5b56fe608060405261000f36600061015b565b805160208201f3"
        "5b60006060905090565b6000604051905090565b6000601f19601f8301169050919050565b7f4e487b71000000"
        "00000000000000000000000000000000000000000000000000600052604160045260246000fd5b610073826100"
        "2a565b810181811067ffffffffffffffff821117156100925761009161003b565b5b80604052505050565b6000"
        "6100a5610020565b90506100b1828261006a565b919050565b600067ffffffffffffffff8211156100d1576100"
        "d061003b565b5b6100da8261002a565b9050602081019050919050565b60006100f2826100b6565b6100fb8161"
        "009b565b915082825250919050565b7f7375636365737300000000000000000000000000000000000000000000"
        "000000600082015250565b600061013b60076100e7565b905061014960208201610106565b90565b6000610156"
        "61012f565b905090565b6000610165610017565b809150600a6009015f505061017861014c565b915050929150"
        "5056fea2646970667358221220b3871ed09fbcbb1dac74c3cd48dafa5d097bea7c808b5ff2c16a996cf108d3c6"
        "64736f6c63430008190033";
    //        "60806040523415601057600f6031565b5b60166036565b601c6027565b604c60398239604c81f35b6000604051905090565b600080fd5b56fe6080604052600a600c565b005b60636058015f505056fea2646970667358221220ee9861b869ceda6de64f2ec7ccbebf2babce54b35502a866a4193e05ae595e1f64736f6c63430008130033";

    Json::Value create;

    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "1000000";

    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );  // deploy should succeed
    string contractAddress = receipt["contractAddress"].asString();

    Json::Value callObject;

    callObject["from"] = toJS( fixture.account2.address() );
    callObject["to"] = contractAddress;

#ifndef FAIR
    // first try without PushZeroPatch

    txHash = fixture.rpcClient->eth_sendTransaction( callObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x0" ) );  // exec should fail

    string callResult = fixture.rpcClient->eth_call( callObject, "latest" );
    BOOST_REQUIRE_EQUAL( callResult, string( "0x" ) );  // call too

    // wait for block after timestamp
    BOOST_REQUIRE_LT( fixture.client->blockInfo( LatestBlock ).timestamp(),
        fixture.push0PatchActivationTimestamp );
    while ( time( nullptr ) < fixture.push0PatchActivationTimestamp )
        sleep( 1 );

    // 1st timestamp-crossing block
    txHash = fixture.rpcClient->eth_sendTransaction( callObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE_GE( fixture.client->blockInfo( LatestBlock ).timestamp(),
        fixture.push0PatchActivationTimestamp );
#endif

#ifdef HISTORIC_STATE
    uint64_t crossingBlockNumber = fixture.client->number();
#endif

#ifndef FAIR
    // in the "crossing" block tx still should fail
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x0" ) );
#endif

    // in 1st block with patch call should succeed
    auto callResult1 = fixture.rpcClient->eth_call( callObject, "latest" );
    BOOST_REQUIRE_NE( callResult1, string( "0x" ) );

    // tx should succeed too
    txHash = fixture.rpcClient->eth_sendTransaction( callObject );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );

#ifdef HISTORIC_STATE
#ifndef FAIR
    // historic call should fail before activation and succees after it
    callResult1 = fixture.rpcClient->eth_call( callObject, toJS( crossingBlockNumber - 1 ) );
    BOOST_REQUIRE_EQUAL( callResult1, string( "0x" ) );
#endif // FAIR

    callResult1 = fixture.rpcClient->eth_call( callObject, toJS( crossingBlockNumber ) );
    BOOST_REQUIRE_NE( callResult1, string( "0x" ) );
#endif // HISTORIC_STATE
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
    testRevert["data"] =
        "0x20987767000000000000000000000000000000000000000000000000000000000000c350";
    string response = fixture.estimateGasShouldFail( testRevert );
    BOOST_CHECK(
        response.find( "EVM revert instruction without description message" ) != string::npos );

    Json::Value testPositive;
    testPositive["to"] = "0xD2001300000000000000000000000000000000D4";
    testPositive["data"] =
        "0xfdde8d66000000000000000000000000000000000000000000000000000000000000c350";
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
    testRevert["data"] =
        "0x6080604052348015600f57600080fd5b50604051633013bad360e21b81524660048201526024016040518091"
        "0390fdfe";

    try {
        fixture.rpcClient->eth_estimateGas( testRevert, "latest" );
    } catch ( jsonrpc::JsonRpcException& ex ) {
        BOOST_CHECK_EQUAL( ex.GetCode(), 3 );
        BOOST_CHECK_EQUAL( ex.GetData().asString(),
            "0xc04eeb4c000000000000000000000000000000000000000000000000000000000000ffff" );
        BOOST_CHECK_EQUAL( ex.GetMessage(), "EVM revert instruction without description message" );
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

BOOST_AUTO_TEST_CASE( web3_sha3, *boost::unit_test::precondition( dev::test::run_not_express ) ) {
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
    std::cout << blockHash << std::endl;
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
    try {
        receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    } catch ( ... ) {
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
    } catch ( jsonrpc::JsonRpcException& ex ) {
        BOOST_CHECK_EQUAL( ex.GetCode(), 3 );
        BOOST_CHECK_EQUAL( ex.GetData().asString(), "0x82b42900" );
        BOOST_CHECK_EQUAL( ex.GetMessage(), "EVM revert instruction without description message" );
    }
}

BOOST_AUTO_TEST_CASE( eth_call_create ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );

    auto senderAddress = fixture.coinbase.address();

    // contract test {
    //   function f(uint a) returns(uint d) { return a * 7; }
    // }
    string compiled =
        "6080604052348015600f57600080fd5b5060b98061001d6000396000f300"
        "608060405260043610603f576000357c01000000000000000000000000"
        "00000000000000000000000000000000900463ffffffff168063b3de64"
        "8b146044575b600080fd5b3415604e57600080fd5b606a600480360381"
        "019080803590602001909291905050506080565b604051808281526020"
        "0191505060405180910390f35b60006007820290509190505600a16562"
        "7a7a72305820f294e834212334e2978c6dd090355312a3f0f9476b8eb9"
        "8fb480406fc2728a960029";

    // eth_call with no "to" field triggers creation transaction
    Json::Value callObject;
    callObject["from"] = toJS( senderAddress );
    callObject["data"] = "0x" + compiled;
    callObject["gas"] = "1000000";
    callObject["gasPrice"] = "0";

    string callResult = fixture.rpcClient->eth_call( callObject, "latest" );
    // creation via eth_call should return the runtime bytecode
    BOOST_REQUIRE( callResult.size() > 2 );
    BOOST_REQUIRE( callResult != "0x" );

    // verify state was not modified by eth_call (nonce unchanged)
    u256 nonceAfterCall = jsToU256(
        fixture.rpcClient->eth_getTransactionCount( toJS( senderAddress ), "latest" ) );
    BOOST_REQUIRE_EQUAL( nonceAfterCall, 0 );

    // now actually deploy via eth_sendTransaction and verify we get the same runtime bytecode
    Json::Value deployTx;
    deployTx["from"] = toJS( senderAddress );
    deployTx["code"] = compiled;
    deployTx["gas"] = "1000000";
    string txHash = fixture.rpcClient->eth_sendTransaction( deployTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );
    BOOST_REQUIRE( !receipt["contractAddress"].isNull() );

    string contractAddress = receipt["contractAddress"].asString();
    string deployedCode = fixture.rpcClient->eth_getCode( contractAddress, "latest" );

    // the runtime bytecode from eth_call should match the actually deployed code
    BOOST_REQUIRE_EQUAL( callResult, deployedCode );
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
    } catch ( jsonrpc::JsonRpcException& ex ) {
        BOOST_CHECK_EQUAL( ex.GetCode(), 3 );
        BOOST_CHECK_EQUAL( ex.GetData().asString(), "0x82b42900" );
        BOOST_CHECK_EQUAL( ex.GetMessage(), "EVM revert instruction without description message" );
    }
}

#ifndef FAIR
BOOST_AUTO_TEST_CASE( simplePoWTransaction ) {
    u256 ESTIMATE_AFTER_PATCH = u256( 21000 + 1024 * 16 );
    u256 ESTIMATE_BEFORE_PATCH = u256( 21000 + 1024 * 68 );

    // 1s empty block interval
    JsonRpcFixture fixture( "", true, true, false, false, false, 1000 );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    auto senderAddress = fixture.coinbase.address();

    Json::Value transact;
    transact["from"] = toJS( senderAddress );
    transact["to"] = toJS( senderAddress );
    // 1k
    ostringstream ss( "0x" );
    for ( int i = 0; i < 1024 / 16; ++i )
        ss << "112233445566778899aabbccddeeff11";
    transact["data"] = ss.str();

    string gasEstimateStr = fixture.rpcClient->eth_estimateGas( transact );
    u256 gasEstimate = jsToU256( gasEstimateStr );

    // old estimate before patch
    BOOST_REQUIRE_EQUAL( gasEstimate, ESTIMATE_BEFORE_PATCH );

    u256 powGasPrice = 0;


    do {
        // mine enough POW to tun transaction after PATCH but not before patch
        const u256 GAS_PER_HASH = 1;
        u256 candidate = h256::random();
        h256 hash = dev::sha3( senderAddress ) ^ dev::sha3( u256( 0 ) ) ^ dev::sha3( candidate );
        u256 externalGas = ~u256( 0 ) / u256( hash ) / GAS_PER_HASH;
        if ( externalGas >= ESTIMATE_AFTER_PATCH &&
             externalGas < ESTIMATE_AFTER_PATCH + ESTIMATE_AFTER_PATCH / 10 ) {
            powGasPrice = candidate;
        }
    } while ( !powGasPrice );
    // Account balance is too low will mean that PoW didn't work out
    transact["gasPrice"] = toJS( powGasPrice );

    // we may've been calculating pow for too long and patch is active already
    // need to know the block number at this point
    auto latestBlockNumber =
        fixture.client->blockInfo( fixture.client->hashFromNumber( LatestBlock ) ).number();

    // wait for patch turning on and see how it happens
    string txHash;
    BlockHeader badInfo, goodInfo;
    uint64_t blockCounter = 2;
    for ( ;; ) {
        gasEstimateStr = fixture.rpcClient->eth_estimateGas( transact );
        gasEstimate = jsToU256( gasEstimateStr );
        // old
        if ( gasEstimate == ESTIMATE_BEFORE_PATCH ) {
            // we are before patch. Sending show fail since we do not have enough PoW gas
            try {
                fixture.rpcClient->eth_sendTransaction( transact );
                BOOST_REQUIRE( false );
            } catch ( const std::exception& ex ) {
                assert( string( ex.what() ).find( "balance is too low" ) != string::npos );
                badInfo =
                    fixture.client->blockInfo( fixture.client->hashFromNumber( LatestBlock ) );
                dev::eth::mineTransaction( *( fixture.client ), 1 );  // empty block
                fixture.client->state().getOriginalDb()->createBlockSnap( blockCounter );
                blockCounter++;
            }
        } else {  // now we are after patch
            BOOST_REQUIRE_EQUAL( gasEstimate, ESTIMATE_AFTER_PATCH );
            txHash = fixture.rpcClient->eth_sendTransaction( transact );
            goodInfo = fixture.client->blockInfo( fixture.client->hashFromNumber( LatestBlock ) );
            break;
        }
    }

    BOOST_REQUIRE_LT( badInfo.timestamp(), fixture.powPatchActivationTimestamp );
    BOOST_REQUIRE_GE( goodInfo.timestamp(), fixture.powPatchActivationTimestamp );
    BOOST_REQUIRE_EQUAL( std::max( badInfo.number() + 1, latestBlockNumber ), goodInfo.number() );

    dev::eth::mineTransaction( *( fixture.client ), 1 );
    fixture.client->state().getOriginalDb()->createBlockSnap( blockCounter );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], "0x1" );
}
#endif

#ifndef FAIR
BOOST_AUTO_TEST_CASE( clearPartialReceipts ) {
    // Prepare fixture
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    std::string chainID = "0x97";  // 151
    ret["params"]["chainID"] = chainID;
    time_t clearPartialReceiptsActivationTs = time( nullptr ) + 10;
    ret["skaleConfig"]["sChain"]["clearPartialReceiptsPatchTimestamp"] =
        clearPartialReceiptsActivationTs;
    ret["skaleConfig"]["sChain"]["singleStateCommitPerBlockPatchTimestamp"] =
        std::numeric_limits< time_t >::max();

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    // To fill coinbase wallet
    dev::eth::simulateMining( *( fixture.client ), 20 );

    string senderAddress = toJS( fixture.coinbase.address() );

    Json::Value transactionCallObject;
    transactionCallObject["from"] = toJS( senderAddress );
    transactionCallObject["to"] = "0x692a70d2e424a56d2c6c27aa97d1a86395877b3a";
    transactionCallObject["data"] = "0x28b5e32b";

    int64_t blocksToCheck = 6, timestampTransitionBlock = 4, expectedNoLegacyReceiptsBlock = 5;

    for ( int64_t block = 2; block < blocksToCheck; ++block ) {
        if ( block == timestampTransitionBlock ) {
            sleep( 12 );
        }

        TransactionSkeleton ts = toTransactionSkeleton( transactionCallObject );
        ts = fixture.client->populateTransactionWithDefaults( ts );
        pair< bool, Secret > ar = fixture.accountHolder->authenticate( ts );
        Transaction tx( ts, ar.second );
        auto txHash = fixture.rpcClient->eth_sendRawTransaction( toJS( tx.toBytes() ) );
        dev::eth::mineTransaction( *( fixture.client ), 1 );
        auto receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
        dev::eth::BlockNumber blockNumber = jsToInt( receipt["blockNumber"].asString() );
        skale::State state( fixture.client->state() );
        BOOST_REQUIRE_EQUAL( blockNumber, block );
        BOOST_REQUIRE_EQUAL( state.safePartialTransactionReceipts( blockNumber ).size(), 0 );
        int64_t expectedSize = block == expectedNoLegacyReceiptsBlock ? 0 : 1;
        BOOST_REQUIRE_EQUAL( state.safeLegacyPartialTransactionReceipts().size(), expectedSize );
    }
}

BOOST_AUTO_TEST_CASE( single_state_commit_per_block_patch_transition ) {
    Json::Value configJson;
    Json::Reader().parse( c_genesisConfigString, configJson );
    time_t activationTimestamp = time( nullptr ) + 5;
    configJson["skaleConfig"]["sChain"]["SingleStateCommitPerBlockPatchTimestamp"] =
        static_cast< Json::Int64 >( activationTimestamp );

    Json::FastWriter fastWriter;
    JsonRpcFixture fixture( fastWriter.write( configJson ), true, true, false, true );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    struct DbCommitCounterGuard {
        DbCommitCounterGuard() { skale::state_commit_counter::enable( true ); }
        ~DbCommitCounterGuard() { skale::state_commit_counter::enable( false ); }
    } guard;

    u256 nextNonce;

    auto sendPayment = [&]() {
        Json::Value tx;
        tx["from"] = fixture.coinbase.address().hex();
        tx["to"] = fixture.account2.address().hex();
        tx["value"] = toJS( 1 );
        tx["gas"] = toJS( 21000 );
        tx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
        tx["nonce"] = toJS( nextNonce );
        nextNonce++;
        std::string hash = fixture.rpcClient->eth_sendTransaction( tx );
        BOOST_REQUIRE( !hash.empty() );
    };

    auto produceBlockWithTransaction = [&]() {
        nextNonce = jsToU256( fixture.rpcClient->eth_getTransactionCount(
            fixture.coinbase.address().hex(), "latest" ) );
        sendPayment();
        dev::eth::mineTransaction( *( fixture.client ), 1 );
    };

    skale::state_commit_counter::reset();
    produceBlockWithTransaction();
    // Before activation: 2 commits (1 for tx execution, 1 for block state)
    BOOST_REQUIRE_EQUAL( skale::state_commit_counter::count(), 2 );

    // Receipts should not be saved before activation
    auto progressLog = fixture.client->state().getProgressLog();
    BOOST_REQUIRE( progressLog );
    auto receiptsBefore = progressLog->loadProgressData();
    BOOST_CHECK( !receiptsBefore );

    sleep( 6 );

    // Produce a block after the activation timestamp so subsequent blocks observe
    // commit-per-block semantics.
    produceBlockWithTransaction();

    skale::state_commit_counter::reset();
    produceBlockWithTransaction();
    // After activation: 1 commit (single commit per block)
    BOOST_REQUIRE_EQUAL( skale::state_commit_counter::count(), 1 );

    // Receipts should be saved after activation
    auto receiptsAfter = progressLog->loadProgressData();
    BOOST_REQUIRE( receiptsAfter );
    BOOST_CHECK_EQUAL( receiptsAfter->receipts.size(), 1 );
}

BOOST_AUTO_TEST_CASE( state_progress_log_skip_already_committed ) {
    Json::Value configJson;
    Json::Reader().parse( c_genesisConfigString, configJson );
    configJson["skaleConfig"]["sChain"]["singleStateCommitPerBlockPatchTimestamp"] =
        static_cast< Json::Int64 >( 1 );

    Json::FastWriter fastWriter;
    JsonRpcFixture fixture( fastWriter.write( configJson ), true, true, false, true );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    struct DbCommitCounterGuard {
        DbCommitCounterGuard() { skale::state_commit_counter::enable( true ); }
        ~DbCommitCounterGuard() { skale::state_commit_counter::enable( false ); }
    } guard;

    u256 nextNonce;

    auto sendPayment = [&]() {
        Json::Value tx;
        tx["from"] = fixture.coinbase.address().hex();
        tx["to"] = fixture.account2.address().hex();
        tx["value"] = toJS( 1 );
        tx["gas"] = toJS( 21000 );
        tx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
        tx["nonce"] = toJS( nextNonce );
        nextNonce++;
        std::string hash = fixture.rpcClient->eth_sendTransaction( tx );
        BOOST_REQUIRE( !hash.empty() );
    };

    auto produceBlockWithTransaction = [&]() {
        nextNonce = jsToU256( fixture.rpcClient->eth_getTransactionCount(
            fixture.coinbase.address().hex(), "latest" ) );
        sendPayment();
        dev::eth::mineTransaction( *( fixture.client ), 1 );
    };

    produceBlockWithTransaction();

    skale::state_commit_counter::reset();
    produceBlockWithTransaction();
    BOOST_REQUIRE_EQUAL( skale::state_commit_counter::count(), 1 );

    auto progressLog = fixture.client->state().getProgressLog();
    BOOST_REQUIRE( progressLog );
    BOOST_CHECK( progressLog->isBlockCommitCompleted( fixture.client->number() ) );
}

BOOST_AUTO_TEST_CASE( state_progress_log_crash_recovery ) {
    Json::Value configJson;
    Json::Reader().parse( c_genesisConfigString, configJson );
    configJson["skaleConfig"]["sChain"]["singleStateCommitPerBlockPatchTimestamp"] =
        static_cast< Json::Int64 >( 1 );

    Json::FastWriter fastWriter;
    JsonRpcFixture fixture( fastWriter.write( configJson ), true, true, false, true );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    struct DbCommitCounterGuard {
        DbCommitCounterGuard() { skale::state_commit_counter::enable( true ); }
        ~DbCommitCounterGuard() { skale::state_commit_counter::enable( false ); }
    } guard;

    u256 nextNonce;

    auto sendPayment = [&]() {
        Json::Value tx;
        tx["from"] = fixture.coinbase.address().hex();
        tx["to"] = fixture.account2.address().hex();
        tx["value"] = toJS( 1 );
        tx["gas"] = toJS( 21000 );
        tx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
        tx["nonce"] = toJS( nextNonce );
        nextNonce++;
        std::string hash = fixture.rpcClient->eth_sendTransaction( tx );
        BOOST_REQUIRE( !hash.empty() );
    };

    auto produceBlockWithTransaction = [&]() {
        nextNonce = jsToU256( fixture.rpcClient->eth_getTransactionCount(
            fixture.coinbase.address().hex(), "latest" ) );
        sendPayment();
        dev::eth::mineTransaction( *( fixture.client ), 1 );
    };

    produceBlockWithTransaction();

    auto progressLog = fixture.client->state().getProgressLog();
    BOOST_REQUIRE( progressLog );

    uint64_t completedBlock = fixture.client->number();
    BOOST_CHECK( progressLog->isBlockCommitCompleted( completedBlock ) );

    progressLog->markBlockCommitStarted( completedBlock + 1 );
    BOOST_CHECK( !progressLog->isBlockCommitCompleted( completedBlock + 1 ) );

    skale::state_commit_counter::reset();
    produceBlockWithTransaction();
    BOOST_REQUIRE_EQUAL( skale::state_commit_counter::count(), 1 );

    BOOST_CHECK( progressLog->isBlockCommitCompleted( fixture.client->number() ) );
}

namespace {
template < typename PatchType >
void waitForPatchActivation( int timeoutSeconds = 10 ) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( timeoutSeconds );
    while ( !PatchType::isEnabledWhen( time( nullptr ) ) ) {
        BOOST_REQUIRE( std::chrono::steady_clock::now() < deadline );
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    }
}
}  // namespace

#ifndef FAIR
// Test fs commits before SingleStateCommitPerBlockPatch is active
BOOST_AUTO_TEST_CASE( single_fs_commit_per_block_patch_before ) {
    Json::Value configJson;
    Json::Reader().parse( c_genesisConfigString, configJson );
    // Patch disabled (far future timestamp)
    configJson["skaleConfig"]["sChain"]["singleStateCommitPerBlockPatchTimestamp"] =
        static_cast< Json::Int64 >( 0 );
    configJson["skaleConfig"]["sChain"]["revertableFSPatchTimestamp"] =
        static_cast< Json::Int64 >( 1 );

    Json::FastWriter fastWriter;
    RestrictedAddressFixture fixture( fastWriter.write( configJson ), true );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    struct FsCommitCounterGuard {
        FsCommitCounterGuard() { skale::fs_commit_counter::enable( true ); }
        ~FsCommitCounterGuard() { skale::fs_commit_counter::enable( false ); }
    } guard;

    auto senderAddress = fixture.coinbase.address();
    fixture.client->setAuthor( senderAddress );

    u256 nextNonce;

    auto executeFilestorageOperation = [&]() {
        Json::Value tx;
        tx["from"] = toJS( senderAddress );
        tx["to"] = "0x692a70d2e424a56d2c6c27aa97d1a86395877b3a";
        tx["data"] = "0xf38fb65b";
        tx["nonce"] = toJS( nextNonce );
        nextNonce++;
        TransactionSkeleton ts = toTransactionSkeleton( tx );
        ts = fixture.client->populateTransactionWithDefaults( ts );
        pair< bool, Secret > ar = fixture.accountHolder->authenticate( ts );
        Transaction transaction( ts, ar.second );
        fixture.rpcClient->eth_sendRawTransaction( toJS( transaction.toBytes() ) );
    };

    auto produceBlockWithFilestorageOperations = [&]() {
        nextNonce = jsToU256( fixture.rpcClient->eth_getTransactionCount(
            toJS( senderAddress ), "latest" ) );
        executeFilestorageOperation();
        executeFilestorageOperation();
        dev::eth::mineTransaction( *( fixture.client ), 1 );
        fixture.client->state().getOriginalDb()->createBlockSnap( fixture.client->number() );
    };

    // Without patch: expect 2 commits (one per transaction)
    skale::fs_commit_counter::reset();
    produceBlockWithFilestorageOperations();
    BOOST_REQUIRE_EQUAL( skale::fs_commit_counter::count(), 2 );
}

// Test fs commits after SingleStateCommitPerBlockPatch is active
BOOST_AUTO_TEST_CASE( single_fs_commit_per_block_patch_after ) {
    Json::Value configJson;
    Json::Reader().parse( c_genesisConfigString, configJson );
    // Patch enabled from genesis
    configJson["skaleConfig"]["sChain"]["singleStateCommitPerBlockPatchTimestamp"] =
        static_cast< Json::Int64 >( 1 );
    configJson["skaleConfig"]["sChain"]["revertableFSPatchTimestamp"] =
        static_cast< Json::Int64 >( 1 );

    Json::FastWriter fastWriter;
    RestrictedAddressFixture fixture( fastWriter.write( configJson ), true );
    dev::eth::simulateMining( *( fixture.client ), 1 );

    struct FsCommitCounterGuard {
        FsCommitCounterGuard() { skale::fs_commit_counter::enable( true ); }
        ~FsCommitCounterGuard() { skale::fs_commit_counter::enable( false ); }
    } guard;

    auto senderAddress = fixture.coinbase.address();
    fixture.client->setAuthor( senderAddress );

    u256 nextNonce;

    auto executeFilestorageOperation = [&]() {
        Json::Value tx;
        tx["from"] = toJS( senderAddress );
        tx["to"] = "0x692a70d2e424a56d2c6c27aa97d1a86395877b3a";
        tx["data"] = "0xf38fb65b";
        tx["nonce"] = toJS( nextNonce );
        nextNonce++;
        TransactionSkeleton ts = toTransactionSkeleton( tx );
        ts = fixture.client->populateTransactionWithDefaults( ts );
        pair< bool, Secret > ar = fixture.accountHolder->authenticate( ts );
        Transaction transaction( ts, ar.second );
        fixture.rpcClient->eth_sendRawTransaction( toJS( transaction.toBytes() ) );
    };

    auto produceBlockWithFilestorageOperations = [&]() {
        nextNonce = jsToU256( fixture.rpcClient->eth_getTransactionCount(
            toJS( senderAddress ), "latest" ) );
        executeFilestorageOperation();
        executeFilestorageOperation();
        dev::eth::mineTransaction( *( fixture.client ), 1 );
        fixture.client->state().getOriginalDb()->createBlockSnap( fixture.client->number() );
    };

    // With patch: expect 1 commit (one per block)
    skale::fs_commit_counter::reset();
    produceBlockWithFilestorageOperations();
    BOOST_REQUIRE_EQUAL( skale::fs_commit_counter::count(), 1 );
}
#endif

BOOST_AUTO_TEST_CASE( recalculateExternalGas ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    std::string chainID = "0x15";
    ret["params"]["chainID"] = chainID;

    // remove deployment control
    auto accounts = ret["accounts"];
    accounts.removeMember( "0xD2002000000000000000000000000000000000D2" );
    ret["accounts"] = accounts;

    // setup patch
    time_t externalGasPatchActivationTimestamp = time( nullptr ) + 10;
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
    std::string bytecode =
        "608060405234801561001057600080fd5b5061015e806100206000396000f3fe60806040523480156100105760"
        "0080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080"
        "fd5b610043610075565b60405161005091906100e7565b60405180910390f35b61007360048036038101906100"
        "6e91906100ab565b61007e565b005b60008054905090565b806000819055508060018190555080600281905550"
        "50565b6000813590506100a581610111565b92915050565b6000602082840312156100c1576100c061010c565b"
        "5b60006100cf84828501610096565b91505092915050565b6100e181610102565b82525050565b600060208201"
        "90506100fc60008301846100d8565b92915050565b6000819050919050565b600080fd5b61011a81610102565b"
        "811461012557600080fd5b5056fea2646970667358221220780703bb6ac2eec922a510d57edcae39b852b578e7"
        "f63a263ddb936758dc9c4264736f6c63430008070033";

    // deploy contact
    Json::Value create;
    create["from"] = senderAddress;
    create["code"] = bytecode;
    create["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    create["gas"] = 140000;
    create["nonce"] = 0;

    std::string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    fixture.client->state().getOriginalDb()->createBlockSnap( 2 );
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
    auto t = dev::eth::Transaction(
        ts, dev::Secret( "7be24de049f2d0d4ecaeaa81564aecf647fa7a4c86264243d77e01da25d859a0" ) );

    txHash = fixture.rpcClient->eth_sendRawTransaction( dev::toHex( t.toBytes() ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    fixture.client->state().getOriginalDb()->createBlockSnap( 3 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );


    BOOST_REQUIRE( receipt["status"].asString() == "0x0" );
    BOOST_REQUIRE( receipt["gasUsed"].asString() == "0x61cb" );


    sleep( 10 );

    // push new block to update timestamp
    Json::Value refill;
    refill["from"] = senderAddress;
    refill["to"] = dev::Address::random().hex();
    refill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    refill["value"] = 100;
    refill["nonce"] = 1;

    txHash = fixture.rpcClient->eth_sendTransaction( refill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    fixture.client->state().getOriginalDb()->createBlockSnap( 4 );

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
    t = dev::eth::Transaction(
        ts, dev::Secret( "8df08814fcfc169aad0015654114be06c28b27bdcdef286cf4dbd5e2950a3ffc" ) );

    txHash = fixture.rpcClient->eth_sendRawTransaction( dev::toHex( t.toBytes() ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    fixture.client->state().getOriginalDb()->createBlockSnap( 5 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );

    BOOST_REQUIRE( receipt["status"].asString() == "0x1" );
    BOOST_REQUIRE( receipt["gasUsed"].asString() == "0x13ef4" );
}
#endif

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
    auto t = dev::eth::Transaction(
        ts, dev::Secret( "08cee1f4bc8c37f88124bb3fc64566ccd35dbeeac84c62300f6b8809cab9ea2f" ) );

    txHash = fixture.rpcClient->eth_sendRawTransaction( dev::toHex( t.toBytes() ) );
    BOOST_REQUIRE( txHash == "0x95fb5557db8cc6de0aff3a64c18a6d9378b0d312b24f5d77e8dbf5cc0612d74f" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["gasUsed"].asString() == "0x5ac0" );
}

#ifndef FAIR
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

    string gasEstimateStr = fixture.rpcClient->eth_estimateGas( transact );
    u256 gasEstimate = jsToU256( gasEstimateStr );

    u256 powGasPrice = 0;
    do {
        const u256 GAS_PER_HASH = 1;
        u256 candidate = h256::random();
        h256 hash = dev::sha3( address2 ) ^ dev::sha3( u256( 0 ) ) ^ dev::sha3( candidate );
        u256 externalGas = ~u256( 0 ) / u256( hash ) * GAS_PER_HASH;
        if ( externalGas >= gasEstimate && externalGas < gasEstimate + gasEstimate / 10 ) {
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
#endif

BOOST_AUTO_TEST_CASE( eth_sendRawTransaction_gasPriceTooLow ) {
    JsonRpcFixture fixture;
    auto senderAddress = fixture.coinbase.address();
    auto receiver = KeyPair::create();

    // Mine to generate a non-zero account balance
    const int blocksToMine = 1;
#ifdef FAIR
    const u256 blockReward = fixture.client->chainParams().blockReward(
                fixture.client->blockChain().info().timestamp(),
                fixture.client->blockChain().info().number() );
#else
    const u256 blockReward = 2 * dev::eth::ether;
#endif
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
    }*/

    string bytecode =
        "6080604052348015600f57600080fd5b50609b8061001e6000396000f3fe608060405260015460001b60005460"
        "001b4360001b4360001b6040518082815260200191505060405180910390a36001600081548092919060010191"
        "90505550600a6001541415606357600060018190555060008081548092919060010191905055505b00fea26469"
        "70667358221220fdf2f98961b803b6b32dfc9be766990cbdb17559d9a03724d12fc672e33804b164736f6c6343"
        "00060c0033";

    Json::Value create;
    create["code"] = bytecode;
    create["gas"] = "180000";  // TODO or change global default of 90000?

    string deployHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value deployReceipt = fixture.rpcClient->eth_getTransactionReceipt( deployHash );
    string contractAddress = deployReceipt["contractAddress"].asString();

    for ( int i = 0; i <= 23; ++i ) {
        Json::Value t;
        t["from"] = toJS( fixture.coinbase.address() );
        t["value"] = jsToDecimal( "0" );
        t["to"] = contractAddress;
        t["gas"] = "99000";

#ifdef FAIR
        std::string txHash;
        if (i%2) {
            txHash = fixture.rpcClient->eth_sendTransaction( t );
        }
        else {
            std::string addrWithout0x = contractAddress.substr( 2 );
            dev::bytes encryptedData = formEncryptedMessageMockup( dev::bytes(), dev::Address( addrWithout0x ) );
            // account for the nonce 0 used for contract deployment
            size_t nonce = static_cast<size_t>(i + 1);
            std::string rlp = formTransactionRlp( fixture, t["from"].asString(),
                dev::toHex( encryptedData ), nonce, addrWithout0x);
            txHash = fixture.rpcClient->eth_sendRawTransaction( rlp );
        }
#else
        std::string txHash = fixture.rpcClient->eth_sendTransaction( t );
#endif
        BOOST_REQUIRE( !txHash.empty() );

        dev::eth::mineTransaction( *( fixture.client ), 1 );
        Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    }
    BOOST_REQUIRE_EQUAL( fixture.client->number(), 26 );  // block 1 - bootstrapAll, block 2 -
                                                          // deploy

    // ask for logs
    Json::Value t;
    t["fromBlock"] = 3;
    t["toBlock"] = 26;
    t["address"] = contractAddress;
    t["topics"] = Json::Value( Json::arrayValue );

    // 1 topics = []
    Json::Value logs = fixture.rpcClient->eth_getLogs( t );

    BOOST_REQUIRE( logs.isArray() );
    BOOST_REQUIRE_EQUAL( logs.size(), 24 );
    u256 t1 = dev::jsToU256( logs[12]["topics"][1].asString() );
    BOOST_REQUIRE_EQUAL( t1, 1 );
    u256 t2 = dev::jsToU256( logs[12]["topics"][2].asString() );
    BOOST_REQUIRE_EQUAL( t2, 2 );

    // 2 topics = [a]
    t["topics"] = Json::Value( Json::arrayValue );
    t["topics"][1] = u256_to_js( dev::u256( 2 ) );

    logs = fixture.rpcClient->eth_getLogs( t );

    BOOST_REQUIRE( logs.isArray() );
    BOOST_REQUIRE_EQUAL( logs.size(), 4 );
    t1 = dev::jsToU256( logs[0]["topics"][1].asString() );
    BOOST_REQUIRE_EQUAL( t1, 2 );
    t2 = dev::jsToU256( logs[0]["topics"][2].asString() );
    BOOST_REQUIRE_EQUAL( t2, 0 );

    // 3 topics = [null, a]
    t["topics"] = Json::Value( Json::arrayValue );
    t["topics"][2] = u256_to_js( dev::u256( 1 ) );  // 01,11,21 but not 1x

    logs = fixture.rpcClient->eth_getLogs( t );

    BOOST_REQUIRE( logs.isArray() );
    BOOST_REQUIRE_EQUAL( logs.size(), 3 );

    // 4 topics = [a,b]
    t["topics"] = Json::Value( Json::arrayValue );
    t["topics"][1] = u256_to_js( dev::u256( 1 ) );
    t["topics"][2] = u256_to_js( dev::u256( 2 ) );

    logs = fixture.rpcClient->eth_getLogs( t );

    BOOST_REQUIRE( logs.isArray() );
    BOOST_REQUIRE_EQUAL( logs.size(), 1 );

    // 5 topics = [[a,b]]
    t["topics"] = Json::Value( Json::arrayValue );
    t["topics"][1] = Json::Value( Json::arrayValue );
    t["topics"][1][0] = u256_to_js( dev::u256( 1 ) );
    t["topics"][1][1] = u256_to_js( dev::u256( 2 ) );

    logs = fixture.rpcClient->eth_getLogs( t );

    BOOST_REQUIRE( logs.isArray() );
    BOOST_REQUIRE_EQUAL( logs.size(), 10 + 4 );

    // 6 topics = [a,a]
    t["topics"] = Json::Value( Json::arrayValue );
    t["topics"][1] = u256_to_js( dev::u256( 1 ) );
    t["topics"][2] = u256_to_js( dev::u256( 1 ) );

    logs = fixture.rpcClient->eth_getLogs( t );

    BOOST_REQUIRE( logs.isArray() );
    BOOST_REQUIRE_EQUAL( logs.size(), 1 );

    // 7 topics = [[a,b], c]
    t["topics"] = Json::Value( Json::arrayValue );
    t["topics"][1] = Json::Value( Json::arrayValue );
    t["topics"][1][0] = u256_to_js( dev::u256( 1 ) );
    t["topics"][1][1] = u256_to_js( dev::u256( 2 ) );
    t["topics"][2] = u256_to_js( dev::u256( 1 ) );  // 11, 21

    logs = fixture.rpcClient->eth_getLogs( t );

    BOOST_REQUIRE( logs.isArray() );
    BOOST_REQUIRE_EQUAL( logs.size(), 2 );
    t1 = dev::jsToU256( logs[0]["topics"][1].asString() );
    BOOST_REQUIRE_EQUAL( t1, 1 );
    t2 = dev::jsToU256( logs[0]["topics"][2].asString() );
    BOOST_REQUIRE_EQUAL( t2, 1 );
    t1 = dev::jsToU256( logs[1]["topics"][1].asString() );
    BOOST_REQUIRE_EQUAL( t1, 2 );
    t2 = dev::jsToU256( logs[1]["topics"][2].asString() );
    BOOST_REQUIRE_EQUAL( t2, 1 );

    // 8 repeat #7 without address
    auto logs7 = logs;
    t["address"] = Json::Value( Json::arrayValue );
    logs = fixture.rpcClient->eth_getLogs( t );
    BOOST_REQUIRE_EQUAL( logs7, logs );

    // 9 repeat #7 with 2 addresses
    t["address"] = Json::Value( Json::arrayValue );
    t["address"][0] = contractAddress;
    t["address"][1] = "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba";  // dummy
    logs = fixture.rpcClient->eth_getLogs( t );
    BOOST_REQUIRE_EQUAL( logs7, logs );

    // 10 request address only
    t["topics"] = Json::Value( Json::arrayValue );
    logs = fixture.rpcClient->eth_getLogs( t );
    BOOST_REQUIRE( logs.isArray() );
    BOOST_REQUIRE_EQUAL( logs.size(), 24 );

    Json::Value filterReq;
    filterReq["fromBlock"] = 3;
    filterReq["toBlock"] = 26;
    filterReq["address"] = contractAddress;
    filterReq["topics"] = Json::Value( Json::arrayValue );
    std::string filterId = fixture.rpcClient->eth_newFilter( filterReq );
    Json::Value filterLogs = fixture.rpcClient->eth_getFilterLogs( filterId );
    BOOST_REQUIRE( filterLogs.isArray() );
    BOOST_REQUIRE_EQUAL( filterLogs.size(), logs.size() );
}

// limit on getLogs output
BOOST_AUTO_TEST_CASE( getLogs_limit ) {
    JsonRpcFixture fixture(
        "", true, true, false, false, false, -1, { { "getLogsBlocksLimit", "10" } } );

    dev::eth::simulateMining( *( fixture.client ), 100 );

    // push0Patch is enabled by default for FAIR
#ifndef FAIR
    // wait for push0Patch to be activated
    sleep(10);

    // update block timestamp to activate patch
    Json::Value txRefill;
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["from"] = toJS( fixture.coinbase.address() );
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#endif

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

    string bytecode =
        "6080604052348015600e575f80fd5b5060c080601a5f395ff3fe60806040525f5b6064811015604f577f907787"
        "67414a5c844b9d35a8745f67697ee3b8c2c3f4feafe5d9a3e234a5a3654382604051603d9291906067565b6040"
        "5180910390a18060010190506006565b005b5f819050919050565b6061816051565b82525050565b5f60408201"
        "905060785f830185605a565b60836020830184605a565b939250505056fea264697066735822122040208e35f2"
        "706dd92c17579466ab671c308efec51f558a755ea2cf81105ab22964736f6c63430008190033";

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
    t["gas"] = "999000";

    for ( int i = 0; i < 11; ++i ) {
        std::string txHash = fixture.rpcClient->eth_sendTransaction( t );
        BOOST_REQUIRE( !txHash.empty() );
        dev::eth::mineTransaction( *( fixture.client ), 1 );
        Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
        BOOST_REQUIRE_EQUAL( receipt["status"], "0x1" );
    }

    // ask for logs
    Json::Value req;
    req["fromBlock"] = 1;
    req["toBlock"] = 11;
    req["topics"] = Json::Value( Json::arrayValue );

    // 1 10 blocks
    BOOST_REQUIRE_NO_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ) );

    // 2 with topics
    req["address"] = contractAddress;
    BOOST_REQUIRE_NO_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ) );

    // 3 11 blocks
    req["toBlock"] = 12;
    BOOST_CHECK_EXCEPTION(
        ( fixture.rpcClient->eth_getLogs( req ) ),
        jsonrpc::JsonRpcException,
        []( const jsonrpc::JsonRpcException& ex ) {
            return ex.GetCode() == -32005 && std::string( ex.GetMessage() ).find( "Block range limit exceeded" ) !=
                   std::string::npos;
        } );

    // 4 filter
    string filterId = fixture.rpcClient->eth_newFilter( req );

    BOOST_REQUIRE_NO_THROW( Json::Value res = fixture.rpcClient->eth_getFilterChanges( filterId ) );

    req["toBlock"] = 2;
    filterId = fixture.rpcClient->eth_newFilter( req );
    BOOST_REQUIRE_NO_THROW( Json::Value res = fixture.rpcClient->eth_getFilterLogs( filterId ) );
    BOOST_REQUIRE_NO_THROW(
        Json::Value res2 = fixture.rpcClient->eth_getFilterChanges( filterId ) );

    req["toBlock"] = 50;
    filterId = fixture.rpcClient->eth_newFilter( req );
    BOOST_CHECK_EXCEPTION(
        ( fixture.rpcClient->eth_getFilterLogs( filterId ) ),
        jsonrpc::JsonRpcException,
        []( const jsonrpc::JsonRpcException& ex ) {
            std::cout << ex.GetCode() << " " << ex.GetMessage() << std::endl;
            return ex.GetCode() == -32005 && std::string( ex.GetMessage() ).find( "Block range limit exceeded" ) !=
                   std::string::npos;
        } );
}

BOOST_AUTO_TEST_CASE( getResponseLogCountLimit ) {
    JsonRpcFixture fixture(
        "", true, true, false, false, false, -1, { { "getResponseLogCountLimit", "201" } } );

    dev::eth::simulateMining( *( fixture.client ), 1 );

#ifndef FAIR
    sleep( 10 );
    Json::Value txRefill;
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["from"] = toJS( fixture.coinbase.address() );
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#endif

    string bytecode =
        "6080604052348015600e575f80fd5b5060c080601a5f395ff3fe60806040525f5b6064811015604f577f907787"
        "67414a5c844b9d35a8745f67697ee3b8c2c3f4feafe5d9a3e234a5a3654382604051603d9291906067565b6040"
        "5180910390a18060010190506006565b005b5f819050919050565b6061816051565b82525050565b5f60408201"
        "905060785f830185605a565b60836020830184605a565b939250505056fea264697066735822122040208e35f2"
        "706dd92c17579466ab671c308efec51f558a755ea2cf81105ab22964736f6c63430008190033";

    Json::Value create;
    create["code"] = bytecode;
    create["gas"] = "180000";
    string deployHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    Json::Value deployReceipt = fixture.rpcClient->eth_getTransactionReceipt( deployHash );
    string contractAddress = deployReceipt["contractAddress"].asString();

    Json::Value callTx;
    callTx["from"] = toJS( fixture.coinbase.address() );
    callTx["to"] = contractAddress;
    callTx["gas"] = "999000";

    for ( int i = 0; i < 5; ++i ) {
        string h = fixture.rpcClient->eth_sendTransaction( callTx );
        BOOST_REQUIRE( !h.empty() );
        dev::eth::mineTransaction( *( fixture.client ), 1 );
    }

    Json::Value req;
    req["fromBlock"] = 1;
    req["topics"] = Json::Value( Json::arrayValue );
    req["address"] = contractAddress;

    req["toBlock"] = 4;
    BOOST_REQUIRE_NO_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ) );

    string overHash = fixture.rpcClient->eth_sendTransaction( callTx );
    BOOST_REQUIRE( !overHash.empty() );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    req["toBlock"] = fixture.client->number();
    BOOST_CHECK_EXCEPTION(
        ( fixture.rpcClient->eth_getLogs( req ) ),
        jsonrpc::JsonRpcException,
        []( const jsonrpc::JsonRpcException& ex ) {
            return ex.GetCode() == -32005 && std::string( ex.GetMessage() ).find( "Response log count limit exceeded" ) !=
                   std::string::npos;
        } );
}

// test blockHash parameter
BOOST_AUTO_TEST_CASE( getLogs_blockHash ) {
    JsonRpcFixture fixture;
    dev::eth::simulateMining( *( fixture.client ), 1 );

    string latestHash =
        fixture.rpcClient->eth_getBlockByNumber( "latest", false )["hash"].asString();

    Json::Value req;
    req["blockHash"] = "xyz";
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ), std::exception );

    req["blockHash"] = Json::Value( Json::arrayValue );
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ), std::exception );

    req["fromBlock"] = 1;
    req["toBlock"] = 1;
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ), std::exception );

    req["blockHash"] = latestHash;
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ), std::exception );

    req.removeMember( "fromBlock" );
    req.removeMember( "toBlock" );
    BOOST_REQUIRE_NO_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ) );

    req["blockHash"] = "0x88df016429689c079f3b2f6ad39fa052532c56795b733da78a91ebe6a713944b";
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ), std::exception );

    req["blockHash"] = "";
    BOOST_REQUIRE_THROW( Json::Value logs = fixture.rpcClient->eth_getLogs( req ), std::exception );
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

    string bytecode =
        "608060405260006101005534801561001657600080fd5b50610104806100266000396000f3fe60806040523480"
        "15600f57600080fd5b506004361060325760003560e01c80636057361d146037578063c0fe1af8146062575b60"
        "0080fd5b606060048036036020811015604b57600080fd5b8101908080359060200190929190505050608d565b"
        "005b608b60048036036020811015607657600080fd5b810190808035906020019092919050505060b8565b005b"
        "806000610100546101008110609e57fe5b018190555060016101006000828254019250508190555050565b6000"
        "8082610100811060c657fe5b01819055505056fea26469706673582212206c8da972693a5b8c9bf59c197c4a0c"
        "554e9f51abd20047572c9c19125b533d2964736f6c634300060c0033";

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
    estimateGasCall["data"] =
        "0xc0fe1af80000000000000000000000000000000000000000000000000000000000000000";
    estimateGasCall["from"] = toJS( senderAddress );
    estimateGasCall["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    string estimatedGas = fixture.rpcClient->eth_estimateGas( estimateGasCall );

    dev::bytes data = dev::jsToBytes( estimateGasCall["data"].asString() );
    BOOST_REQUIRE(
        dev::jsToU256( estimatedGas ) >
        dev::eth::TransactionBase::baseGasRequired( false, &data,
            fixture.client->chainParams().makeEvmSchedule(
                fixture.client->latestBlock().info().timestamp(), fixture.client->number() ) ) );

    // try to send with this gas
    estimateGasCall["gas"] = toJS( jsToInt( estimatedGas ) );
    string clearHash = fixture.rpcClient->eth_sendTransaction( estimateGasCall );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    Json::Value clearReceipt = fixture.rpcClient->eth_getTransactionReceipt( clearHash );
    BOOST_REQUIRE_EQUAL( clearReceipt["status"], "0x1" );
    BOOST_REQUIRE_LT( jsToInt( clearReceipt["gasUsed"].asString() ), 21000 );

    // try to lower gas
    estimateGasCall["gas"] = toJS( jsToInt( estimatedGas ) - 1 );
    clearHash = fixture.rpcClient->eth_sendTransaction( estimateGasCall );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    clearReceipt = fixture.rpcClient->eth_getTransactionReceipt( clearHash );
    BOOST_REQUIRE_EQUAL( clearReceipt["status"], "0x0" );
    BOOST_REQUIRE_GT( jsToInt( clearReceipt["gasUsed"].asString() ), 21000 );
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

    std::string bytecode =
        "0x608060405234801561001057600080fd5b5061034f806100206000396000f300608060405260043610610083"
        "576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff1680630e"
        "031ab1146100885780631007f753146100c95780636057361d146100f6578063c298557814610123578063c67c"
        "d8841461013a578063d269ad4e14610167578063e0353e5914610194575b600080fd5b34801561009457600080"
        "fd5b506100b3600480360381019080803590602001909291905050506101c1565b604051808281526020019150"
        "5060405180910390f35b3480156100d557600080fd5b506100f460048036038101908080359060200190929190"
        "5050506101e4565b005b34801561010257600080fd5b5061012160048036038101908080359060200190929190"
        "505050610204565b005b34801561012f57600080fd5b50610138610233565b005b34801561014657600080fd5b"
        "506101656004803603810190808035906020019092919050505061026c565b005b34801561017357600080fd5b"
        "50610192600480360381019080803590602001909291905050506102a3565b005b3480156101a057600080fd5b"
        "506101bf60048036038101908080359060200190929190505050610302565b005b6000818154811015156101d0"
        "57fe5b906000526020600020016000915090505481565b6000818154811015156101f357fe5b90600052602060"
        "0020016000905550565b6000819080600181540180825580915050906001820390600052602060002001600090"
        "91929091909150555050565b600080805490509050600060019080600181540180825580915050906001820390"
        "60005260206000200160009091929091909150555050565b600081908060018154018082558091505090600182"
        "03906000526020600020016000909192909190915055506102a0610233565b50565b6001600082815481101515"
        "6102b457fe5b9060005260206000200181905550600080828154811015156102d257fe5b906000526020600020"
        "018190555060026000828154811015156102f157fe5b906000526020600020018190555050565b600080828154"
        "8110151561031257fe5b9060005260206000200181905550505600a165627a7a723058201ed095336772c55688"
        "864a6b45ca6ab89311c5533f8d38cdf931f1ce38be78080029";

    auto senderAddress = fixture.coinbase.address();

    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["data"] = bytecode;
    create["gas"] = "1800000";  // TODO or change global default of 90000?
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress = receipt["contractAddress"].asString();
#ifndef FAIR
    dev::Address contract = dev::Address( contractAddress );
#endif

    Json::Value txCall;  // call foo()
    txCall["to"] = contractAddress;
    txCall["data"] = "0xc2985578";
    txCall["from"] = toJS( senderAddress );
    txCall["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    auto callResult = fixture.rpcClient->eth_call( txCall, "latest" );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 0 );
#endif
    BOOST_REQUIRE_EQUAL( callResult, std::string( "0x" ) );

    Json::Value txPushValueAndCall;  // call storeAndCall(1)
    txPushValueAndCall["to"] = contractAddress;
    txPushValueAndCall["data"] =
        "0xc67cd8840000000000000000000000000000000000000000000000000000000000000001";
    txPushValueAndCall["from"] = toJS( senderAddress );
    txPushValueAndCall["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txPushValueAndCall );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 96 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txPushValue;  // call store(2)
    txPushValue["to"] = contractAddress;
    txPushValue["data"] =
        "0x6057361d0000000000000000000000000000000000000000000000000000000000000002";
    txPushValue["from"] = toJS( senderAddress );
    txPushValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txPushValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 128 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txThrow;  // trying to call store(3)
    txThrow["to"] = contractAddress;
    txThrow["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000003";
    txThrow["from"] = toJS( senderAddress );
    txThrow["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txThrow );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 128 );
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x0" );
#else
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );
#endif

    Json::Value txEraseValue;  // call erase(2)
    txEraseValue["to"] = contractAddress;
    txEraseValue["data"] =
        "0x1007f7530000000000000000000000000000000000000000000000000000000000000002";
    txEraseValue["from"] = toJS( senderAddress );
    txEraseValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txEraseValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 96 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );


    Json::Value txZeroValue;  // call zero(1)
    txZeroValue["to"] = contractAddress;
    txZeroValue["data"] =
        "0xe0353e590000000000000000000000000000000000000000000000000000000000000001";
    txZeroValue["from"] = toJS( senderAddress );
    txZeroValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txZeroValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 64 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txZeroValue1;  // call zero(1)
    txZeroValue1["to"] = contractAddress;
    txZeroValue1["data"] =
        "0xe0353e590000000000000000000000000000000000000000000000000000000000000001";
    txZeroValue1["from"] = toJS( senderAddress );
    txZeroValue1["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txZeroValue1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 64 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txValueChanged;  // call strangeFunction(1)
    txValueChanged["to"] = contractAddress;
    txValueChanged["data"] =
        "0xd269ad4e0000000000000000000000000000000000000000000000000000000000000001";
    txValueChanged["from"] = toJS( senderAddress );
    txValueChanged["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txValueChanged );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 96 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txValueChanged1;  // call strangeFunction(0)
    txValueChanged1["to"] = contractAddress;
    txValueChanged1["data"] =
        "0xd269ad4e0000000000000000000000000000000000000000000000000000000000000000";
    txValueChanged1["from"] = toJS( senderAddress );
    txValueChanged1["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txValueChanged1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 96 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txValueChanged2;  // call strangeFunction(2)
    txValueChanged2["to"] = contractAddress;
    txValueChanged2["data"] =
        "0xd269ad4e0000000000000000000000000000000000000000000000000000000000000002";
    txValueChanged2["from"] = toJS( senderAddress );
    txValueChanged2["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txValueChanged2 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 128 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txValueChanged3;  // try call strangeFunction(3)
    txValueChanged3["to"] = contractAddress;
    txValueChanged3["data"] =
        "0xd269ad4e0000000000000000000000000000000000000000000000000000000000000003";
    txValueChanged3["from"] = toJS( senderAddress );
    txValueChanged3["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txValueChanged3 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE(
        fixture.client->state().createReadOnlySnapBasedCopy().storageUsed( contract ) == 128 );
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x0" );
#else
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );
#endif
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
    std::string bytecode1 =
        "608060405234801561001057600080fd5b5061012c806100206000396000f3fe6080604052348015600f576000"
        "80fd5b5060043610604f576000357c010000000000000000000000000000000000000000000000000000000090"
        "0480631007f7531460545780636057361d14607f575b600080fd5b607d60048036036020811015606857600080"
        "fd5b810190808035906020019092919050505060aa565b005b60a860048036036020811015609357600080fd5b"
        "810190808035906020019092919050505060c7565b005b6000818154811060b657fe5b90600052602060002001"
        "6000905550565b6000600282019080600181540180825580915050600190039060005260206000200160009091"
        "9091909150555056fea264697066735822122055c65b9e093cdb44864dac3fb79ec15a542db86c2f897b938043"
        "d8e15468ca4464736f6c63430006060033";

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
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    // call store(2)
    txStore["to"] = contractAddress1;
    txStore["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000002";
    txStore["from"] = toJS( senderAddress );
    txStore["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStore );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 96 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txErase;  // call erase(1)
    txErase["to"] = contractAddress1;
    txErase["data"] = "0x1007f7530000000000000000000000000000000000000000000000000000000000000001";
    txErase["from"] = toJS( senderAddress );
    txErase["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txErase );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

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
    std::string bytecode2 =
        "608060405234801561001057600080fd5b50610129806100206000396000f3fe6080604052348015600f576000"
        "80fd5b5060043610604f576000357c010000000000000000000000000000000000000000000000000000000090"
        "0480631007f7531460545780636057361d14607f575b600080fd5b607d60048036036020811015606857600080"
        "fd5b810190808035906020019092919050505060aa565b005b60a860048036036020811015609357600080fd5b"
        "810190808035906020019092919050505060c7565b005b6000818154811060b657fe5b90600052602060002001"
        "6000905550565b6000819080600181540180825580915050600190039060005260206000200160009091909190"
        "9150555056fea26469706673582212202bfb5f6fb63ae4f1c9a362ed3f7de7aa5514029db925efa368e711e35d"
        "9ebc0a64736f6c63430006060033";

    Json::Value create2;
    create2["from"] = toJS( senderAddress );
    create2["data"] = bytecode2;
    create2["gas"] = "1800000";
    txHash = fixture.rpcClient->eth_sendTransaction( create2 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt2 = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    string contractAddress2 = receipt2["contractAddress"].asString();

    Json::Value txStoreSecondContract;  // call store(1)
    txStoreSecondContract["to"] = contractAddress2;
    txStoreSecondContract["data"] =
        "0x6057361d0000000000000000000000000000000000000000000000000000000000000001";
    txStoreSecondContract["from"] = toJS( senderAddress );
    txStoreSecondContract["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStoreSecondContract );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 128 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    // try call store(2) to second contract
    txStoreSecondContract["to"] = contractAddress2;
    txStoreSecondContract["data"] =
        "0x6057361d0000000000000000000000000000000000000000000000000000000000000002";
    txStoreSecondContract["from"] = toJS( senderAddress );
    txStoreSecondContract["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStoreSecondContract );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 128 );
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x0" );
#else
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );
#endif

    // try call store(3) to first contract
    txStore["to"] = contractAddress1;
    txStore["data"] = "0x6057361d0000000000000000000000000000000000000000000000000000000000000001";
    txStore["from"] = toJS( senderAddress );
    txStore["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStore );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 128 );
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x0" );
#else
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );
#endif

    Json::Value txZeroValue;  // call zero(1)
    txZeroValue["to"] = contractAddress1;
    txZeroValue["data"] =
        "0x1007f7530000000000000000000000000000000000000000000000000000000000000000";
    txZeroValue["from"] = toJS( senderAddress );
    txZeroValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txZeroValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 96 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );
}

BOOST_AUTO_TEST_CASE( storage_limit_predeployed ) {
    JsonRpcFixture fixture( c_genesisConfigString );
    dev::eth::simulateMining( *( fixture.client ), 20 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64 );
#endif

    string contractAddress = "0xC2002000000000000000000000000000000000C2";
    string senderAddress = toJS( fixture.coinbase.address() );

    Json::Value txChangeInt;
    txChangeInt["to"] = contractAddress;
    txChangeInt["data"] =
        "0xcd16ecbf0000000000000000000000000000000000000000000000000000000000000002";
    txChangeInt["from"] = senderAddress;
    txChangeInt["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    string txHash = fixture.rpcClient->eth_sendTransaction( txChangeInt );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txZeroValue;
    txZeroValue["to"] = contractAddress;
    txZeroValue["data"] =
        "0xcd16ecbf0000000000000000000000000000000000000000000000000000000000000000";
    txZeroValue["from"] = senderAddress;
    txZeroValue["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txZeroValue );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 32 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );

    Json::Value txChangeInt1;
    txChangeInt["to"] = contractAddress;
    txChangeInt["data"] =
        "0x9b0631040000000000000000000000000000000000000000000000000000000000000001";
    txChangeInt["from"] = senderAddress;
    txChangeInt["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txChangeInt );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == 64 );
#endif
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"], "0x1" );
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
    std::string bytecode1 =
        "0x608060405234801561001057600080fd5b50610134806100206000396000f3fe6080604052348015600f5760"
        "0080fd5b506004361060285760003560e01c80636ed28ed014602d575b600080fd5b6043600480360381019060"
        "3f91906096565b6045565b005b81600082600a8110605757605660cf565b5b01819055505050565b600080fd5b"
        "6000819050919050565b6076816065565b8114608057600080fd5b50565b600081359050609081606f565b9291"
        "5050565b6000806040838503121560aa5760a96060565b5b600060b6858286016083565b925050602060c58582"
        "86016083565b9150509250929050565b7f4e487b71000000000000000000000000000000000000000000000000"
        "00000000600052603260045260246000fdfea2646970667358221220ec8739ad7fc74a76053c683510b3c836d0"
        "1c7eda3687d89e65380260a97e741b64736f6c63430008120033";
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

    std::string bytecode2 =
        "608060405234801561001057600080fd5b506040516106f93803806106f9833981810160405281019061003291"
        "906100dc565b80600260006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373"
        "ffffffffffffffffffffffffffffffffffffffff16021790555050610109565b600080fd5b600073ffffffffff"
        "ffffffffffffffffffffffffffffff82169050919050565b60006100a98261007e565b9050919050565b6100b9"
        "8161009e565b81146100c457600080fd5b50565b6000815190506100d6816100b0565b92915050565b60006020"
        "82840312156100f2576100f1610079565b5b6000610100848285016100c7565b91505092915050565b6105e180"
        "6101186000396000f3fe608060405234801561001057600080fd5b506004361061002b5760003560e01c8063c1"
        "8829ca14610030575b600080fd5b61003861003a565b005b6001808190555060016000806101000a81548160ff"
        "021916908315150217905550600260009054906101000a900473ffffffffffffffffffffffffffffffffffffff"
        "ff1673ffffffffffffffffffffffffffffffffffffffff16636ed28ed0600a806040518363ffffffff1660e01b"
        "81526004016100b99291906102de565b600060405180830381600087803b1580156100d357600080fd5b505af1"
        "9250505080156100e4575060015b610235576100f0610314565b806308c379a00361014c57506101046103b156"
        "5b8061010f57506101c5565b7f51a7f65c6325882f237d4aeb43228179cfad48b868511d508e24b4437a819137"
        "8160405161013e91906104c0565b60405180910390a150610230565b634e487b71036101c55761015e6104e256"
        "5b9061016957506101c5565b7f51a7f65c6325882f237d4aeb43228179cfad48b868511d508e24b4437a819137"
        "8160405160200161019b9190610524565b6040516020818303038152906040526040516101b791906104c0565b"
        "60405180910390a150610230565b3d80600081146101f1576040519150601f19603f3d011682016040523d8252"
        "3d6000602084013e6101f6565b606091505b507f51a7f65c6325882f237d4aeb43228179cfad48b868511d508e"
        "24b4437a8191378160405161022691906104c0565b60405180910390a1505b61026b565b7f51a7f65c6325882f"
        "237d4aeb43228179cfad48b868511d508e24b4437a8191376040516102629061058b565b60405180910390a15b"
        "600060018190555060008060006101000a81548160ff021916908315150217905550565b600081905091905056"
        "5b6000819050919050565b6000819050919050565b60006102c86102c36102be8461028f565b6102a3565b6102"
        "99565b9050919050565b6102d8816102ad565b82525050565b60006040820190506102f360008301856102cf56"
        "5b61030060208301846102cf565b9392505050565b60008160e01c9050919050565b600060033d111561033357"
        "60046000803e610330600051610307565b90505b90565b6000604051905090565b6000601f19601f8301169050"
        "919050565b7f4e487b710000000000000000000000000000000000000000000000000000000060005260416004"
        "5260246000fd5b61038982610340565b810181811067ffffffffffffffff821117156103a8576103a761035156"
        "5b5b80604052505050565b600060443d1061043e576103c3610336565b60043d036004823e80513d6024820111"
        "67ffffffffffffffff821117156103eb57505061043e565b808201805167ffffffffffffffff81111561040957"
        "5050505061043e565b80602083010160043d03850181111561042657505050505061043e565b61043582602001"
        "850186610380565b82955050505050505b90565b600081519050919050565b6000828252602082019050929150"
        "50565b60005b8381101561047b578082015181840152602081019050610460565b60008484015250505050565b"
        "600061049282610441565b61049c818561044c565b93506104ac81856020860161045d565b6104b58161034056"
        "5b840191505092915050565b600060208201905081810360008301526104da8184610487565b90509291505056"
        "5b60008060233d11156104ff576020600460003e6001915060005190505b9091565b6000819050919050565b61"
        "051e61051982610299565b610503565b82525050565b6000610530828461050d565b6020820191508190509291"
        "5050565b7f7472756500000000000000000000000000000000000000000000000000000000600082015250565b"
        "600061057560048361044c565b91506105808261053f565b602082019050919050565b60006020820190508181"
        "0360008301526105a481610568565b905091905056fea26469706673582212201a522ad11a321603efd182e33e"
        "10b59f65b8c9a8b84c8ec3d832ff1d0b726cc564736f6c63430008120033" +
        std::string( "000000000000000000000000" ) + contractAddress1.substr( 2 );
    Json::Value create2;
    create2["from"] = toJS( senderAddress );
    create2["data"] = bytecode2;
    create2["gas"] = "1800000";
    txHash = fixture.rpcClient->eth_sendTransaction( create2 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt2 = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt2["status"] == string( "0x1" ) );
    string contractAddress2 = receipt2["contractAddress"].asString();

#ifndef FAIR
    auto storageUsed = fixture.client->state().storageUsedTotal();
#endif

    Json::Value txStoreTry;
    txStoreTry["to"] = contractAddress2;
    txStoreTry["data"] = "0xc18829ca";
    txStoreTry["from"] = senderAddress.hex();
    txStoreTry["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txStoreTry );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
#ifndef FAIR
    BOOST_REQUIRE( fixture.client->state().storageUsedTotal() == storageUsed );
#endif

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["gasUsed"] != "0x0" );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
}

BOOST_AUTO_TEST_CASE( setSchainExitTime ) {
    JsonRpcFixture fixture;
    Json::Value requestJson;
    requestJson["finishTime"] = 100;
    BOOST_REQUIRE_THROW(
        fixture.rpcClient->setSchainExitTime( requestJson ), jsonrpc::JsonRpcException );
}

#ifndef FAIR
/*
BOOST_AUTO_TEST_CASE( oracle, *boost::unit_test::disabled() ) {

    JsonRpcFixture fixture;
    std::string receipt;
    std::string result;
    std::time_t current = std::time(nullptr);
    std::string request;
    for (int i = 0; i < 1000000; ++i) {
        request =
skutils::tools::format("{\"cid\":1,\"uri\":\"http://worldtimeapi.org/api/timezone/Europe/Kiev\",\"jsps\":[\"/unixtime\",\"/day_of_year\",\"/xxx\"],\"trims\":[1,1,1],\"time\":%zu000,\"pow\":%zu}",
current, i); auto os = make_shared<OracleRequestSpec>(request); if ( os->verifyPow() ) { break;
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
#endif

BOOST_AUTO_TEST_CASE( doDbCompactionDebugCall ) {
    JsonRpcFixture fixture;

    fixture.rpcClient->debug_doStateDbCompaction();

    fixture.rpcClient->debug_doBlocksDbCompaction();
}

BOOST_AUTO_TEST_CASE( debugGetPatchTimestamps ) {
    Json::Value configJson;
    Json::Reader().parse( c_genesisConfigString, configJson );

    // indexed by enum int value
    std::vector< size_t > patchTimestamps;

    // Set custom config file & create timestamps for each patch
    size_t numPatches = static_cast< size_t >( SchainPatchEnum::PatchesCount );
    for ( size_t patch = 0; patch < numPatches; patch++ ) {
        SchainPatchEnum patchEnum = static_cast< SchainPatchEnum >( patch );
        size_t ts = patch + 1000;  // just to offset from the default values (0, 1)
        patchTimestamps.push_back( ts );

        std::string patchName = getPatchNameForEnum( patchEnum ) + "Timestamp";
        patchName[0] = tolower( patchName[0] );
        configJson["skaleConfig"]["sChain"][patchName] = ts;
    }

    Json::FastWriter fastWriter;
    std::string customConfigFile = fastWriter.write( configJson );

    JsonRpcFixture fixture( customConfigFile, false, false, false, false );
    Json::Value returnedPatchTimestamps = fixture.rpcClient->debug_getPatchTimestamps();

    // compare returned timestamps to actual timestamps
    for ( size_t patchIdx = 0; patchIdx < numPatches; patchIdx++ ) {
        SchainPatchEnum patchEnum = static_cast< SchainPatchEnum >( patchIdx );

        std::string patchName = getPatchNameForEnum( patchEnum ) + "Timestamp";
        patchName[0] = tolower( patchName[0] );
        size_t returnedTimestamp =
            static_cast< size_t >( returnedPatchTimestamps[patchName].asInt() );

        BOOST_REQUIRE_EQUAL( returnedTimestamp, patchTimestamps[patchIdx] );
    }
}

#ifndef FAIR
BOOST_AUTO_TEST_CASE( powTxnGasLimit ) {
    Json::Value configJson;
    Json::Reader().parse( c_genesisConfigString, configJson );
    configJson["skaleConfig"]["sChain"]["powCheckPatchTimestamp"] = 1;
    Json::FastWriter fastWriter;
    std::string customConfigFile = fastWriter.write( configJson );
    JsonRpcFixture fixture( customConfigFile, false, false, true, false );

    // mine blocks without transactions
    dev::eth::simulateMining( *( fixture.client ), 2000000 );

    string senderAddress = toJS( fixture.coinbase.address() );

    Json::Value txPOW1;
    txPOW1["to"] = "0x0000000000000000000000000000000000000033";
    txPOW1["from"] = senderAddress;
    txPOW1["gas"] = "100000";
    txPOW1["gasPrice"] =
        "0xa449dcaf2bca14e6bd0ac650eed9555008363002b2fc3a4c8422b7a9525a8135";  // gas 200k
    txPOW1["value"] = 1;
    string txHash = fixture.rpcClient->eth_sendTransaction( txPOW1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt1 = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt1["status"] == string( "0x1" ) );

    Json::Value txPOW2;
    txPOW2["to"] = "0x0000000000000000000000000000000000000033";
    txPOW2["from"] = senderAddress;
    txPOW2["gas"] = "100000";
    txPOW2["gasPrice"] =
        "0xc5002ab03e1e7e196b3d0ffa9801e783fcd48d4c6d972f1389ab63f4e2d0bef0";  // gas 1m
    txPOW2["value"] = 100;

    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendTransaction( txPOW2 ),
        jsonrpc::JsonRpcException );  // block gas limit reached
}
#endif

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


    std::array< Json::Value, 4 > wellFormedCalls = { eip1898WellFormed, eip1898WellFormed1,
        eip1898WellFormed2, eip1898WellFormed3 };
    std::array< Json::Value, 6 > badFormedCalls = { eip1898BadFormed, eip1898BadFormed1,
        eip1898BadFormed2, eip1898BadFormed3, eip1898BadFormed4, eip1898BadFormed5 };


    auto address = fixture.coinbase.address();

    std::string response;
    for ( const auto& call : wellFormedCalls ) {
        BOOST_REQUIRE_NO_THROW( fixture.rpcClient->eth_getBalanceEIP1898( toJS( address ), call ) );
    }

    for ( const auto& call : badFormedCalls ) {
        BOOST_REQUIRE_THROW( fixture.rpcClient->eth_getBalanceEIP1898( toJS( address ), call ),
            jsonrpc::JsonRpcException );
    }


    for ( const auto& call : wellFormedCalls ) {
        Json::Value transactionCallObject;
        transactionCallObject["to"] = "0x0000000000000000000000000000000000000005";
        transactionCallObject["data"] = "0x0000000000000000000000000000000000000005";
        BOOST_REQUIRE_NO_THROW( fixture.rpcClient->eth_callEIP1898( transactionCallObject, call ) );
    }

    for ( const auto& call : badFormedCalls ) {
        Json::Value transactionCallObject;
        transactionCallObject["to"] = "0x0000000000000000000000000000000000000005";
        transactionCallObject["data"] = "0x0000000000000000000000000000000000000005";
        BOOST_REQUIRE_THROW( fixture.rpcClient->eth_callEIP1898( transactionCallObject, call ),
            jsonrpc::JsonRpcException );
    }

    for ( const auto& call : wellFormedCalls ) {
        BOOST_REQUIRE_NO_THROW( fixture.rpcClient->eth_getCodeEIP1898( toJS( address ), call ) );
    }

    for ( const auto& call : badFormedCalls ) {
        BOOST_REQUIRE_THROW( fixture.rpcClient->eth_getCodeEIP1898( toJS( address ), call ),
            jsonrpc::JsonRpcException );
    }

    for ( const auto& call : wellFormedCalls ) {
        BOOST_REQUIRE_NO_THROW(
            fixture.rpcClient->eth_getStorageAtEIP1898( toJS( address ), toJS( address ), call ) );
    }

    for ( const auto& call : badFormedCalls ) {
        BOOST_REQUIRE_THROW(
            fixture.rpcClient->eth_getStorageAtEIP1898( toJS( address ), toJS( address ), call ),
            jsonrpc::JsonRpcException );
    }

    for ( const auto& call : wellFormedCalls ) {
        BOOST_REQUIRE_NO_THROW(
            fixture.rpcClient->eth_getTransactionCountEIP1898( toJS( address ), call ) );
    }

    for ( const auto& call : badFormedCalls ) {
        BOOST_REQUIRE_THROW(
            fixture.rpcClient->eth_getTransactionCountEIP1898( toJS( address ), call ),
            jsonrpc::JsonRpcException );
    }
}

BOOST_AUTO_TEST_CASE( eip2930Transactions ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 151
    std::string chainID = "0x97";
    ret["params"]["chainID"] = chainID;
    time_t eip1559PatchActivationTimestamp = time( nullptr ) + 10;
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] =
        eip1559PatchActivationTimestamp;


    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS( fixture.coinbase.address() );

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

    BOOST_REQUIRE( fixture.rpcClient->eth_getBalance( "0xc868AF52a6549c773082A334E5AE232e0Ea3B513",
                       "latest" ) == "0x16345785d8a0000" );

#ifndef FAIR
    // try sending type1 txn before patchTimestmap
    BOOST_REQUIRE_THROW(
        fixture.rpcClient->eth_sendRawTransaction(
            "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180c001a01e"
            "bdc546c8b85511b7ba831f47c4981069d7af972d10b7dce2c57225cb5df6a7a055ae1e84fea41d37589eb7"
            "40a0a93017a5cd0e9f10ee50f165bf4b1b4c78ddae" ),
        jsonrpc::JsonRpcException );  // INVALID_PARAMS
    sleep( 10 );
#endif

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

    // send 1 WEI from 0xc868AF52a6549c773082A334E5AE232e0Ea3B513 to
    // 0x7D36aF85A184E220A656525fcBb9A63B9ab3C12b encoded type 1 txn
    txHash = fixture.rpcClient->eth_sendRawTransaction(
        "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180c001a01ebdc5"
        "46c8b85511b7ba831f47c4981069d7af972d10b7dce2c57225cb5df6a7a055ae1e84fea41d37589eb740a0a930"
        "17a5cd0e9f10ee50f165bf4b1b4c78ddae" );
    auto pendingTransactions = fixture.rpcClient->eth_pendingTransactions();
    BOOST_REQUIRE( pendingTransactions.isArray() && pendingTransactions.size() == 1 );
    BOOST_REQUIRE( pendingTransactions[0]["type"] == "0x1" );
    BOOST_REQUIRE( pendingTransactions[0].isMember( "yParity" ) &&
                   pendingTransactions[0].isMember( "accessList" ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // compare with txn hash from geth
    BOOST_REQUIRE( txHash == "0xc843560015a655b8f81f65a458be9019bdb5cd8e416b6329ca18f36de0b8244d" );

    BOOST_REQUIRE( dev::toHexPrefixed( fixture.client->transactions( 4 )[0].toBytes() ) ==
                   "0x01f8678197808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180c"
                   "001a01ebdc546c8b85511b7ba831f47c4981069d7af972d10b7dce2c57225cb5df6a7a055ae1e84"
                   "fea41d37589eb740a0a93017a5cd0e9f10ee50f165bf4b1b4c78ddae" );

    BOOST_REQUIRE( fixture.rpcClient->eth_getBalance(
                       "0x7D36aF85A184E220A656525fcBb9A63B9ab3C12b", "latest" ) == "0x1" );

    auto block = fixture.rpcClient->eth_getBlockByNumber( "4", false );
    BOOST_REQUIRE( block["transactions"].size() == 1 );
    BOOST_REQUIRE( block["transactions"][0].asString() == txHash );

    block = fixture.rpcClient->eth_getBlockByNumber( "4", true );
    BOOST_REQUIRE( block["transactions"].size() == 1 );
    BOOST_REQUIRE( block["transactions"][0]["hash"].asString() == txHash );
    BOOST_REQUIRE( block["transactions"][0]["type"] == "0x1" );

    BOOST_REQUIRE( block["transactions"][0]["yParity"].asString() ==
                   block["transactions"][0]["v"].asString() );

    BOOST_REQUIRE( block["transactions"][0]["accessList"].isArray() );
    BOOST_REQUIRE( block["transactions"][0]["accessList"].size() == 0 );
    BOOST_REQUIRE( block["transactions"][0].isMember( "chainId" ) );
    BOOST_REQUIRE( block["transactions"][0]["chainId"].asString() == chainID );

    std::string blockHash = block["hash"].asString();
    BOOST_REQUIRE(
        fixture.client->transactionHashes( dev::h256( blockHash ) )[0] ==
        dev::h256( "0xc843560015a655b8f81f65a458be9019bdb5cd8e416b6329ca18f36de0b8244d" ) );

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
    // [ { 'address': HexBytes( "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae" ), 'storageKeys': (
    // "0x0000000000000000000000000000000000000000000000000000000000000003",
    // "0x0000000000000000000000000000000000000000000000000000000000000007" ) } ]
    txHash = fixture.rpcClient->eth_sendRawTransaction(
        "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b0"
        "3eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c"
        "07b6ff742311b04dab760bb3bc206054332879" );
    pendingTransactions = fixture.rpcClient->eth_pendingTransactions();
    BOOST_REQUIRE( pendingTransactions.isArray() && pendingTransactions.size() == 1 );
    BOOST_REQUIRE( pendingTransactions[0]["type"] == "0x1" );
    BOOST_REQUIRE( pendingTransactions[0].isMember( "yParity" ) &&
                   pendingTransactions[0].isMember( "accessList" ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // compare with txn hash from geth
    BOOST_REQUIRE( txHash == "0xa6d3541e06dff71fb8344a4db2a4ad4e0b45024eb23a8f568982b70a5f50f94d" );
    BOOST_REQUIRE(
        dev::toHexPrefixed( fixture.client->transactions( 5 )[0].toBytes() ) ==
        "0x01f8c38197018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000780a0b0"
        "3eaf481958e22fc39bd1d526eb9255be1e6625614f02ca939e51c3d7e64bcaa05f675640c04bb050d27bd1f39c"
        "07b6ff742311b04dab760bb3bc206054332879" );

    result = fixture.rpcClient->eth_getTransactionByHash( txHash );
    BOOST_REQUIRE( result["type"] == "0x1" );
    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["accessList"].size() == 1 );
    BOOST_REQUIRE( result["accessList"][0].isObject() &&
                   result["accessList"][0].getMemberNames().size() == 2 );
    BOOST_REQUIRE( result["accessList"][0].isMember( "address" ) &&
                   result["accessList"][0].isMember( "storageKeys" ) );
    BOOST_REQUIRE( result["accessList"][0]["address"].asString() ==
                   "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae" );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"].isArray() &&
                   result["accessList"][0]["storageKeys"].size() == 2 );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"][0].asString() ==
                   "0x0000000000000000000000000000000000000000000000000000000000000003" );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"][1].asString() ==
                   "0x0000000000000000000000000000000000000000000000000000000000000007" );

    block = fixture.rpcClient->eth_getBlockByNumber( "5", true );
    result = block["transactions"][0];
    BOOST_REQUIRE( result["type"] == "0x1" );
    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["accessList"].size() == 1 );
    BOOST_REQUIRE( result["accessList"][0].isObject() &&
                   result["accessList"][0].getMemberNames().size() == 2 );
    BOOST_REQUIRE( result["accessList"][0].isMember( "address" ) &&
                   result["accessList"][0].isMember( "storageKeys" ) );
    BOOST_REQUIRE( result["accessList"][0]["address"].asString() ==
                   "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae" );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"].isArray() &&
                   result["accessList"][0]["storageKeys"].size() == 2 );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"][0].asString() ==
                   "0x0000000000000000000000000000000000000000000000000000000000000003" );
    BOOST_REQUIRE( result["accessList"][0]["storageKeys"][1].asString() ==
                   "0x0000000000000000000000000000000000000000000000000000000000000007" );
}

BOOST_AUTO_TEST_CASE( eip1559Transactions ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 151
    std::string chainID = "0x97";
    ret["params"]["chainID"] = chainID;
    time_t eip1559PatchActivationTimestamp = time( nullptr ) + 10;
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] =
        eip1559PatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    fixture.client->state().getOriginalDb()->createBlockSnap( 2 );
    string senderAddress = toJS( fixture.coinbase.address() );

    Json::Value txRefill;
    txRefill["to"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    fixture.client->state().getOriginalDb()->createBlockSnap( 3 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x0" );

    auto result = fixture.rpcClient->eth_getTransactionByHash( txHash );
    BOOST_REQUIRE( result["type"] == "0x0" );
    BOOST_REQUIRE( !result.isMember( "yParity" ) );
    BOOST_REQUIRE( !result.isMember( "accessList" ) );

    BOOST_REQUIRE( fixture.rpcClient->eth_getBalance( "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251",
                       "latest" ) == "0x16345785d8a0000" );

#ifndef FAIR
    // try sending type2 txn before patchTimestmap
    BOOST_REQUIRE_THROW(
        fixture.rpcClient->eth_sendRawTransaction(
            "0x02f8c98197808504a817c8018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b"
            "0180f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a000000000000000000000000000"
            "00000000000000000000000000000000000003a00000000000000000000000000000000000000000000000"
            "00000000000000000701a005bd1eedc509a8e94cfcfc84d0b5fd53a0888a475274cbeee321047da5d139f8"
            "a00e7f0dd8b5277766d447ea51b7d8f571dc8bb57ff95c068c58f5b6fe9089dde8" ),
        jsonrpc::JsonRpcException );  // INVALID_PARAMS
    sleep( 10 );
#endif

    // force 1 block to update timestamp
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 0;
    txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    fixture.client->state().getOriginalDb()->createBlockSnap( 4 );
    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x0" );
    BOOST_REQUIRE( receipt["effectiveGasPrice"] == "0x4a817c800" );


    // send 1 WEI from 0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251 to
    // 0x7D36aF85A184E220A656525fcBb9A63B9ab3C12b encoded type 2 txn
    txHash = fixture.rpcClient->eth_sendRawTransaction(
        "0x02f8c98197808504a817c8008504a817c801827530947d36af85a184e220a656525fcbb9a63b9ab3c12b018"
        "0f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a000000000000000000000000000000000"
        "00000000000000000000000000000003a00000000000000000000000000000000000000000000000000000000"
        "00000000780a0f000a16aeee9ac96602e8b179d39caca61f347a296e78ec9c57ec06c6e378422a02462cfad48"
        "32a32ed8c55fddf8a033d4a73f62fed993de3687ba14902eaea2d4" );

    auto pendingTransactions = fixture.rpcClient->eth_pendingTransactions();
    BOOST_REQUIRE( pendingTransactions.isArray() && pendingTransactions.size() == 1 );
    BOOST_REQUIRE( pendingTransactions[0]["type"] == "0x2" );
    BOOST_REQUIRE( pendingTransactions[0].isMember( "yParity" ) &&
                   pendingTransactions[0].isMember( "accessList" ) );
    BOOST_REQUIRE( pendingTransactions[0].isMember( "maxFeePerGas" ) &&
                   pendingTransactions[0].isMember( "maxPriorityFeePerGas" ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // compare with txn hash from geth
    BOOST_REQUIRE( txHash == "0xd9463624627c7add3b3ac18a228569b3a9075cc05bec64db060ea8904f3c4288" );
    BOOST_REQUIRE(
        dev::toHexPrefixed( fixture.client->transactions( 4 )[0].toBytes() ) ==
            "0x02f8c98197808504a817c8008504a817c801827530947d36af85a184e220a656525fcbb9a63b9ab3c12b018"
            "0f85bf85994de0b295669a9fd93d5f28d9ec85e40f4cb697baef842a000000000000000000000000000000000"
            "00000000000000000000000000000003a00000000000000000000000000000000000000000000000000000000"
            "00000000780a0f000a16aeee9ac96602e8b179d39caca61f347a296e78ec9c57ec06c6e378422a02462cfad48"
            "32a32ed8c55fddf8a033d4a73f62fed993de3687ba14902eaea2d4" );

    BOOST_REQUIRE( fixture.rpcClient->eth_getBalance(
                       "0x7D36aF85A184E220A656525fcBb9A63B9ab3C12b", "latest" ) == "0x1" );

    auto block = fixture.rpcClient->eth_getBlockByNumber( "4", false );
    BOOST_REQUIRE( block["transactions"].size() == 1 );
    BOOST_REQUIRE( block["transactions"][0].asString() == txHash );

    block = fixture.rpcClient->eth_getBlockByNumber( "4", true );
    BOOST_REQUIRE( !block["baseFeePerGas"].asString().empty() );
    BOOST_REQUIRE( block["transactions"].size() == 1 );
    BOOST_REQUIRE( block["transactions"][0]["hash"].asString() == txHash );
    BOOST_REQUIRE( block["transactions"][0]["type"] == "0x2" );

    BOOST_REQUIRE( block["transactions"][0]["yParity"].asString() ==
                   block["transactions"][0]["v"].asString() );

    BOOST_REQUIRE( block["transactions"][0]["accessList"].isArray() );
    BOOST_REQUIRE( block["transactions"][0].isMember( "chainId" ) );
    BOOST_REQUIRE( block["transactions"][0]["chainId"].asString() == chainID );

    std::string blockHash = block["hash"].asString();

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    BOOST_REQUIRE( receipt["type"] == "0x2" );
    BOOST_REQUIRE( receipt["effectiveGasPrice"] == "0x4a817c801" );

    result = fixture.rpcClient->eth_getTransactionByHash( txHash );
    BOOST_REQUIRE( result["hash"].asString() == txHash );
    BOOST_REQUIRE( result["type"] == "0x2" );

    BOOST_REQUIRE( result["yParity"].asString() == result["v"].asString() );

    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE(
        result.isMember( "maxPriorityFeePerGas" ) && result["maxPriorityFeePerGas"].isString() );
    BOOST_REQUIRE( result.isMember( "maxFeePerGas" ) && result["maxFeePerGas"].isString() );
    BOOST_REQUIRE( result["maxPriorityFeePerGas"] == "0x4a817c800" );
    BOOST_REQUIRE( result["maxFeePerGas"] == "0x4a817c801" );

    result = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex( blockHash, "0x0" );
    BOOST_REQUIRE( result["hash"].asString() == txHash );
    BOOST_REQUIRE( result["type"] == "0x2" );

    BOOST_REQUIRE( result["yParity"].asString() == result["v"].asString() );

    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["maxPriorityFeePerGas"] == "0x4a817c800" );
    BOOST_REQUIRE( result["maxFeePerGas"] == "0x4a817c801" );

    result = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex( "0x4", "0x0" );
    BOOST_REQUIRE( result["hash"].asString() == txHash );
    BOOST_REQUIRE( result["type"] == "0x2" );

    BOOST_REQUIRE( result["yParity"].asString() == result["v"].asString() );

    BOOST_REQUIRE( result["accessList"].isArray() );
    BOOST_REQUIRE( result["maxPriorityFeePerGas"] == "0x4a817c800" );
    BOOST_REQUIRE( result["maxFeePerGas"] == "0x4a817c801" );

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
    string senderAddress = toJS( fixture.coinbase.address() );

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
    time_t eip1559PatchActivationTimestamp = time( nullptr ) + 5;
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] =
        eip1559PatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS( fixture.coinbase.address() );

    Json::Value txRefill;
    txRefill["to"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    for ( size_t i = 0; i < 10; ++i ) {
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

    for ( Json::Value::ArrayIndex i = 0; i < blockCnt; ++i ) {
        BOOST_REQUIRE( feeHistory["baseFeePerGas"][i].isString() );
        std::string estimatedBaseFeePerGas =
            EIP1559TransactionsPatch::isEnabledWhen(
                fixture.client->blockInfo( bn - i - 1 ).timestamp() ) ?
                toJS( fixture.client->gasBidPrice( bn - i - 1 ) ) :
                toJS( 0 );
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
    time_t eip1559PatchActivationTimestamp = time( nullptr );
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] =
        eip1559PatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS( fixture.coinbase.address() );

    Json::Value txRefill;
    txRefill["to"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // send non replay protected txn
    txHash = fixture.rpcClient->eth_sendRawTransaction(
        "0xf864808504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b01801ba0171c7f31feaa0f"
        "d7825a5a28d7b535d0b0ee200b27792f66eb7796e7a6a555d7a0081790244f21cefa563b55a7a68ee78f846673"
        "8b5827be19faaeff0586fd71be" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value txn = fixture.rpcClient->eth_getTransactionByHash( txHash );
    dev::u256 v = dev::jsToU256( txn["v"].asString() );
    BOOST_REQUIRE( v < 29 && v > 26 );

    // send replay protected legacy txn
    txHash = fixture.rpcClient->eth_sendRawTransaction(
        "0xf866018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180820151a018b400fc56"
        "bc3568e4f23f6f93d538745a5b18054252d6030791c294c9aea9d4a00930492125784fad0a8b38b915e8621f54"
        "c53f0878a77f21920c751ec5fd220a" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txn = fixture.rpcClient->eth_getTransactionByHash( txHash );
    v = dev::jsToU256( txn["v"].asString() );
    BOOST_REQUIRE( v < 339 && v > 336 );  // 2 * 151 + 35

    // send type1 txn
    txHash = fixture.rpcClient->eth_sendRawTransaction(
        "0x01f8c38197028504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b0180f85bf85994de"
        "0b295669a9fd93d5f28d9ec85e40f4cb697baef842a00000000000000000000000000000000000000000000000"
        "000000000000000003a0000000000000000000000000000000000000000000000000000000000000000701a0ee"
        "608b7c5df843b4a1988a3e9c24d53019fa674e06a6b2ae0c347a00601c1a84a06ed451f9cc0f4334a180458605"
        "ecaa212e58f8436e1a4318e75ae417c72eba2b" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txn = fixture.rpcClient->eth_getTransactionByHash( txHash );
    v = dev::jsToU256( txn["v"].asString() );
    BOOST_REQUIRE( v < 2 && v >= 0 );

    // send type2 txn
    txHash = fixture.rpcClient->eth_sendRawTransaction(
        "0x02f86d8197038504a817c8008504a817c801827530947d36af85a184e220a656525fcbb9a63b9ab3c12b808"
        "0c001a0f399bf6ecf7f7d464d18a36a05475c6464760a111a2039b9be76eadbe185e4b4a0234c828163103039"
        "446653ebc8031631ab984e59169be57a759345fe4a1bf127" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txn = fixture.rpcClient->eth_getTransactionByHash( txHash );
    v = dev::jsToU256( txn["v"].asString() );
    BOOST_REQUIRE( v < 2 && v >= 0 );
}

BOOST_AUTO_TEST_CASE( InvalidTransactionFormatPatch ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    // Set chainID = 151
    std::string chainID = "0x97";
    ret["params"]["chainID"] = chainID;
    time_t eip1559PatchActivationTimestamp = time( nullptr ) - 1;
    time_t InvalidTransactionFormatPatchActivationTimestamp = time( nullptr ) + 10;
    ret["skaleConfig"]["sChain"]["InvalidTransactionFormatPatchTimestamp"] =
        InvalidTransactionFormatPatchActivationTimestamp;
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] =
        eip1559PatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS( fixture.coinbase.address() );

    Json::Value txRefill;
    txRefill["to"] = "0x5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 1000000000000000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );


#ifndef FAIR
    // send a txn with maxPriorityFeePerGas > maxFeePerGas before InvalidTransactionFormatPatchTimestamp
    txHash = fixture.rpcClient->eth_sendRawTransaction(
        "0x02f86d8197808504a817c8018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b8080"
        "c001a0db2fe04a66fa54bfe9c6e0166d85a31b34cbff10dbde0e0584081aec6bb33c30a06b956a49c52f1460da"
        "9f93fc495eaa863ae5a8c91ee9230c2f3976f5e74d4f47" );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );

    Json::Value tx = fixture.rpcClient->eth_getTransactionByHash( txHash );
    BOOST_REQUIRE( dev::jsToU256( tx["maxFeePerGas"].asString() ) <
                   dev::jsToU256( tx["maxPriorityFeePerGas"].asString() ) );

    dev::eth::Transaction t = fixture.client->transaction( dev::h256( txHash ) );
    BOOST_REQUIRE( t.maxFeePerGas() < t.maxPriorityFeePerGas() );
#endif

    sleep( 10 );

    // force 1 block to update timestamp
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["from"] = senderAddress;
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 0;
    txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    sleep( 1 );

    // send a txn with maxPriorityFeePerGas > maxFeePerGas after
    // InvalidTransactionFormatPatchTimestamp, it should fail
    BOOST_REQUIRE_THROW(
        fixture.rpcClient->eth_sendRawTransaction(
            "0x02f86d8197018504a817c8018504a817c800827530947d36af85a184e220a656525fcbb9a63b9ab3c12b"
            "8080c080a0aea5ff86373cbbbb33c9f3e9a25ceb9a694ee71beff452a4d29903d73fd30ca9a00458d4f7d5"
            "4be178b42d230cc5a4740540d55b8ca0f9c74c79c1d49f6686b1e6" ),
        jsonrpc::JsonRpcException );  // INVALID_PARAMS
}

BOOST_AUTO_TEST_CASE( jsonrpcVersionInResponseHeader ) {
    JsonRpcFixture fixture;

    dev::eth::simulateMining( *( fixture.client ), 20 );
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
    std::string bytecode =
        "6080604052348015600f57600080fd5b5061015e8061001f6000396000f3fe6080604052348015610010576000"
        "80fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd"
        "5b610043610075565b60405161005091906100af565b60405180910390f35b610073600480360381019061006e"
        "91906100fb565b61007e565b005b60008054905090565b80600081905550806001819055508060028190555050"
        "565b6000819050919050565b6100a981610096565b82525050565b60006020820190506100c460008301846100"
        "a0565b92915050565b600080fd5b6100d881610096565b81146100e357600080fd5b50565b6000813590506100"
        "f5816100cf565b92915050565b600060208284031215610111576101106100ca565b5b600061011f8482850161"
        "00e6565b9150509291505056fea264697066735822122081840c9060f8fb10a0bdf054a92c6bd15ea462286507"
        "fad9a9fe26e653e2f2e264736f6c634300081a0033";
    auto senderAddress = fixture.coinbase.address();

    Json::Value create1;
    create1["from"] = toJS( senderAddress );
    create1["data"] = bytecode;
    create1["gas"] = "1800000";
    string txHash = fixture.rpcClient->eth_sendTransaction( create1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );
    string contractAddress = receipt["contractAddress"].asString();

    skutils::rest::client cli( skutils::rest::g_nClientConnectionTimeoutMS );
    std::string url =
        std::string( "http://" ) +
        fixture.skale_server_connector->opts_.netOpts_.bindOptsStandard_.strAddrHTTP4_ +
        std::string( ":" ) +
        std::to_string(
            fixture.skale_server_connector->opts_.netOpts_.bindOptsStandard_.nBasePortHTTP4_ );
    BOOST_REQUIRE( cli.open( url ) );

    // try to send bad call to trigger EVM reverted w/o description error
    nlohmann::json joIn = nlohmann::json::object();
    joIn["jsonrpc"] = "2.0";
    joIn["method"] = "eth_call";
    nlohmann::json params = nlohmann::json::array();
    nlohmann::json callDetails = nlohmann::json::object();
    callDetails["data"] =
        "0x01ffc9a7d9b67a2600000000000000000000000000000000000000000000000000000000";
    callDetails["to"] = contractAddress;
    params.push_back( callDetails );
    params.push_back( "latest" );
    joIn["params"] = params;
    skutils::rest::data_t d = cli.call( joIn );

    nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
    BOOST_REQUIRE( joAnswer.count( "jsonrpc" ) > 0 );
    BOOST_REQUIRE( joAnswer["jsonrpc"] == "2.0" );

    // try to send legit eth_call as well
    joIn = nlohmann::json::object();
    joIn["jsonrpc"] = "2.0";
    joIn["method"] = "eth_call";
    params = nlohmann::json::array();
    callDetails = nlohmann::json::object();
    callDetails["data"] = "0x2e64cec1";
    callDetails["to"] = contractAddress;
    callDetails["from"] = senderAddress.hex();
    callDetails["value"] = "0x0";
    params.push_back( callDetails );
    params.push_back( "latest" );
    joIn["params"] = params;
    d = cli.call( joIn );

    joAnswer = nlohmann::json::parse( d.s_ );
    BOOST_REQUIRE( joAnswer.count( "jsonrpc" ) > 0 );
    BOOST_REQUIRE( joAnswer["jsonrpc"] == "2.0" );
}

BOOST_AUTO_TEST_CASE( getZeroBlock ) {
    JsonRpcFixture fixture;
    Json::Value block = fixture.rpcClient->eth_getBlockByNumber( "0", "false" );
    BOOST_REQUIRE( block["number"] == string( "0x0" ) );

    string blockHash = block["hash"].asString();
    Json::Value blockByHash = fixture.rpcClient->eth_getBlockByHash( blockHash, "false" );
    BOOST_REQUIRE( blockByHash["number"] == string( "0x0" ) );
    BOOST_REQUIRE( blockByHash["hash"] == blockHash );
}

BOOST_AUTO_TEST_CASE( getBlockRandom ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    time_t currentBlockRandomPatchActivationTimestamp = time( nullptr ) + 10;
    ret["skaleConfig"]["sChain"]["currentBlockRandomPatchTimestamp"] =
        currentBlockRandomPatchActivationTimestamp;

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );

    JsonRpcFixture fixture( config );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS( fixture.coinbase.address() );
    dev::eth::g_skaleHost = fixture.client->skaleHost();

    // create block and save block random for it
    Json::Value txRefill;
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["from"] = toJS( senderAddress );
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 1;
    fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    PrecompiledExecutor blockRandomExecutor = PrecompiledRegistrar::executor( "getBlockRandom" );
    auto blockNumberEarly = fixture.client->number();
    dev::eth::PrecompiledCallContext ctx( blockNumberEarly,
#ifdef BITE2
                                          0,
                                          1,
#endif
                                          true );
    auto blockRandomEarly = blockRandomExecutor( dev::bytesConstRef(), ctx );

    // wait till patch is activated
    sleep( 10 );
    fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

//    pragma solidity ^0.8.13;

//    contract GetBlockRandomPrecompiled {
//        address public constant PRECOMPILE_0X08 = address(0x08);
//        bytes32 lastBlockRandom;

//        function getBlockRandom() public returns (bytes32) {
//            (bool success, bytes memory result) = PRECOMPILE_0X08.staticcall("");
//            require(success, "Call to precompile 0x06 failed");
//            require(result.length >= 20, "Invalid result length");
//            lastBlockRandom = bytes32(result);

//            return lastBlockRandom;
//        }

//        function getLastBlockRandom() public view returns (bytes32) {
//            return lastBlockRandom;
//        }
//    }
    std::string bytecode = "6080604052348015600f57600080fd5b506104548061001f6000396000f3fe608060405234801561001057600080fd5b50600436106100415760003560e01c80633ec4b2de146100465780635e2e884d14610064578063dc031dfe14610082575b600080fd5b61004e6100a0565b60405161005b91906101fc565b60405180910390f35b61006c6100a5565b6040516100799190610230565b60405180910390f35b61008a6100ae565b6040516100979190610230565b60405180910390f35b600881565b60008054905090565b6000806000600873ffffffffffffffffffffffffffffffffffffffff166040516100d79061027c565b600060405180830381855afa9150503d8060008114610112576040519150601f19603f3d011682016040523d82523d6000602084013e610117565b606091505b50915091508161015c576040517f08c379a0000000000000000000000000000000000000000000000000000000008152600401610153906102ee565b60405180910390fd5b6014815110156101a1576040517f08c379a00000000000000000000000000000000000000000000000000000000081526004016101989061035a565b60405180910390fd5b806101ab906103b7565b6000819055506000549250505090565b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b60006101e6826101bb565b9050919050565b6101f6816101db565b82525050565b600060208201905061021160008301846101ed565b92915050565b6000819050919050565b61022a81610217565b82525050565b60006020820190506102456000830184610221565b92915050565b600081905092915050565b50565b600061026660008361024b565b915061027182610256565b600082019050919050565b600061028782610259565b9150819050919050565b600082825260208201905092915050565b7f43616c6c20746f20707265636f6d70696c652030783036206661696c65640000600082015250565b60006102d8601e83610291565b91506102e3826102a2565b602082019050919050565b60006020820190508181036000830152610307816102cb565b9050919050565b7f496e76616c696420726573756c74206c656e6774680000000000000000000000600082015250565b6000610344601583610291565b915061034f8261030e565b602082019050919050565b6000602082019050818103600083015261037381610337565b9050919050565b600081519050919050565b6000819050602082019050919050565b60006103a18251610217565b80915050919050565b600082821b905092915050565b60006103c28261037a565b826103cc84610385565b90506103d781610395565b92506020821015610417576104127fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff836020036008026103aa565b831692505b505091905056fea26469706673582212200547f4a1b7b525d8f531c7e991887fdca3437350b86fc09b06a1acaa83b5c3ad64736f6c634300081e0033";

    // deploy contract
    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = bytecode;
    create["gas"] = "900000";
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    auto txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    std::string contractAddress = txReceipt["contractAddress"].asString();

    // submit getBlockRandom transaction
    Json::Value txGenerate;
    txGenerate["to"] = contractAddress;
    txGenerate["data"] = "0xdc031dfe";
    txGenerate["from"] = toJS( senderAddress );
    fixture.rpcClient->eth_sendTransaction( txGenerate );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // read data from contract
    Json::Value callGetLast;
    callGetLast["to"] = contractAddress;
    callGetLast["data"] = "0x5e2e884d";
    callGetLast["from"] = toJS( senderAddress );
    dev::bytes blockRandomFromContract = dev::fromHex( fixture.rpcClient->eth_call( callGetLast, "latest" ) );

    ctx = PrecompiledCallContext( fixture.client->number(),
#ifdef BITE2
                                0,
                                1,
#endif
                                true );
    auto executionResult = blockRandomExecutor( dev::bytesConstRef(), ctx );

    BOOST_REQUIRE( executionResult.first );
    BOOST_REQUIRE( executionResult.second == blockRandomFromContract );

#ifdef HISTORIC_STATE
    // produce more blocks, execute historic call and compare results
    fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    dev::bytes blockRandomFromHistoricCall = dev::fromHex( fixture.rpcClient->eth_call( callGetLast, toJS( fixture.client->number() - 1 ) ) );
    BOOST_REQUIRE( blockRandomFromHistoricCall == blockRandomFromContract );

    // ask for blockRandom for early block
    ctx = PrecompiledCallContext( blockNumberEarly,
#ifdef BITE2
                                0,
                                1,
#endif
                                true );
    auto blockRandomEarlyHistoric = blockRandomExecutor(dev::bytesConstRef(), ctx );
    BOOST_REQUIRE( blockRandomEarlyHistoric.first );
    BOOST_REQUIRE( blockRandomEarlyHistoric.second == blockRandomEarly.second );
#endif
}

#ifdef BITE

#ifndef FAIR
static std::string const c_BITEConfigString =
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
         "externalGasDifficulty": "0x1",
         "minGasLimit": "0xFFFFFFF",
         "maxGasLimit": "7fffffffffffffff"
    },
    "genesis": {
        "author" : "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
        "difficulty" : "0x20000",
        "gasLimit": "0xFFFFFFF",
        "nonce" : "0x00",
        "extraData" : "0x00",
        "timestamp" : "0x00",
        "mixHash" : "0x00",
        "stateRoot": "0x01"
    },
    "skaleConfig": {
        "nodeInfo": {
            "nodeID": 8,
            "nodeName": "test_node",
            "bindIP": "0.0.0.0",
            "logLevel": "info",
            "logLevelConfig": "info",
            "imaMessageProxySChain": "0xd2AAa00100000000000000000000000000000000",
            "imaMessageProxyMainNet": "0x337591F78cbf2b113A57D9709511a1b6E524DdaE",
            "rotateAfterBlock": 10240,
            "basePort": )" +
    std::to_string( rand_port ) + R"(,
            "logLevel": "trace",
            "logLevelProposal": "trace",
            "ecdsaKeyName": "NEK:d391a1af1cd9663335e0f970e59402bf16fcfe0cc421c535bf60ba618a456d68",
            "wallets": {
                "ima": {
                    "keyShareName": "BLS_KEY:SCHAIN_ID:1:NODE_ID:0:DKG_ID:0",
                    "t": 1,
                    "n": 1,
                    "certFile": "/skale-data/node_data/sgx_certs/sgx.crt",
                    "keyFile": "/skale-data/node_data/sgx_certs/sgx.key",
                    "commonBLSPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                    "commonBLSPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                    "commonBLSPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                    "commonBLSPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693",
                    "BLSPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                    "BLSPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                    "BLSPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                    "BLSPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693"
                }
            }
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "contractStorageLimit": 128,
            "emptyBlockIntervalMs": -1,
            "nodeGroups": {
                "0": {
                    "nodes": {
                        "8": [
                            1,
                            8,
                            "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea105a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467"
                        ]
                    },
                    "finish_ts": null,
                    "bls_public_key": {
                        "blsPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                        "blsPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                        "blsPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                        "blsPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693"
                    }
                }
            },
            "nodes": [
                {
                    "nodeID": 8,
                    "nodeName": "test_node",
                    "basePort": )" + std::to_string( rand_port ) + R"(,
                    "httpRpcPort": 9568,
                    "httpsRpcPort": 9573,
                    "wsRpcPort": 9567,
                    "wssRpcPort": 9572,
                    "blsPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                    "blsPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                    "blsPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                    "blsPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693",
                    "publicKey": "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea105a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467",
                    "owner": "0x5112ce768917e907191557d7e9521c2590cdd3a0",
                    "schainIndex": 1,
                    "ip": "186.14.217.13",
                    "publicIP": "4.127.224.50"
                }
            ]
        }
    },
    "accounts": {
        "0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
        "0000000000000000000000000000000000000005": { "precompiled": { "name": "getBlockRandom", "linear": { "base": 15, "word": 0 } } },)"
#ifdef BITE2
    R"(
		"0000000000000000000000000000000000000006": { "precompiled": { "name": "getRandomWalletAndSignatureForCTX", "linear": { "base": 15, "word": 0 } } },
        "0000000000000000000000000000000000000007": { "precompiled": { "name": "submitCTX", "linear": { "base": 15, "word": 0 } } },)"
#endif
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
    R"("0x5c4e11842e8be09264dc1976943571d7af6d00f9" : {
            "balance" : "1000000000000000000000000000000"
        },
        "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F": {
            "balance": "1000000000000000000000000000000"
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
                "0x0": "0x5C4e11842E8be09264dc1976943571d7Af6d00F9"
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
#else
static std::string const c_BITEConfigString =
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
         "minGasLimit": "0xFFFFFFF",
         "maxGasLimit": "7fffffffffffffff"
    },
    "genesis": {
        "author" : "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
        "difficulty" : "0x20000",
        "gasLimit": "0xFFFFFFF",
        "nonce" : "0x00",
        "extraData" : "0x00",
        "timestamp" : "0x00",
        "mixHash" : "0x00",
        "stateRoot": "0x01"
    },
    "skaleConfig": {
        "nodeInfo": {
            "nodeID": 8,
            "nodeName": "test_node",
            "bindIP": "0.0.0.0",
            "logLevel": "info",
            "logLevelConfig": "info",
            "rotateAfterBlock": 10240,
            "basePort": )" +
    std::to_string( rand_port ) + R"(,
            "logLevel": "trace",
            "logLevelProposal": "trace",
            "ecdsaKeyName": "NEK:d391a1af1cd9663335e0f970e59402bf16fcfe0cc421c535bf60ba618a456d68"
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "emptyBlockIntervalMs": -1,
            "nodeGroups": {
                "0": {
                    "nodes": {
                        "8": [
                            1,
                            8,
                            "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea105a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467"
                        ]
                    },
                    "finish_ts": null,
                    "bls_public_key": {
                        "blsPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                        "blsPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                        "blsPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                        "blsPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693"
                    }
                }
            },
            "nodes": {
                "1": {
                    "blsKey": {
                        "keyShareName": "BLS_KEY:SCHAIN_ID:1:NODE_ID:0:DKG_ID:0",
                        "t": 1,
                        "n": 1,
                        "certFile": "/skale-data/node_data/sgx_certs/sgx.crt",
                        "keyFile": "/skale-data/node_data/sgx_certs/sgx.key",
                        "commonBLSPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                        "commonBLSPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                        "commonBLSPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                        "commonBLSPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693",
                        "BLSPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                        "BLSPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                        "BLSPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                        "BLSPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693"
                    },
                    "group": [
                        {
                            "nodeID": 8,
                            "nodeName": "test_node",
                            "basePort": )" + std::to_string( rand_port ) + R"(,
                            "httpRpcPort": 9568,
                            "httpsRpcPort": 9573,
                            "wsRpcPort": 9567,
                            "wssRpcPort": 9572,
                            "blsPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                            "blsPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                            "blsPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                            "blsPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693",
                            "publicKey": "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea105a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467",
                            "owner": "0x5112ce768917e907191557d7e9521c2590cdd3a0",
                            "schainIndex": 1,
                            "ip": "186.14.217.13",
                            "publicIP": "4.127.224.50"
                        }
                    ]
                },
                "-1": {}
            }
        }
    },
    "accounts": {
        "0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
        "0000000000000000000000000000000000000005": { "precompiled": { "name": "getBlockRandom", "linear": { "base": 15, "word": 0 } } },)"
#ifdef BITE2
    R"(
        "0000000000000000000000000000000000000006": { "precompiled": { "name": "getRandomWalletAndSignatureForCTX", "linear": { "base": 15, "word": 0 } } },
        "0000000000000000000000000000000000000007": { "precompiled": { "name": "submitCTX", "linear": { "base": 15, "word": 0 } } },)"
#endif
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
    R"("0x5c4e11842e8be09264dc1976943571d7af6d00f9" : {
            "balance" : "1000000000000000000000000000000"
        },
        "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F": {
            "balance": "1000000000000000000000000000000"
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

static std::string const c_BITECommitteeRotationConfigString =
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
            "nodeID": 8,
            "nodeName": "test_node",
            "bindIP": "0.0.0.0",
            "logLevel": "info",
            "logLevelConfig": "info",
            "imaMessageProxySChain": "0xd2AAa00100000000000000000000000000000000",
            "imaMessageProxyMainNet": "0x337591F78cbf2b113A57D9709511a1b6E524DdaE",
            "rotateAfterBlock": 10240,
            "basePort": )" +
    std::to_string( rand_port ) + R"(,
            "logLevel": "trace",
            "logLevelProposal": "trace",
            "ecdsaKeyName": "NEK:d391a1af1cd9663335e0f970e59402bf16fcfe0cc421c535bf60ba618a456d68"
        },
        "sChain": {
            "schainName": "TestChain",
            "schainID": 1,
            "emptyBlockIntervalMs": -1,
            "nodeGroups": {
                "0": {
                    "nodes": {
                        "8": [
                            0,
                            8,
                            "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea105a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467",
                            "0x08151B8F80bfa7dEa760e461412AF24348224edf"
                        ]
                    },
                    "finish_ts": 1,
                    "bls_public_key": {
                        "blsPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                        "blsPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                        "blsPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                        "blsPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693"
                    }
                },
                "1": {
                    "nodes": {
                        "8": [
                            0,
                            8,
                            "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea105a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467",
                            "0x405c96D388cDFBa4f17493c875CCE9c680225276"
                        ]
                    },
                    "finish_ts": null,
                    "bls_public_key": {
                        "blsPublicKey0": "3842742177969966091367527274107524613106077736353521259727282251005583743182",
                        "blsPublicKey1": "3497912824016228906558906422247670474553186446469877598411863912329082553081",
                        "blsPublicKey2": "8173996886448941320370434854289578123609627835954133538412363037981850950343",
                        "blsPublicKey3": "20979370720689475348670582375026949105497642726992863932315517524004804784155"
                    }
                }
            },
            "nodes": {
                "1": {
                    "blsKey": {
                        "keyShareName": "BLS_KEY:SCHAIN_ID:1:NODE_ID:0:DKG_ID:0",
                        "t": 1,
                        "n": 1,
                        "certFile": "/skale-data/node_data/sgx_certs/sgx.crt",
                        "keyFile": "/skale-data/node_data/sgx_certs/sgx.key",
                        "commonBLSPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                        "commonBLSPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                        "commonBLSPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                        "commonBLSPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693",
                        "BLSPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                        "BLSPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                        "BLSPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                        "BLSPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693"
                    },
                    "stakingContractAddress": "0x5C60C315985977b7a408eBF4256984Acdf949549",
                    "group": [
                        {
                            "nodeID": 8,
                            "nodeName": "test_node",
                            "basePort": )" + std::to_string( rand_port ) + R"(,
                            "httpRpcPort": 9568,
                            "httpsRpcPort": 9573,
                            "wsRpcPort": 9567,
                            "wssRpcPort": 9572,
                            "blsPublicKey0": "15959969554621958245201075983340071881770733084910870228938077786643587385029",
                            "blsPublicKey1": "7970122607051572307517094692346020360016825923464107614135327251488152616550",
                            "blsPublicKey2": "3371162264373897025322009434717052197952692496405149486989861571246537813591",
                            "blsPublicKey3": "13678625751515504401110635369790787716744686498431213713911601759809559919693",
                            "publicKey": "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea105a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467",
                            "owner": "0x5112ce768917e907191557d7e9521c2590cdd3a0",
                            "schainIndex": 1,
                            "ip": "186.14.217.13",
                            "publicIP": "4.127.224.50"
                        }
                    ]
                },
                "-1": {
                    "blsKey": {
                        "keyShareName": "BLS_KEY:SCHAIN_ID:1:NODE_ID:0:DKG_ID:0",
                        "t": 1,
                        "n": 1,
                        "certFile": "/skale-data/node_data/sgx_certs/sgx.crt",
                        "keyFile": "/skale-data/node_data/sgx_certs/sgx.key",
                        "commonBLSPublicKey0": "3842742177969966091367527274107524613106077736353521259727282251005583743182",
                        "commonBLSPublicKey1": "3497912824016228906558906422247670474553186446469877598411863912329082553081",
                        "commonBLSPublicKey2": "8173996886448941320370434854289578123609627835954133538412363037981850950343",
                        "commonBLSPublicKey3": "20979370720689475348670582375026949105497642726992863932315517524004804784155",
                        "BLSPublicKey0": "3842742177969966091367527274107524613106077736353521259727282251005583743182",
                        "BLSPublicKey1": "3497912824016228906558906422247670474553186446469877598411863912329082553081",
                        "BLSPublicKey2": "8173996886448941320370434854289578123609627835954133538412363037981850950343",
                        "BLSPublicKey3": "20979370720689475348670582375026949105497642726992863932315517524004804784155"
                    },
                    "group": [
                        {
                            "nodeID": 8,
                            "nodeName": "test_node",
                            "basePort": )" + std::to_string( rand_port ) + R"(,
                            "httpRpcPort": 9568,
                            "httpsRpcPort": 9573,
                            "wsRpcPort": 9567,
                            "wssRpcPort": 9572,
                            "blsPublicKey0": "3842742177969966091367527274107524613106077736353521259727282251005583743182",
                            "blsPublicKey1": "3497912824016228906558906422247670474553186446469877598411863912329082553081",
                            "blsPublicKey2": "8173996886448941320370434854289578123609627835954133538412363037981850950343",
                            "blsPublicKey3": "20979370720689475348670582375026949105497642726992863932315517524004804784155",
                            "publicKey": "0xf925c203a30ec6cad5a263db3efab7ed4c1fd74c8688167e10a5a22e15ab5018d8553df0ac54ea105a3d21845e5660bc3d4e7c82e7af1daa3baad393b1521467",
                            "owner": "0x5112ce768917e907191557d7e9521c2590cdd3a0",
                            "schainIndex": 1,
                            "ip": "186.14.217.13",
                            "publicIP": "4.127.224.50"
                        }
                    ]
                }
            }
        }
    },
    "accounts": {
        "0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },)"
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
    R"("0x5c4e11842e8be09264dc1976943571d7af6d00f9" : {
            "balance" : "1000000000000000000000000000000"
        },
        "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F": {
            "balance": "1000000000000000000000000000000"
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
#endif // #ifndef FAIR


BOOST_AUTO_TEST_CASE( getCommonPublicKey ) {
    JsonRpcFixture fixture( c_BITEConfigString, false, false, true );

    Json::Value biteInfo = fixture.rpcClient->bite_getCommitteesInfo();

    BOOST_REQUIRE( biteInfo.isArray() );
    std::string blsPublicKey = biteInfo[0]["commonBLSPublicKey"].asString();
    uint64_t epochId = biteInfo[0]["epochId"].asUInt64();

    auto commonPublicKeyFromConfig = fixture.client->chainParams().getCommonBlsPublicKey();
    libBLS::algebra::G2Point commonPublicKey = libBLS::algebra::G2Point::fromString( commonPublicKeyFromConfig, libBLS::Base::DEC );

    BOOST_REQUIRE_EQUAL( blsPublicKey.size(), 256 );
    BOOST_REQUIRE_EQUAL( libBLS::TEPublicKey( blsPublicKey, libBLS::Base::HEXA ).getPublicKeyRaw(), commonPublicKey );
    BOOST_REQUIRE_EQUAL( epochId, fixture.client->getCurrentEpochId() );
}

#ifdef BITE2

// Helper function to build abi.encode(bytes[] args1, bytes[] args2)
dev::bytes buildAbiEncodedArrays( const std::vector<dev::bytes>& args1Elements, const std::vector<dev::bytes>& args2Elements ) {
    // Validate that all args1 elements meet minimum length requirement
    for ( size_t i = 0; i < args1Elements.size(); ++i ) {
        if ( args1Elements[i].size() < BITE_CIPHERTEXT_MIN_LEN ) {
            throw std::runtime_error( "buildAbiEncodedArrays: args1 element " + std::to_string(i) +
                " is too short (" + std::to_string(args1Elements[i].size()) + " bytes), must be at least " +
                std::to_string(BITE_CIPHERTEXT_MIN_LEN) + " bytes" );
        }
    }

    dev::bytes result;

    // Calculate total size for args1 array data
    size_t args1DataSize = 32;  // length field
    args1DataSize += 32 * args1Elements.size();  // offsets for each element
    for ( const auto& elem : args1Elements ) {
        args1DataSize += 32;  // length field for element
        args1DataSize += (elem.size() + 31) / 32 * 32;  // padded element data
    }

    // Write offsets to both arrays
    dev::bytes args1Offset = dev::toBigEndian( dev::u256( 64 ) );
    dev::bytes args2Offset = dev::toBigEndian( dev::u256( 64 + args1DataSize ) );
    result.insert( result.end(), args1Offset.begin(), args1Offset.end() );
    result.insert( result.end(), args2Offset.begin(), args2Offset.end() );

    // Encode args1 array
    dev::bytes args1Length = dev::toBigEndian( dev::u256( args1Elements.size() ) );
    result.insert( result.end(), args1Length.begin(), args1Length.end() );

    // Calculate and write element offsets for args1
    size_t currentOffset = 32 * args1Elements.size();  // After all offset fields
    for ( const auto& elem : args1Elements ) {
        dev::bytes elemOffset = dev::toBigEndian( dev::u256( currentOffset ) );
        result.insert( result.end(), elemOffset.begin(), elemOffset.end() );
        currentOffset += 32 + (elem.size() + 31) / 32 * 32;  // length + padded data
    }

    // Write args1 element data
    for ( const auto& elem : args1Elements ) {
        dev::bytes elemLength = dev::toBigEndian( dev::u256( elem.size() ) );
        result.insert( result.end(), elemLength.begin(), elemLength.end() );
        result.insert( result.end(), elem.begin(), elem.end() );
        while( result.size() % 32 != 0 ) result.push_back(0);
    }

    // Encode args2 array (same structure as args1)
    dev::bytes args2Length = dev::toBigEndian( dev::u256( args2Elements.size() ) );
    result.insert( result.end(), args2Length.begin(), args2Length.end() );

    // Calculate and write element offsets for args2
    currentOffset = 32 * args2Elements.size();
    for ( const auto& elem : args2Elements ) {
        dev::bytes elemOffset = dev::toBigEndian( dev::u256( currentOffset ) );
        result.insert( result.end(), elemOffset.begin(), elemOffset.end() );
        currentOffset += 32 + (elem.size() + 31) / 32 * 32;
    }

    // Write args2 element data
    for ( const auto& elem : args2Elements ) {
        dev::bytes elemLength = dev::toBigEndian( dev::u256( elem.size() ) );
        result.insert( result.end(), elemLength.begin(), elemLength.end() );
        result.insert( result.end(), elem.begin(), elem.end() );
        while( result.size() % 32 != 0 ) result.push_back(0);
    }

    return result;
}

BOOST_AUTO_TEST_CASE( getRandomWalletAndSignatureForCTX ) {
    JsonRpcFixture fixture( c_BITEConfigString, true, true, true, true, false, -1, {{ "contractStorageLimit", "100000" }} );

    dev::eth::g_skaleHost = fixture.client->skaleHost();

    string senderAddress = toJS( fixture.coinbase.address() );

//    pragma solidity ^0.8.13;

//    contract Precompile0x06Caller {
//        address public lastGenerated;
//        address public preLastGenerated;
//        bytes public lastSignature;
//        bytes public preLastSignature;

//        function generateRandomWallet() public returns (address) {
//            uint256 randomNumber = uint256(keccak256(abi.encodePacked(block.timestamp, block.number)));
//            bytes[] memory args1 = new bytes[](2);
//            // args1 elements must be at least BITE_CIPHERTEXT_MIN_LEN bytes (276 bytes)
//            args1[0] = new bytes(276);
//            for (uint i = 0; i < 276; i++) {
//                args1[0][i] = 0x11;
//            }
//            args1[1] = new bytes(276);
//            for (uint i = 0; i < 276; i++) {
//                args1[1][i] = 0x22;
//            }
//            bytes[] memory args2 = new bytes[](2);
//            args2[0] = abi.encodePacked("plaintext1");
//            args2[1] = abi.encodePacked("plaintext2");
//            bytes memory randomBytes = abi.encode(args1, args2);
//            bytes memory input = abi.encode(address(this), randomNumber, randomBytes);

//            (bool success, bytes memory result) = address(0x06).staticcall(input);

//            preLastGenerated = lastGenerated;
//            preLastSignature = lastSignature;

//            address addr = address(bytes20(result));

//            bytes memory signature = new bytes(result.length - 20);
//            for (uint i = 0; i < result.length - 20; i++) {
//                signature[i] = result[i + 20];
//            }

//            lastGenerated = addr;
//            lastSignature = signature;

//            return addr;
//        }

//        function generateRandomWalletWithInput(bytes calldata input) public returns (address) {
//            (bool success, bytes memory result) = address(0x06).staticcall(input);

//            preLastGenerated = lastGenerated;
//            preLastSignature = lastSignature;

//            address addr = address(bytes20(result));

//            bytes memory signature = new bytes(result.length - 20);
//            for (uint i = 0; i < result.length - 20; i++) {
//                signature[i] = result[i + 20];
//            }

//            lastGenerated = addr;
//            lastSignature = signature;

//            return addr;
//        }

//        function getLastGeneratedAddress() public view returns (address) {
//            return lastGenerated;
//        }

//        function getPreLastGeneratedAddress() public view returns (address) {
//            return preLastGenerated;
//        }

//        function getLastSignature() public view returns (bytes memory) {
//            return lastSignature;
//        }

//        function getPreLastSignature() public view returns (bytes memory) {
//            return preLastSignature;
//        }
//    }
    std::string bytecode = "6080604052348015600f57600080fd5b506117a78061001f6000396000f3fe608060405234801561001057600080fd5b506004361061009e5760003560e01c80638628f93a116100665780638628f93a146101395780638ee64f3614610169578063c667882614610187578063cdf72f9e146101a5578063fcab457f146101c35761009e565b8063041d5d7b146100a35780630c2dd5f4146100c15780632bbdbd7e146100df57806352c92885146100fd5780638482f2461461011b575b600080fd5b6100ab6101e1565b6040516100b89190610cf6565b60405180910390f35b6100c961020a565b6040516100d69190610cf6565b60405180910390f35b6100e7610230565b6040516100f49190610da1565b60405180910390f35b6101056102be565b6040516101129190610cf6565b60405180910390f35b610123610873565b6040516101309190610da1565b60405180910390f35b610153600480360381019061014e9190610e32565b610905565b6040516101609190610cf6565b60405180910390f35b610171610b47565b60405161017e9190610da1565b60405180910390f35b61018f610bd9565b60405161019c9190610da1565b60405180910390f35b6101ad610c67565b6040516101ba9190610cf6565b60405180910390f35b6101cb610c8b565b6040516101d89190610cf6565b60405180910390f35b60008060009054906101000a900473ffffffffffffffffffffffffffffffffffffffff16905090565b600160009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b6003805461023d90610eae565b80601f016020809104026020016040519081016040528092919081815260200182805461026990610eae565b80156102b65780601f1061028b576101008083540402835291602001916102b6565b820191906000526020600020905b81548152906001019060200180831161029957829003601f168201915b505050505081565b60008042436040516020016102d4929190610f0a565b6040516020818303038152906040528051906020012060001c90506000600267ffffffffffffffff81111561030c5761030b610f36565b5b60405190808252806020026020018201604052801561033f57816020015b606081526020019060019003908161032a5790505b50905061011467ffffffffffffffff81111561035e5761035d610f36565b5b6040519080825280601f01601f1916602001820160405280156103905781602001600182028036833780820191505090505b50816000815181106103a5576103a4610f65565b5b602002602001018190525060005b61011481101561042c57601160f81b826000815181106103d6576103d5610f65565b5b602002602001015182815181106103f0576103ef610f65565b5b60200101907effffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff1916908160001a90535080806001019150506103b3565b5061011467ffffffffffffffff81111561044957610448610f36565b5b6040519080825280601f01601f19166020018201604052801561047b5781602001600182028036833780820191505090505b50816001815181106104905761048f610f65565b5b602002602001018190525060005b61011481101561051757602260f81b826001815181106104c1576104c0610f65565b5b602002602001015182815181106104db576104da610f65565b5b60200101907effffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff1916908160001a905350808060010191505061049e565b506000600267ffffffffffffffff81111561053557610534610f36565b5b60405190808252806020026020018201604052801561056857816020015b60608152602001906001900390816105535790505b50905060405160200161057a90610feb565b6040516020818303038152906040528160008151811061059d5761059c610f65565b5b60200260200101819052506040516020016105b79061104c565b604051602081830303815290604052816001815181106105da576105d9610f65565b5b6020026020010181905250600082826040516020016105fa92919061116d565b60405160208183030381529060405290506000308583604051602001610622939291906111b3565b6040516020818303038152906040529050600080600673ffffffffffffffffffffffffffffffffffffffff168360405161065c919061122d565b600060405180830381855afa9150503d8060008114610697576040519150601f19603f3d011682016040523d82523d6000602084013e61069c565b606091505b509150915060008054906101000a900473ffffffffffffffffffffffffffffffffffffffff16600160006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff160217905550600260039081610712919061141b565b5060008161071f90611554565b60601c905060006014835161073491906115ea565b67ffffffffffffffff81111561074d5761074c610f36565b5b6040519080825280601f01601f19166020018201604052801561077f5781602001600182028036833780820191505090505b50905060005b6014845161079391906115ea565b81101561081357836014826107a8919061161e565b815181106107b9576107b8610f65565b5b602001015160f81c60f81b8282815181106107d7576107d6610f65565b5b60200101907effffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff1916908160001a9053508080600101915050610785565b50816000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff16021790555080600290816108639190611652565b5081995050505050505050505090565b60606002805461088290610eae565b80601f01602080910402602001604051908101604052809291908181526020018280546108ae90610eae565b80156108fb5780601f106108d0576101008083540402835291602001916108fb565b820191906000526020600020905b8154815290600101906020018083116108de57829003601f168201915b5050505050905090565b6000806000600673ffffffffffffffffffffffffffffffffffffffff168585604051610932929190611758565b600060405180830381855afa9150503d806000811461096d576040519150601f19603f3d011682016040523d82523d6000602084013e610972565b606091505b509150915060008054906101000a900473ffffffffffffffffffffffffffffffffffffffff16600160006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff1602179055506002600390816109e8919061141b565b506000816109f590611554565b60601c9050600060148351610a0a91906115ea565b67ffffffffffffffff811115610a2357610a22610f36565b5b6040519080825280601f01601f191660200182016040528015610a555781602001600182028036833780820191505090505b50905060005b60148451610a6991906115ea565b811015610ae95783601482610a7e919061161e565b81518110610a8f57610a8e610f65565b5b602001015160f81c60f81b828281518110610aad57610aac610f65565b5b60200101907effffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff1916908160001a9053508080600101915050610a5b565b50816000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff1602179055508060029081610b399190611652565b508194505050505092915050565b606060038054610b5690610eae565b80601f0160208091040260200160405190810160405280929190818152602001828054610b8290610eae565b8015610bcf5780601f10610ba457610100808354040283529160200191610bcf565b820191906000526020600020905b815481529060010190602001808311610bb257829003601f168201915b5050505050905090565b60028054610be690610eae565b80601f0160208091040260200160405190810160405280929190818152602001828054610c1290610eae565b8015610c5f5780601f10610c3457610100808354040283529160200191610c5f565b820191906000526020600020905b815481529060010190602001808311610c4257829003601f168201915b505050505081565b60008054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b6000600160009054906101000a900473ffffffffffffffffffffffffffffffffffffffff16905090565b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b6000610ce082610cb5565b9050919050565b610cf081610cd5565b82525050565b6000602082019050610d0b6000830184610ce7565b92915050565b600081519050919050565b600082825260208201905092915050565b60005b83811015610d4b578082015181840152602081019050610d30565b60008484015250505050565b6000601f19601f8301169050919050565b6000610d7382610d11565b610d7d8185610d1c565b9350610d8d818560208601610d2d565b610d9681610d57565b840191505092915050565b60006020820190508181036000830152610dbb8184610d68565b905092915050565b600080fd5b600080fd5b600080fd5b600080fd5b600080fd5b60008083601f840112610df257610df1610dcd565b5b8235905067ffffffffffffffff811115610e0f57610e0e610dd2565b5b602083019150836001820283011115610e2b57610e2a610dd7565b5b9250929050565b60008060208385031215610e4957610e48610dc3565b5b600083013567ffffffffffffffff811115610e6757610e66610dc8565b5b610e7385828601610ddc565b92509250509250929050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052602260045260246000fd5b60006002820490506001821680610ec657607f821691505b602082108103610ed957610ed8610e7f565b5b50919050565b6000819050919050565b6000819050919050565b610f04610eff82610edf565b610ee9565b82525050565b6000610f168285610ef3565b602082019150610f268284610ef3565b6020820191508190509392505050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b7f4e487b7100000000000000000000000000000000000000000000000000000000600052603260045260246000fd5b600081905092915050565b7f706c61696e746578743100000000000000000000000000000000000000000000600082015250565b6000610fd5600a83610f94565b9150610fe082610f9f565b600a82019050919050565b6000610ff682610fc8565b9150819050919050565b7f706c61696e746578743200000000000000000000000000000000000000000000600082015250565b6000611036600a83610f94565b915061104182611000565b600a82019050919050565b600061105782611029565b9150819050919050565b600081519050919050565b600082825260208201905092915050565b6000819050602082019050919050565b600082825260208201905092915050565b60006110a982610d11565b6110b3818561108d565b93506110c3818560208601610d2d565b6110cc81610d57565b840191505092915050565b60006110e3838361109e565b905092915050565b6000602082019050919050565b600061110382611061565b61110d818561106c565b93508360208202850161111f8561107d565b8060005b8581101561115b578484038952815161113c85826110d7565b9450611147836110eb565b925060208a01995050600181019050611123565b50829750879550505050505092915050565b6000604082019050818103600083015261118781856110f8565b9050818103602083015261119b81846110f8565b90509392505050565b6111ad81610edf565b82525050565b60006060820190506111c86000830186610ce7565b6111d560208301856111a4565b81810360408301526111e78184610d68565b9050949350505050565b600081905092915050565b600061120782610d11565b61121181856111f1565b9350611221818560208601610d2d565b80840191505092915050565b600061123982846111fc565b915081905092915050565b60008154905061125381610eae565b9050919050565b60008190508160005260206000209050919050565b60008190508160005260206000209050919050565b60006020601f8301049050919050565b600082821b905092915050565b6000600883026112d17fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff82611294565b6112db8683611294565b95508019841693508086168417925050509392505050565b6000819050919050565b600061131861131361130e84610edf565b6112f3565b610edf565b9050919050565b6000819050919050565b611332836112fd565b61134661133e8261131f565b8484546112a1565b825550505050565b600090565b61135b61134e565b611366818484611329565b505050565b5b8181101561138a5761137f600082611353565b60018101905061136c565b5050565b601f8211156113cf576113a08161125a565b6113a984611284565b810160208510156113b8578190505b6113cc6113c485611284565b83018261136b565b50505b505050565b600082821c905092915050565b60006113f2600019846008026113d4565b1980831691505092915050565b600061140b83836113e1565b9150826002028217905092915050565b818103611429575050611501565b61143282611244565b67ffffffffffffffff81111561144b5761144a610f36565b5b6114558254610eae565b61146082828561138e565b6000601f83116001811461148f576000841561147d578287015490505b61148785826113ff565b8655506114fa565b601f19841661149d8761126f565b96506114a88661125a565b60005b828110156114d0578489015482556001820191506001850194506020810190506114ab565b868310156114ed57848901546114e9601f8916826113e1565b8355505b6001600288020188555050505b5050505050505b565b6000819050602082019050919050565b60007fffffffffffffffffffffffffffffffffffffffff00000000000000000000000082169050919050565b600061154b8251611513565b80915050919050565b600061155f82610d11565b8261156984611503565b90506115748161153f565b925060148210156115b4576115af7fffffffffffffffffffffffffffffffffffffffff00000000000000000000000083601403600802611294565b831692505b5050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b60006115f582610edf565b915061160083610edf565b9250828203905081811115611618576116176115bb565b5b92915050565b600061162982610edf565b915061163483610edf565b925082820190508082111561164c5761164b6115bb565b5b92915050565b61165b82610d11565b67ffffffffffffffff81111561167457611673610f36565b5b61167e8254610eae565b61168982828561138e565b600060209050601f8311600181146116bc57600084156116aa578287015190505b6116b485826113ff565b86555061171c565b601f1984166116ca8661125a565b60005b828110156116f2578489015182556001820191506020850194506020810190506116cd565b8683101561170f578489015161170b601f8916826113e1565b8355505b6001600288020188555050505b505050505050565b82818337600083830152505050565b600061173f83856111f1565b935061174c838584611724565b82840190509392505050565b6000611765828486611733565b9150819050939250505056fea26469706673582212207b6fd0a0d4cd02303d170f0a0bf04ef7650f8eb3e54238c2f571b99f020b2c8664736f6c634300081e0033";

    // deploy contract
    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = bytecode;
    create["gas"] = "1500000";
    create["nonce"] = 0;
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    auto txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    std::string contractAddress = txReceipt["contractAddress"].asString();
    BOOST_REQUIRE_EQUAL( txReceipt["status"], "0x1" );

    // submit 2 transactions in different blocks
    Json::Value txGenerate;
    txGenerate["to"] = contractAddress;
    txGenerate["gas"] = "1000000";
    txGenerate["data"] = "0x52c92885";
    txGenerate["from"] = toJS( senderAddress );
    txGenerate["nonce"] = 1;

    txHash = fixture.rpcClient->eth_sendTransaction( txGenerate );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( txReceipt["status"], "0x1" );

    txGenerate["nonce"] = 2;
    txHash = fixture.rpcClient->eth_sendTransaction( txGenerate );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( txReceipt["status"], "0x1" );

    // read data from contract
    Json::Value callGetLastAddress;
    callGetLastAddress["to"] = contractAddress;
    callGetLastAddress["data"] = "0x041d5d7b";
    callGetLastAddress["from"] = toJS( senderAddress );
    dev::Address randomAddress1( dev::unpadLeft( dev::fromHex( fixture.rpcClient->eth_call( callGetLastAddress, "latest" ) ) ) );

    Json::Value callGetPreLastAddress;
    callGetPreLastAddress["to"] = contractAddress;
    callGetPreLastAddress["data"] = "0xfcab457f";
    callGetPreLastAddress["from"] = toJS( senderAddress );
    dev::Address randomAddress2( dev::unpadLeft( dev::fromHex( fixture.rpcClient->eth_call( callGetPreLastAddress, "latest" ) ) ) );

    BOOST_REQUIRE_NE( randomAddress1, randomAddress2 );

    Json::Value callGetLastSignature;
    callGetLastSignature["to"] = contractAddress;
    callGetLastSignature["data"] = "0x8482f246";
    callGetLastSignature["from"] = toJS( senderAddress );
    dev::bytes randomSignatureBytes = dev::fromHex( fixture.rpcClient->eth_call( callGetLastSignature, "latest" ) );
    dev::h256 rBytes( dev::bytes( randomSignatureBytes.begin(), randomSignatureBytes.begin() + dev::h256::size ) );
    dev::h256 sBytes( dev::bytes( randomSignatureBytes.begin() + dev::h256::size, randomSignatureBytes.begin() + 2 * dev::h256::size ) );
    dev::h256 vBytes( dev::bytes( randomSignatureBytes.begin() + 2 * dev::h256::size, randomSignatureBytes.begin() + 3 * dev::h256::size ) );
    dev::SignatureStruct randomSignature1( rBytes, sBytes, dev::h256::Arith( vBytes ).convert_to< _byte_ >() );

    Json::Value callGetPreLastSignature;
    callGetPreLastSignature["to"] = contractAddress;
    callGetPreLastSignature["data"] = "0x8ee64f36";
    callGetPreLastSignature["from"] = toJS( senderAddress );
    randomSignatureBytes = dev::fromHex( fixture.rpcClient->eth_call( callGetPreLastSignature, "latest" ) );
    rBytes = dev::h256( dev::bytes( randomSignatureBytes.begin(), randomSignatureBytes.begin() + dev::h256::size ) );
    sBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + dev::h256::size, randomSignatureBytes.begin() + 2 * dev::h256::size ) );
    vBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + 2 * dev::h256::size, randomSignatureBytes.begin() + 3 * dev::h256::size ) );
    dev::SignatureStruct randomSignature2( rBytes, sBytes, dev::h256::Arith( vBytes ).convert_to< _byte_ >() );

    BOOST_REQUIRE( randomSignature1 != randomSignature2 );

    // submit 2 transactions in one block
    fixture.rpcClient->debug_pauseConsensus( true );
    txGenerate["nonce"] = 3;
    fixture.rpcClient->eth_sendTransaction( txGenerate );

    txGenerate["nonce"] = 4;
    fixture.rpcClient->eth_sendTransaction( txGenerate );
    fixture.rpcClient->debug_pauseConsensus( false );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value latestBlock = fixture.rpcClient->eth_getBlockByNumber( "latest", "false" );
    BOOST_REQUIRE_EQUAL( latestBlock["transactions"].size(), 2 );

    // read data from contract again
    dev::Address randomAddress3( dev::unpadLeft( dev::fromHex( fixture.rpcClient->eth_call( callGetLastAddress, "latest" ) ) ) );
    dev::Address randomAddress4( dev::unpadLeft( dev::fromHex( fixture.rpcClient->eth_call( callGetPreLastAddress, "latest" ) ) ) );
    BOOST_REQUIRE_NE( randomAddress1, randomAddress3 );
    BOOST_REQUIRE_NE( randomAddress3, randomAddress4 );
    randomSignatureBytes = dev::fromHex( fixture.rpcClient->eth_call( callGetLastSignature, "latest" ) );
    rBytes = dev::h256( dev::bytes( randomSignatureBytes.begin(), randomSignatureBytes.begin() + dev::h256::size ) );
    sBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + dev::h256::size, randomSignatureBytes.begin() + 2 * dev::h256::size ) );
    vBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + 2 * dev::h256::size, randomSignatureBytes.begin() + 3 * dev::h256::size ) );
    dev::SignatureStruct randomSignature3( rBytes, sBytes, dev::h256::Arith( vBytes ).convert_to< _byte_ >() );
    randomSignatureBytes = dev::fromHex( fixture.rpcClient->eth_call( callGetPreLastSignature, "latest" ) );
    rBytes = dev::h256( dev::bytes( randomSignatureBytes.begin(), randomSignatureBytes.begin() + dev::h256::size ) );
    sBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + dev::h256::size, randomSignatureBytes.begin() + 2 * dev::h256::size ) );
    vBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + 2 * dev::h256::size, randomSignatureBytes.begin() + 3 * dev::h256::size ) );
    dev::SignatureStruct randomSignature4( rBytes, sBytes, dev::h256::Arith( vBytes ).convert_to< _byte_ >() );
    BOOST_REQUIRE( randomSignature1 != randomSignature3 );
    BOOST_REQUIRE( randomSignature3 != randomSignature4 );

    // verify data is parsed correctly
    dev::Address randomAddress = dev::Address::random();
    dev::bytes randomAddressBytes = randomAddress.asBytes();
    dev::bytes randomAddressLeftPadded( 32, 0 );
    std::copy( randomAddressBytes.begin(), randomAddressBytes.end(), randomAddressLeftPadded.begin() + 12 );
    dev::u256 randomGasLimit = dev::h256::Arith( dev::h256::random() );
    dev::bytes randomGasLimitBytes = dev::toBigEndian( randomGasLimit );

    // Build abi.encode(bytes[] args1, bytes[] args2) with 2 elements each
    // args1 elements must be at least BITE_CIPHERTEXT_MIN_LEN bytes (encrypted data)
    std::vector<dev::bytes> args1 = {
        dev::bytes( BITE_CIPHERTEXT_MIN_LEN, 0x11 ),  // First encrypted element (minimum length)
        dev::bytes( BITE_CIPHERTEXT_MIN_LEN, 0x22 )  // Second encrypted element (slightly longer)
    };
    std::vector<dev::bytes> args2 = {
        dev::fromHex("706c61696e746578743122"),  // "plaintext1"
        dev::fromHex("706c61696e746578743222")   // "plaintext2"
    };

    dev::bytes randomData = buildAbiEncodedArrays( args1, args2 );

    // Build ABI-encoded input: abi.encode(address, uint256, bytes)
    // Format: address(32) + gasLimit(32) + offset_to_bytes(32) + bytes_length(32) + bytes_data
    dev::bytes resultData;

    // address value (left-padded to 32 bytes)
    resultData.insert( resultData.end(), randomAddressLeftPadded.begin(), randomAddressLeftPadded.end() );

    // gasLimit value (32 bytes)
    resultData.insert( resultData.end(), randomGasLimitBytes.begin(), randomGasLimitBytes.end() );

    // offset to bytes data (points to position 96 = 3 * 32)
    dev::bytes dataOffset = dev::toBigEndian( dev::u256( 96 ) );
    resultData.insert( resultData.end(), dataOffset.begin(), dataOffset.end() );
    // bytes data (length + content)
    dev::bytes dataLength = dev::toBigEndian( dev::u256( randomData.size() ) );
    resultData.insert( resultData.end(), dataLength.begin(), dataLength.end() );
    resultData.insert( resultData.end(), randomData.begin(), randomData.end() );

    txGenerate["to"] = contractAddress;
    txGenerate["data"] = "0x8628f93a" + dev::toHex( dev::u256( 32 ) ) + dev::toHex( dev::u256( resultData.size() ) ) + dev::toHex( resultData );
    txGenerate["from"] = toJS( senderAddress );
    txGenerate["nonce"] = 5;
    fixture.rpcClient->eth_sendTransaction( txGenerate );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    dev::Address randomAddress5( dev::unpadLeft( dev::fromHex( fixture.rpcClient->eth_call( callGetLastAddress, "latest" ) ) ) );

    PrecompiledExecutor randomWalletExecutor = PrecompiledRegistrar::executor( "getRandomWalletAndSignatureForCTX" );
    dev::eth::PrecompiledCallContext ctx( fixture.client->number(), 0, 1, true );

    dev::bytesConstRef input( resultData.data(), resultData.size() );
    auto res = randomWalletExecutor( input, ctx );
    BOOST_REQUIRE( res.first );

    dev::Address addressFromPrecompiled( dev::bytes( res.second.begin(), res.second.begin() + dev::Address::size ) );
    randomSignatureBytes = dev::bytes( res.second.begin() + dev::Address::size, res.second.end() );
    rBytes = dev::h256( dev::bytes( randomSignatureBytes.begin(), randomSignatureBytes.begin() + dev::h256::size ) );
    sBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + dev::h256::size, randomSignatureBytes.begin() + 2 * dev::h256::size ) );
    vBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + 2 * dev::h256::size, randomSignatureBytes.begin() + 3 * dev::h256::size ) );
    dev::SignatureStruct signatureFromPrecompiled( rBytes, sBytes, dev::h256::Arith( vBytes ).convert_to< _byte_ >() );

    PrecompiledExecutor blockRandomExecutor = PrecompiledRegistrar::executor( "getBlockRandom" );
    auto vrs = dev::makeSignature( blockRandomExecutor( bytesConstRef(), ctx ).second, ctx.currentTxnIndex );
    dev::u256 gasPrice = g_skaleHost->getGasPrice( ctx.blockNumber.convert_to< unsigned >() );

    // Build expected RLP-encoded data: RLP(RLP(args1[0], args1[1]), RLP(args2[0], args2[1]))
    RLPStream args1Stream;
    args1Stream.appendList( args1.size() );
    for ( const auto& elem : args1 ) {
        args1Stream << elem;
    }

    RLPStream args2Stream;
    args2Stream.appendList( args2.size() );
    for ( const auto& elem : args2 ) {
        args2Stream << elem;
    }

    RLPStream finalStream;
    finalStream.appendList( 2 );
    finalStream.appendRaw( args1Stream.out() );
    finalStream.appendRaw( args2Stream.out() );

    dev::bytes rlpEncodedData = finalStream.out();
    rlpEncodedData.insert( rlpEncodedData.begin(),
        ON_DECRYPT_FUNCTION_SELECTOR.begin(),
        ON_DECRYPT_FUNCTION_SELECTOR.end() );

    // Create expected transaction for signature verification using RLP-encoded data
    Transaction expectedTransaction( 0, gasPrice, randomGasLimit, randomAddress, rlpEncodedData, 0 );
    dev::h256 expectedTxnHash = expectedTransaction.sha3( dev::eth::WithoutSignature );
    dev::Public expectedPublicKey = recover( vrs, expectedTxnHash );
    dev::Address expectedWalletAddress = dev::toAddress( expectedPublicKey );

    BOOST_REQUIRE( signatureFromPrecompiled == vrs );
    BOOST_REQUIRE_EQUAL( addressFromPrecompiled, expectedWalletAddress );
    BOOST_REQUIRE_EQUAL( addressFromPrecompiled, randomAddress5 );
}

BOOST_AUTO_TEST_CASE( submitCTX ) {
    JsonRpcFixture fixture( c_BITEConfigString, true, true, true, true, false, -1, {{ "contractStorageLimit", "100000" }} );

    dev::eth::g_skaleHost = fixture.client->skaleHost();

    string senderAddress = toJS( fixture.coinbase.address() );

//    pragma solidity ^0.8.13;

//    contract Precompile0x07Caller {
//        constructor() payable {}
//
//        function submitCTX() public {
//            uint256 randomNumber = uint256(keccak256(abi.encodePacked(block.timestamp, block.number))) % 250000 + 100000;
//            bytes[] memory args1 = new bytes[](2);
//            // args1 elements must be at least BITE_CIPHERTEXT_MIN_LEN bytes (276 bytes)
//            args1[0] = new bytes(276);
//            for (uint i = 0; i < 276; i++) {
//                args1[0][i] = 0x11;
//            }
//            args1[1] = new bytes(276);
//            for (uint i = 0; i < 276; i++) {
//                args1[1][i] = 0x22;
//            }
//            bytes[] memory args2 = new bytes[](2);
//            args2[0] = abi.encodePacked("plaintext1");
//            args2[1] = abi.encodePacked("plaintext2");

//            bytes memory randomBytes = abi.encode(args1, args2);
//            bytes memory input = abi.encode(address(this), randomNumber, randomBytes);

//            (bool success, bytes memory result) = address(0x06).staticcall(input);
//            require(success, "0x06 call failed");
//
//            // Extract address from first 20 bytes of result and transfer
//            address walletAddress = address(bytes20(result));
//            payable(walletAddress).transfer(1000);

//            input = abi.encode(result, address( this ), randomNumber, randomBytes);

//            ( success, result ) = address(0x07).staticcall( input );
//            require(success, "0x07 call failed");
//        }

//        function submitCTXWithInput(bytes calldata input) public {
//            (bool success, bytes memory result) = address(0x06).staticcall(input);
//            require(success, "0x06 call failed");
//
//            // Extract address from first 20 bytes of result and transfer
//            address walletAddress = address(bytes20(result));
//            payable(walletAddress).transfer(1000);

//            (address destination, uint256 gasLimit, bytes memory randomBytes) = abi.decode(input, (address, uint256, bytes));

//            bytes memory input1 = abi.encode(result, destination, gasLimit, randomBytes);

//            (success, result) = address(0x07).staticcall(input1);
//            require(success, "0x07 call failed");
//        }

//        function onDecrypt(bytes[] calldata decryptedArguments, bytes[] calldata plaintextArguments) external {
//            return;
//        }
//    }
    std::string bytecode = "60806040526112f4806100136000396000f3fe608060405234801561001057600080fd5b50600436106100415760003560e01c806357983ac8146100465780636040c1fb146100625780637372aa261461007e575b600080fd5b610060600480360381019061005b9190610888565b610088565b005b61007c6004803603810190610077919061095f565b61008e565b005b610086610296565b005b50505050565b600080600673ffffffffffffffffffffffffffffffffffffffff1684846040516100b99291906109eb565b600060405180830381855afa9150503d80600081146100f4576040519150601f19603f3d011682016040523d82523d6000602084013e6100f9565b606091505b50915091508161013e576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161013590610a61565b60405180910390fd5b60008161014a90610aea565b60601c90508073ffffffffffffffffffffffffffffffffffffffff166108fc6103e89081150290604051600060405180830381858888f19350505050158015610197573d6000803e3d6000fd5b50600080600087878101906101ac9190610d17565b9250925092506000858484846040516020016101cb9493929190610e2a565b6040516020818303038152906040529050600773ffffffffffffffffffffffffffffffffffffffff16816040516102029190610eae565b600060405180830381855afa9150503d806000811461023d576040519150601f19603f3d011682016040523d82523d6000602084013e610242565b606091505b5080975081985050508661028b576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161028290610f11565b60405180910390fd5b505050505050505050565b6000620186a06203d09042436040516020016102b3929190610f52565b6040516020818303038152906040528051906020012060001c6102d69190610fad565b6102e0919061100d565b90506000600267ffffffffffffffff8111156102ff576102fe610bfb565b5b60405190808252806020026020018201604052801561033257816020015b606081526020019060019003908161031d5790505b50905061011467ffffffffffffffff81111561035157610350610bfb565b5b6040519080825280601f01601f1916602001820160405280156103835781602001600182028036833780820191505090505b508160008151811061039857610397611041565b5b602002602001018190525060005b61011481101561041f57601160f81b826000815181106103c9576103c8611041565b5b602002602001015182815181106103e3576103e2611041565b5b60200101907effffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff1916908160001a90535080806001019150506103a6565b5061011467ffffffffffffffff81111561043c5761043b610bfb565b5b6040519080825280601f01601f19166020018201604052801561046e5781602001600182028036833780820191505090505b508160018151811061048357610482611041565b5b602002602001018190525060005b61011481101561050a57602260f81b826001815181106104b4576104b3611041565b5b602002602001015182815181106104ce576104cd611041565b5b60200101907effffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff1916908160001a9053508080600101915050610491565b506000600267ffffffffffffffff81111561052857610527610bfb565b5b60405190808252806020026020018201604052801561055b57816020015b60608152602001906001900390816105465790505b50905060405160200161056d906110c7565b604051602081830303815290604052816000815181106105905761058f611041565b5b60200260200101819052506040516020016105aa90611128565b604051602081830303815290604052816001815181106105cd576105cc611041565b5b6020026020010181905250600082826040516020016105ed929190611249565b6040516020818303038152906040529050600030858360405160200161061593929190611280565b6040516020818303038152906040529050600080600673ffffffffffffffffffffffffffffffffffffffff168360405161064f9190610eae565b600060405180830381855afa9150503d806000811461068a576040519150601f19603f3d011682016040523d82523d6000602084013e61068f565b606091505b5091509150816106d4576040517f08c379a00000000000000000000000000000000000000000000000000000000081526004016106cb90610a61565b60405180910390fd5b6000816106e090610aea565b60601c90508073ffffffffffffffffffffffffffffffffffffffff166108fc6103e89081150290604051600060405180830381858888f1935050505015801561072d573d6000803e3d6000fd5b50813089876040516020016107459493929190610e2a565b6040516020818303038152906040529350600773ffffffffffffffffffffffffffffffffffffffff168460405161077c9190610eae565b600060405180830381855afa9150503d80600081146107b7576040519150601f19603f3d011682016040523d82523d6000602084013e6107bc565b606091505b50809350819450505082610805576040517f08c379a00000000000000000000000000000000000000000000000000000000081526004016107fc90610f11565b60405180910390fd5b5050505050505050565b6000604051905090565b600080fd5b600080fd5b600080fd5b600080fd5b600080fd5b60008083601f84011261084857610847610823565b5b8235905067ffffffffffffffff81111561086557610864610828565b5b6020830191508360208202830111156108815761088061082d565b5b9250929050565b600080600080604085870312156108a2576108a1610819565b5b600085013567ffffffffffffffff8111156108c0576108bf61081e565b5b6108cc87828801610832565b9450945050602085013567ffffffffffffffff8111156108ef576108ee61081e565b5b6108fb87828801610832565b925092505092959194509250565b60008083601f84011261091f5761091e610823565b5b8235905067ffffffffffffffff81111561093c5761093b610828565b5b6020830191508360018202830111156109585761095761082d565b5b9250929050565b6000806020838503121561097657610975610819565b5b600083013567ffffffffffffffff8111156109945761099361081e565b5b6109a085828601610909565b92509250509250929050565b600081905092915050565b82818337600083830152505050565b60006109d283856109ac565b93506109df8385846109b7565b82840190509392505050565b60006109f88284866109c6565b91508190509392505050565b600082825260208201905092915050565b7f307830362063616c6c206661696c656400000000000000000000000000000000600082015250565b6000610a4b601083610a04565b9150610a5682610a15565b602082019050919050565b60006020820190508181036000830152610a7a81610a3e565b9050919050565b600081519050919050565b6000819050602082019050919050565b60007fffffffffffffffffffffffffffffffffffffffff00000000000000000000000082169050919050565b6000610ad48251610a9c565b80915050919050565b600082821b905092915050565b6000610af582610a81565b82610aff84610a8c565b9050610b0a81610ac8565b92506014821015610b4a57610b457fffffffffffffffffffffffffffffffffffffffff00000000000000000000000083601403600802610add565b831692505b5050919050565b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b6000610b7c82610b51565b9050919050565b610b8c81610b71565b8114610b9757600080fd5b50565b600081359050610ba981610b83565b92915050565b6000819050919050565b610bc281610baf565b8114610bcd57600080fd5b50565b600081359050610bdf81610bb9565b92915050565b600080fd5b6000601f19601f8301169050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b610c3382610bea565b810181811067ffffffffffffffff82111715610c5257610c51610bfb565b5b80604052505050565b6000610c6561080f565b9050610c718282610c2a565b919050565b600067ffffffffffffffff821115610c9157610c90610bfb565b5b610c9a82610bea565b9050602081019050919050565b6000610cba610cb584610c76565b610c5b565b905082815260208101848484011115610cd657610cd5610be5565b5b610ce18482856109b7565b509392505050565b600082601f830112610cfe57610cfd610823565b5b8135610d0e848260208601610ca7565b91505092915050565b600080600060608486031215610d3057610d2f610819565b5b6000610d3e86828701610b9a565b9350506020610d4f86828701610bd0565b925050604084013567ffffffffffffffff811115610d7057610d6f61081e565b5b610d7c86828701610ce9565b9150509250925092565b600082825260208201905092915050565b60005b83811015610db5578082015181840152602081019050610d9a565b60008484015250505050565b6000610dcc82610a81565b610dd68185610d86565b9350610de6818560208601610d97565b610def81610bea565b840191505092915050565b6000610e0582610b51565b9050919050565b610e1581610dfa565b82525050565b610e2481610baf565b82525050565b60006080820190508181036000830152610e448187610dc1565b9050610e536020830186610e0c565b610e606040830185610e1b565b8181036060830152610e728184610dc1565b905095945050505050565b6000610e8882610a81565b610e9281856109ac565b9350610ea2818560208601610d97565b80840191505092915050565b6000610eba8284610e7d565b915081905092915050565b7f307830372063616c6c206661696c656400000000000000000000000000000000600082015250565b6000610efb601083610a04565b9150610f0682610ec5565b602082019050919050565b60006020820190508181036000830152610f2a81610eee565b9050919050565b6000819050919050565b610f4c610f4782610baf565b610f31565b82525050565b6000610f5e8285610f3b565b602082019150610f6e8284610f3b565b6020820191508190509392505050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601260045260246000fd5b6000610fb882610baf565b9150610fc383610baf565b925082610fd357610fd2610f7e565b5b828206905092915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b600061101882610baf565b915061102383610baf565b925082820190508082111561103b5761103a610fde565b5b92915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052603260045260246000fd5b600081905092915050565b7f706c61696e746578743100000000000000000000000000000000000000000000600082015250565b60006110b1600a83611070565b91506110bc8261107b565b600a82019050919050565b60006110d2826110a4565b9150819050919050565b7f706c61696e746578743200000000000000000000000000000000000000000000600082015250565b6000611112600a83611070565b915061111d826110dc565b600a82019050919050565b600061113382611105565b9150819050919050565b600081519050919050565b600082825260208201905092915050565b6000819050602082019050919050565b600082825260208201905092915050565b600061118582610a81565b61118f8185611169565b935061119f818560208601610d97565b6111a881610bea565b840191505092915050565b60006111bf838361117a565b905092915050565b6000602082019050919050565b60006111df8261113d565b6111e98185611148565b9350836020820285016111fb85611159565b8060005b85811015611237578484038952815161121885826111b3565b9450611223836111c7565b925060208a019950506001810190506111ff565b50829750879550505050505092915050565b6000604082019050818103600083015261126381856111d4565b9050818103602083015261127781846111d4565b90509392505050565b60006060820190506112956000830186610e0c565b6112a26020830185610e1b565b81810360408301526112b48184610dc1565b905094935050505056fea2646970667358221220e3794164e411fb968cf25cc9e7fa58bc47934919a716197518e9c08a20b639dd64736f6c634300081e0033";

    // deploy contract
    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = bytecode;
    create["gas"] = "1500000";
    create["value"] = "1000000000000000000";
    create["nonce"] = 0;
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    auto txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    std::string contractAddress = txReceipt["contractAddress"].asString();
    BOOST_REQUIRE( txReceipt["status"] == "0x1" );

    // send a transaction -> BITE2 txn queue should become non-empty
    Json::Value txGenerate;
    txGenerate["to"] = contractAddress;
    txGenerate["gas"] = "1000000";
    txGenerate["data"] = "0x7372aa26";
    txGenerate["from"] = toJS( senderAddress );
    txGenerate["nonce"] = 1;

    txHash = fixture.rpcClient->eth_sendTransaction( txGenerate );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( txReceipt["status"] == "0x1" );

    BOOST_REQUIRE( fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions().size() == 1 );
    auto bite2Txn = fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions().front();
    BOOST_REQUIRE( !bite2Txn.isInvalid() );
    BOOST_REQUIRE_NE( bite2Txn.sender(), dev::ZeroAddress );
    auto to = bite2Txn.to();
    BOOST_REQUIRE_EQUAL( to, dev::Address( contractAddress ) );

    dev::bytes randomAddressBytes = dev::Address( contractAddress ).asBytes();
    dev::bytes randomAddressLeftPadded( 32, 0 );
    std::copy( randomAddressBytes.begin(), randomAddressBytes.end(), randomAddressLeftPadded.begin() + 12 );
    dev::u256 randomGasLimit = dev::h256::Arith( dev::h256::random() ) % 250000 + 100000;
    dev::bytes randomGasLimitBytes = dev::toBigEndian( randomGasLimit );

    // Build abi.encode(bytes[] args1, bytes[] args2) with 2 elements each
    // args1 elements must be at least BITE_CIPHERTEXT_MIN_LEN bytes (encrypted data)
    std::vector<dev::bytes> args1 = {
        dev::bytes( BITE_CIPHERTEXT_MIN_LEN, 0x11 ),  // First encrypted element (minimum length)
        dev::bytes( BITE_CIPHERTEXT_MIN_LEN, 0x22 )  // Second encrypted element (slightly longer)
    };
    std::vector<dev::bytes> args2 = {
        dev::fromHex("706c61696e746578743122"),  // "plaintext1"
        dev::fromHex("706c61696e746578743222")   // "plaintext2"
    };

    dev::bytes randomData = buildAbiEncodedArrays( args1, args2 );

    // Build ABI-encoded input: abi.encode(address, uint256, bytes)
    // Format: address(32) + gasLimit(32) + offset_to_bytes(32) + bytes_length(32) + bytes_data
    dev::bytes resultData;

    // address value (left-padded to 32 bytes)
    resultData.insert( resultData.end(), randomAddressLeftPadded.begin(), randomAddressLeftPadded.end() );

    // gasLimit value (32 bytes)
    resultData.insert( resultData.end(), randomGasLimitBytes.begin(), randomGasLimitBytes.end() );

    // offset to bytes data (points to position 96 = 3 * 32)
    dev::bytes dataOffset = dev::toBigEndian( dev::u256( 96 ) );
    resultData.insert( resultData.end(), dataOffset.begin(), dataOffset.end() );
    // bytes data (length + content)
    dev::bytes dataLength = dev::toBigEndian( dev::u256( randomData.size() ) );
    resultData.insert( resultData.end(), dataLength.begin(), dataLength.end() );
    resultData.insert( resultData.end(), randomData.begin(), randomData.end() );

    txGenerate["to"] = contractAddress;
    txGenerate["data"] = "0x6040c1fb" + dev::toHex( dev::u256( 32 ) ) + dev::toHex( dev::u256( resultData.size() ) ) + dev::toHex( resultData );
    txGenerate["from"] = toJS( senderAddress );
    txGenerate["nonce"] = 2;
    fixture.rpcClient->eth_sendTransaction( txGenerate );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    PrecompiledExecutor randomWalletExecutor = PrecompiledRegistrar::executor( "getRandomWalletAndSignatureForCTX" );
    dev::eth::PrecompiledCallContext ctx( fixture.client->number(), 0, 1, true );

    dev::bytesConstRef input( resultData.data(), resultData.size() );
    auto res = randomWalletExecutor( input, ctx );
    BOOST_REQUIRE( res.first );

    dev::Address addressFromPrecompiled( dev::bytes( res.second.begin(), res.second.begin() + dev::Address::size ) );
    auto randomSignatureBytes = dev::bytes( res.second.begin() + dev::Address::size, res.second.end() );
    auto rBytes = dev::h256( dev::bytes( randomSignatureBytes.begin(), randomSignatureBytes.begin() + dev::h256::size ) );
    auto sBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + dev::h256::size, randomSignatureBytes.begin() + 2 * dev::h256::size ) );
    auto vBytes = dev::h256( dev::bytes( randomSignatureBytes.begin() + 2 * dev::h256::size, randomSignatureBytes.begin() + 3 * dev::h256::size ) );
    dev::SignatureStruct signatureFromPrecompiled( rBytes, sBytes, dev::h256::Arith( vBytes ).convert_to< _byte_ >() );

    PrecompiledExecutor blockRandomExecutor = PrecompiledRegistrar::executor( "getBlockRandom" );
    auto vrs = dev::makeSignature( blockRandomExecutor( bytesConstRef(), ctx ).second, ctx.currentTxnIndex );
    dev::u256 gasPrice = g_skaleHost->getGasPrice( ctx.blockNumber.convert_to< unsigned >() );

    // Build expected RLP-encoded data: RLP(RLP(args1[0], args1[1]), RLP(args2[0], args2[1]))
    RLPStream args1Stream;
    args1Stream.appendList( args1.size() );
    for ( const auto& elem : args1 ) {
        args1Stream << elem;
    }

    RLPStream args2Stream;
    args2Stream.appendList( args2.size() );
    for ( const auto& elem : args2 ) {
        args2Stream << elem;
    }

    RLPStream finalStream;
    finalStream.appendList( 2 );
    finalStream.appendRaw( args1Stream.out() );
    finalStream.appendRaw( args2Stream.out() );

    dev::bytes rlpEncodedData = finalStream.out();

    rlpEncodedData.insert( rlpEncodedData.begin(),
        ON_DECRYPT_FUNCTION_SELECTOR.begin(),
        ON_DECRYPT_FUNCTION_SELECTOR.end() );

    // Create expected transaction for signature verification using RLP-encoded data
    Transaction expectedTransaction( 0, gasPrice, randomGasLimit, dev::Address( contractAddress ), rlpEncodedData, 0 );
    dev::h256 expectedTxnHash = expectedTransaction.sha3( dev::eth::WithoutSignature );
    dev::Public expectedPublicKey = recover( vrs, expectedTxnHash );
    dev::Address expectedWalletAddress = dev::toAddress( expectedPublicKey );

    BOOST_REQUIRE( fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions().size() == 2 );
    bite2Txn = fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions().back();
    BOOST_REQUIRE_EQUAL( bite2Txn.to(), dev::Address( contractAddress ) );
    BOOST_REQUIRE_EQUAL( bite2Txn.sender(), expectedWalletAddress );
    BOOST_REQUIRE_EQUAL( bite2Txn.gas(), randomGasLimit );
    BOOST_REQUIRE( bite2Txn.data() == rlpEncodedData );
}

#endif // BITE2

#ifdef FAIR
BOOST_AUTO_TEST_CASE( getBLSPublicKey ) {
    JsonRpcFixture fixture( c_BITEConfigString, false, false, true );

    Json::Value blsPublicKey = fixture.rpcClient->skale_getBLSPublicKey();

    BOOST_REQUIRE_EQUAL( blsPublicKey["BLSPublicKey0"], "15959969554621958245201075983340071881770733084910870228938077786643587385029" );
    BOOST_REQUIRE_EQUAL( blsPublicKey["BLSPublicKey1"], "7970122607051572307517094692346020360016825923464107614135327251488152616550" );
    BOOST_REQUIRE_EQUAL( blsPublicKey["BLSPublicKey2"], "3371162264373897025322009434717052197952692496405149486989861571246537813591" );
    BOOST_REQUIRE_EQUAL( blsPublicKey["BLSPublicKey3"], "13678625751515504401110635369790787716744686498431213713911601759809559919693" );
}

BOOST_AUTO_TEST_CASE( dencunOpcodesInConstructor ) {
    JsonRpcFixture fixture( c_BITEConfigString, false, false, true );

    // contract TLoadInConstructor {

    //     constructor(uint256 _input) {
    //         assembly {
    //             let val := tload(0x0)
    //             sstore(0x0, val)
    //         }
    //     }
    // }

    string compiled = "6080604052348015600e575f5ffd5b505f5c805f555060ac8060205f395ff3fe6080604052348015600e575f5ffd5b50600436106026575f3560e01c80636d619daa14602a575b5f5ffd5b60306044565b604051603b9190605f565b60405180910390f35b5f5481565b5f819050919050565b6059816049565b82525050565b5f60208201905060705f8301846052565b9291505056fea26469706673582212201a73df3522b78621c03ab2198ced2428e80055f4b32dc9b71881cb75acf3788e64736f6c634300081e0033";
    auto senderAddress = fixture.coinbase.address();

    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "900000";
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    try {
        fixture.rpcClient->eth_estimateGas( create, "latest" );
    } catch ( jsonrpc::JsonRpcException& ex ) {
        BOOST_CHECK_EQUAL( ex.GetCode(), 3 );
        BOOST_CHECK_EQUAL( ex.GetMessage(), "Contract uses unsupported Dencun opcode. Please ensure it is compiled for EVM <= Shanghai" );
    }

    auto txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( txReceipt["status"].asString(), std::string( "0x0" ) );
    BOOST_REQUIRE_EQUAL( txReceipt["revertReason"].asString(), std::string( "Contract uses unsupported Dencun opcode. Please ensure it is compiled for EVM <= Shanghai" ) );
}

BOOST_AUTO_TEST_CASE( dencunOpcodesInTransaction ) {
    JsonRpcFixture fixture( c_BITEConfigString, false, false, true );

    // contract DencunContract {
    //     uint256 public storedValue;

    //     function test(uint256 _input) external {
    //         assembly {
    //             tstore(0x0, _input)

    //             let val := tload(0x0)

    //             sstore(0x0, val)
    //         }
    //     }

    //     function mcopyTest() public pure returns (uint256) {
    //         uint256[] memory source = new uint256[](3);
    //         source[0] = 10;
    //         source[1] = 20;
    //         source[2] = 30;

    //         uint256[] memory destination = new uint256[](3);

    //         assembly {
    //             let length := mul(3, 32) // 3 elements * 32 bytes each

    //             mcopy(destination, add(source, 32), length)
    //         }

    //         return destination[0];
    //     }
    // }

    string compiled = "6080604052348015600e575f5ffd5b506102f58061001c5f395ff3fe608060405234801561000f575f5ffd5b506004361061003f575f3560e01c806325c696111461004357806329e99f07146100615780636d619daa1461007d575b5f5ffd5b61004b61009b565b60405161005891906101f3565b60405180910390f35b61007b6004803603810190610076919061023a565b6101ca565b005b6100856101d6565b60405161009291906101f3565b60405180910390f35b5f5f600367ffffffffffffffff8111156100b8576100b7610265565b5b6040519080825280602002602001820160405280156100e65781602001602082028036833780820191505090505b509050600a815f815181106100fe576100fd610292565b5b6020026020010181815250506014816001815181106101205761011f610292565b5b602002602001018181525050601e8160028151811061014257610141610292565b5b6020026020010181815250505f600367ffffffffffffffff81111561016a57610169610265565b5b6040519080825280602002602001820160405280156101985781602001602082028036833780820191505090505b50905060206003028060208401835e50805f815181106101bb576101ba610292565b5b60200260200101519250505090565b805f5d5f5c805f555050565b5f5481565b5f819050919050565b6101ed816101db565b82525050565b5f6020820190506102065f8301846101e4565b92915050565b5f5ffd5b610219816101db565b8114610223575f5ffd5b50565b5f8135905061023481610210565b92915050565b5f6020828403121561024f5761024e61020c565b5b5f61025c84828501610226565b91505092915050565b7f4e487b71000000000000000000000000000000000000000000000000000000005f52604160045260245ffd5b7f4e487b71000000000000000000000000000000000000000000000000000000005f52603260045260245ffdfea2646970667358221220879e6b5abbaea0d21b84075bdae00106284ac238ab5826f8e7e6519d744550b464736f6c634300081e0033";
    auto senderAddress = fixture.coinbase.address();

    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = compiled;
    create["gas"] = "900000";
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    auto txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    std::string contractAddress = txReceipt["contractAddress"].asString();

    // Call DencunContract.test(111)
    Json::Value sampleTx;
    sampleTx["data"] = "0x29e99f07000000000000000000000000000000000000000000000000000000000000006f";
    sampleTx["to"] = contractAddress;

    try {
        fixture.rpcClient->eth_estimateGas( sampleTx, "latest" );
    } catch ( jsonrpc::JsonRpcException& ex ) {
        BOOST_CHECK_EQUAL( ex.GetCode(), 3 );
        BOOST_CHECK_EQUAL( ex.GetMessage(), "Contract uses unsupported Dencun opcode. Please ensure it is compiled for EVM <= Shanghai" );
    }

    try {
        fixture.rpcClient->eth_call( sampleTx, "latest" );
    } catch ( jsonrpc::JsonRpcException& ex ) {
        BOOST_CHECK_EQUAL( ex.GetCode(), 3 );
        BOOST_CHECK_EQUAL( ex.GetMessage(), "Contract uses unsupported Dencun opcode. Please ensure it is compiled for EVM <= Shanghai" );
    }

    txHash = fixture.rpcClient->eth_sendTransaction( sampleTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( txReceipt["status"].asString(), std::string( "0x0" ) );
    BOOST_REQUIRE_EQUAL( txReceipt["revertReason"].asString(), std::string( "Contract uses unsupported Dencun opcode. Please ensure it is compiled for EVM <= Shanghai" ) );

    // Call DencunContract.mcopyTest()
    sampleTx["data"] = "0x25c69611";
    txHash = fixture.rpcClient->eth_sendTransaction( sampleTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( txReceipt["status"].asString(), std::string( "0x0" ) );
    BOOST_REQUIRE_EQUAL( txReceipt["revertReason"].asString(), std::string( "Contract uses unsupported Dencun opcode. Please ensure it is compiled for EVM <= Shanghai" ) );
}

#endif // FAIR

BOOST_AUTO_TEST_CASE( importInvalidBITETransaction ) {
    JsonRpcFixture fixture( c_BITEConfigString, false, false, true, true );

    string senderAddress = toJS( fixture.coinbase.address() );
    size_t nonce = 0;
    std::string biteAddress = "0x" + std::string( BITE_ADDRESS_AS_STRING );

    /// Normal valid BITE transaction -> should not throw
    std::string message =
        h256::random().hex() + std::string( "5EdF1e852fdD1B0Bc47C0307EF755C76f4B9c251" );
    auto messageBytes = libBLS::ThresholdUtils::hexCStringToBytes( message.c_str() );

    auto biteInfo = fixture.rpcClient->bite_getCommitteesInfo();
    auto blsPublicKey = biteInfo[0]["commonBLSPublicKey"].asString();
    u256 epochId = biteInfo[0]["epochId"].asUInt64();

    auto encryptedMessage =
        libBLS::ThresholdEncryption::encrypt( messageBytes, libBLS::TEPublicKey( blsPublicKey, libBLS::Base::HEXA ) );
    auto encryptedBytes = encryptedMessage.toBytes();

    auto dataField = formBITEPayloadRlp( epochId, encryptedBytes );

    auto validBITETransactionRlp =
        formTransactionRlp( fixture, senderAddress, dataField, nonce, biteAddress );
    BOOST_REQUIRE_NO_THROW( fixture.rpcClient->eth_sendRawTransaction( validBITETransactionRlp ) );

    /// Spoiling the BITE address -> should not throw any excpetion because txn is not
    /// BITE-formatted
    auto spoiledBiteAddress =
        libBLS::ThresholdUtils::hexCStringToBytes( std::string( BITE_ADDRESS_AS_STRING ).c_str() );
    size_t idxToSpoil = rand() % 20;
    spoiledBiteAddress[idxToSpoil]++;
    std::string spoiledBiteAddressHex =
        libBLS::ThresholdUtils::bytesToHexString( spoiledBiteAddress );

    auto validNonBITETransactionRlp =
        formTransactionRlp( fixture, senderAddress, dataField, nonce, spoiledBiteAddressHex );
    BOOST_REQUIRE_NO_THROW(
        fixture.rpcClient->eth_sendRawTransaction( validNonBITETransactionRlp ) );

    /// Provide wrong epochId -> txn is not validated - should throw an exception
    auto dataFieldWrongEpochId = formBITEPayloadRlp( epochId + 1, encryptedBytes );
    auto invalidBITETransactionRlp =
        formTransactionRlp( fixture, senderAddress, dataFieldWrongEpochId, nonce, biteAddress );
    BOOST_REQUIRE_THROW(
        fixture.client->importTransaction( Transaction(
            dev::jsToBytes( invalidBITETransactionRlp ), CheckTransaction::None, false ) ),
        dev::eth::InvalidBITETransaction );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( invalidBITETransactionRlp ),
        jsonrpc::JsonRpcException );

    /// No data in the data field -> data is bad formatted - should throw an exception
    invalidBITETransactionRlp =
        formTransactionRlp( fixture, senderAddress, "", nonce, biteAddress );
    BOOST_REQUIRE_THROW(
        fixture.client->importTransaction( Transaction(
            dev::jsToBytes( invalidBITETransactionRlp ), CheckTransaction::None, false ) ),
        dev::eth::InvalidBITETransaction );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( invalidBITETransactionRlp ),
        jsonrpc::JsonRpcException );

    RLPStream tooShortRlp;

    /// Only epoch in data field -> data is too short - should throw an exception
    tooShortRlp.appendList( 1 );
    tooShortRlp << epochId;
    dev::bytes invalidTooShortBITETxnData = tooShortRlp.out();
    invalidBITETransactionRlp =
        formTransactionRlp( fixture, senderAddress,
                            dev::toHexPrefixed( invalidTooShortBITETxnData ), nonce, biteAddress );
    BOOST_REQUIRE_THROW(
        fixture.client->importTransaction( Transaction(
            dev::jsToBytes( invalidBITETransactionRlp ), CheckTransaction::None, false ) ),
        dev::eth::InvalidBITETransaction );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( invalidBITETransactionRlp ),
        jsonrpc::JsonRpcException );
    tooShortRlp.clear();

    /// Only epoch + key -> no data - should throw an exception
    encryptedMessage.data = std::make_shared< dev::bytes >();
    tooShortRlp.appendList( 2 );
    tooShortRlp << epochId;
    tooShortRlp << encryptedMessage.toBytes();
    invalidTooShortBITETxnData = tooShortRlp.out();
    invalidBITETransactionRlp =
        formTransactionRlp( fixture, senderAddress,
                            dev::toHexPrefixed( invalidTooShortBITETxnData ), nonce, biteAddress );
    BOOST_REQUIRE_THROW(
        fixture.client->importTransaction( Transaction(
            dev::jsToBytes( invalidBITETransactionRlp ), CheckTransaction::None, false ) ),
        dev::eth::BITETransactionTooShort );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( invalidBITETransactionRlp ),
        jsonrpc::JsonRpcException );

    /// Spoiling key part of ciphertext
    auto randomEncryptedKeyObj = libBLS::CipheredKey::random();
    randomEncryptedKeyObj.V = encryptedMessage.keys[0].V;
    auto randomEncryptedKeyByteArray = randomEncryptedKeyObj.toBytes();
    auto spoiledMessageBytes = encryptedBytes;
    // overwrite key part
    std::copy( randomEncryptedKeyByteArray.begin(), randomEncryptedKeyByteArray.end(),
        spoiledMessageBytes.begin() );

    RLPStream spoiledBITEDataRlp;
    spoiledBITEDataRlp.appendList( 2 );
    spoiledBITEDataRlp << epochId;
    spoiledBITEDataRlp << spoiledMessageBytes;
    dev::bytes invalidBITETxnData = spoiledBITEDataRlp.out();

    invalidBITETransactionRlp =
        formTransactionRlp( fixture, senderAddress,
                            dev::toHexPrefixed( invalidBITETxnData ), nonce, biteAddress );
    BOOST_REQUIRE_THROW(
        fixture.client->importTransaction( Transaction(
            dev::jsToBytes( invalidBITETransactionRlp ), CheckTransaction::None, false ) ),
        dev::eth::InvalidBITETransaction );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( invalidBITETransactionRlp ),
        jsonrpc::JsonRpcException );
    spoiledBITEDataRlp.clear();

    /// Encrypted key is not well formed -> should throw exception
    randomEncryptedKeyObj.U.setXC0( libBLS::algebra::FqElement::random() );
    randomEncryptedKeyObj.W.setY( libBLS::algebra::FqElement::random() );
    randomEncryptedKeyByteArray = randomEncryptedKeyObj.toBytes();
    std::copy( randomEncryptedKeyByteArray.begin(), randomEncryptedKeyByteArray.end(),
        spoiledMessageBytes.begin() );

    spoiledBITEDataRlp.appendList( 2 );
    spoiledBITEDataRlp << epochId;
    spoiledBITEDataRlp << spoiledMessageBytes;
    invalidBITETxnData = spoiledBITEDataRlp.out();

    invalidBITETransactionRlp =
        formTransactionRlp( fixture, senderAddress,
                            dev::toHexPrefixed( invalidBITETxnData ), nonce, biteAddress );
    BOOST_REQUIRE_THROW(
        fixture.client->importTransaction( Transaction(
            dev::jsToBytes( invalidBITETransactionRlp ), CheckTransaction::None, false ) ),
        dev::eth::InvalidBITETransaction );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( invalidBITETransactionRlp ),
        jsonrpc::JsonRpcException );

    // now send BITE txn with multiple epochIds / encryptedAESKeys
    libBLS::TEPublicKey publicKey2 = libBLS::TEPublicKey::random();
    u256 epochId2 = epochId + 5;

    encryptedMessage = libBLS::ThresholdEncryption::encrypt( messageBytes, { libBLS::TEPublicKey( blsPublicKey, libBLS::Base::HEXA ), publicKey2 } );
    auto encryptedBITEDataBytes = encryptedMessage.toBytes();

    // Create payload with 2 encrypted AES keys
    RLPStream bitePayload;
    bitePayload.appendList( 2 );
    bitePayload << epochId;
    bitePayload << encryptedBITEDataBytes;

    auto rlpBytes = bitePayload.out();
    dev::bytes twoPayloadBITETxnData = dev::bytes( rlpBytes.begin(), rlpBytes.end() );

    validBITETransactionRlp =
            formTransactionRlp( fixture, senderAddress,
                                dev::toHexPrefixed( twoPayloadBITETxnData ), nonce, biteAddress );
    BOOST_REQUIRE_NO_THROW( fixture.rpcClient->eth_sendRawTransaction( validBITETransactionRlp ) );

    // 3 elements in payload is not allowed

    RLPStream threeElementsPayload;
    threeElementsPayload.appendList( 3 );
    threeElementsPayload << epochId;
    threeElementsPayload << epochId2;
    threeElementsPayload << encryptedBITEDataBytes;

    auto threeElementsRlpBytes = threeElementsPayload.out();
    dev::bytes threeElementsBITETxnData = dev::bytes( threeElementsRlpBytes.begin(),
                                                      threeElementsRlpBytes.end() );

    std::string threeElementsBITETxnRlp =
            formTransactionRlp( fixture, senderAddress,
                                dev::toHexPrefixed( threeElementsBITETxnData ), nonce, biteAddress );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( threeElementsBITETxnRlp ),
                         jsonrpc::JsonRpcException );

    // epochId doesn't match and only 1 encrypted AES keys
    libBLS::TEPublicKey publicKey3 = libBLS::TEPublicKey::random();

    auto encryptedMessage1Key = libBLS::ThresholdEncryption::encrypt( messageBytes, publicKey3 );
    auto encryptedBITEDataBytes1Key = encryptedMessage1Key.toBytes();

    RLPStream mismatchPayload;
    mismatchPayload.appendList( 2 );
    mismatchPayload << epochId2;
    mismatchPayload << encryptedBITEDataBytes1Key;

    auto mismatchRlpBytes = mismatchPayload.out();
    dev::bytes mismatchBITETxnData = dev::bytes( mismatchRlpBytes.begin(), mismatchRlpBytes.end() );

    std::string mismatchBITETxnRlp =
            formTransactionRlp( fixture, senderAddress,
                                dev::toHexPrefixed( mismatchBITETxnData ), nonce, biteAddress );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( mismatchBITETxnRlp ),
                         jsonrpc::JsonRpcException );

    // 2 encrypted AES keys submitted, but one key is corrupt
    auto corruptEncryptedMessage = libBLS::ThresholdEncryption::encrypt( messageBytes, { libBLS::TEPublicKey( blsPublicKey, libBLS::Base::HEXA ), publicKey2 } );

    // Corrupt the first key by replacing it with a random one
    corruptEncryptedMessage.keys[0] = libBLS::CipheredKey(
        libBLS::algebra::G2Point::random(),
        corruptEncryptedMessage.keys[0].V,
        libBLS::algebra::G1Point::random()
    );

    auto corruptEncryptedBITEDataBytes = corruptEncryptedMessage.toBytes();

    RLPStream corruptPayload;
    corruptPayload.appendList( 2 );
    corruptPayload << epochId;
    corruptPayload << corruptEncryptedBITEDataBytes;

    auto corruptRlpBytes = corruptPayload.out();
    dev::bytes corruptBITETxnData = dev::bytes( corruptRlpBytes.begin(), corruptRlpBytes.end() );

    std::string corruptBITETxnRlp =
            formTransactionRlp( fixture, senderAddress,
                                dev::toHexPrefixed( corruptBITETxnData ), nonce, biteAddress );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( corruptBITETxnRlp ),
                         jsonrpc::JsonRpcException );
}

BOOST_AUTO_TEST_CASE( BITETransactionCouldNotBeDecrypted ) {
    JsonRpcFixture fixture( c_BITEConfigString, false, false, true, true );

    string senderAddress = toJS( fixture.coinbase.address() );

    // address 0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f is preset in config
    auto balanceBefore =
        fixture.rpcClient->eth_getBalance( "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f", "latest" );
    auto balanceBeforeU256 = dev::jsToU256( balanceBefore );
    BOOST_REQUIRE(
        balanceBeforeU256 == dev::u256( dev::bigint( "1000000000000000000000000000000" ) ) );

    // data must have the destination address and the original message
    RLPStream biteDataRlp( 2 );
    biteDataRlp << ( dev::h256::Arith ) h256::random();
    biteDataRlp << ( dev::Address::Arith ) dev::Address( "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f" );

    auto messageBytes = biteDataRlp.out();

    auto biteInfo = fixture.rpcClient->bite_getCommitteesInfo();
    auto blsPublicKey = biteInfo[0]["commonBLSPublicKey"].asString();
    u256 epochId = biteInfo[0]["epochId"].asUInt64();

    auto ciphertext =
        libBLS::ThresholdEncryption::encrypt( messageBytes, libBLS::TEPublicKey( blsPublicKey, libBLS::Base::HEXA ) );
    auto ciphertextBytes = ciphertext.toBytes();

    // spoil random element in decryptedData
    // only tamper the data part
    // | -- Number of keys --| | --- KEY ---- | | --- Data - tamper this part ----|
    size_t idxToSpoil =
        1 + libBLS::CipheredKey::CIPHERED_KEY_SIZE_BYTES +
        rand() % ( ciphertextBytes.size() - libBLS::CipheredKey::CIPHERED_KEY_SIZE_BYTES );
    ciphertextBytes[idxToSpoil]++;

    auto invalidEncryptedData = libBLS::ThresholdUtils::bytesToHexString( ciphertextBytes );

    size_t nonce = 0;

    RLPStream bitePayloadRlp( 2 );

    bitePayloadRlp << epochId;
    bitePayloadRlp << ciphertextBytes;

    auto rlpBytes = bitePayloadRlp.out();
    std::string biteAddress = "0x" + std::string( BITE_ADDRESS_AS_STRING );
    std::string txnRlp = formTransactionRlp(
        fixture, "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f", dev::toHexPrefixed( rlpBytes ),
                nonce, biteAddress );

    Transaction t( dev::fromHex( txnRlp ), dev::eth::CheckTransaction::None );
    auto minGasRequired = t.baseGasRequired( fixture.client->evmSchedule() );

    auto gasPrice = fixture.rpcClient->eth_gasPrice();
    auto invalidTxnHash = fixture.rpcClient->eth_sendRawTransaction( txnRlp );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    auto balanceAfter =
        fixture.rpcClient->eth_getBalance( "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f", "latest" );
    BOOST_REQUIRE_EQUAL( balanceAfter,
                   dev::toJS( balanceBeforeU256 - minGasRequired * dev::jsToU256( gasPrice ) ) );

    try {
        fixture.rpcClient->bite_getDecryptedTransactionData( invalidTxnHash );
    } catch ( const jsonrpc::JsonRpcException& ex ) {
        std::string errorMessage =
            "Transaction with provided hash does not have any decrypted data associated with it.";
        BOOST_REQUIRE( ex.what() == errorMessage );
    }

    auto receipt = fixture.rpcClient->eth_getTransactionReceipt( invalidTxnHash );
    BOOST_REQUIRE(
        receipt["revertReason"] == std::string( "Could not decrypt BITE transaction." ) );
}


BOOST_AUTO_TEST_CASE( getDecryptedTransactionData ) {
    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    // enable type1 & type2 txs
    ret["skaleConfig"]["sChain"]["EIP1559TransactionsPatchTimestamp"] = 1;
    // Set chainID = 151
    std::string chainID = "0x97";
    ret["params"]["chainID"] = chainID;
#ifndef FAIR
    // set contractStorageLimit
    ret["skaleConfig"]["sChain"]["contractStorageLimit"] = 1000000;
#endif

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config, true, true, false, false, false, -1, {{ "contractStorageLimit", "1000000" }} );

    dev::eth::simulateMining( *( fixture.client ), 20 );
    string senderAddress = toJS( fixture.coinbase.address() );

    int nonce = 0;

    // ---- Legacy -----
    Json::Value legacyTx;

    dev::Address originalToAddress( "0x5edf1e852fdd1b0bc47c0307ef755c76f4b9c251" );
    std::string plaintext =
        "0x6057361d0000000000000000000000000000000000000000000000000000000000000001";
    std::string encryptedDataPlusToAddressLegacy = dev::toHexPrefixed(
                formEncryptedMessageMockup( dev::fromHex( plaintext ), originalToAddress ) );

    // signal BITE tx
    legacyTx["to"] = toJS( "0x" + std::string( BITE_ADDRESS_AS_STRING ) );
    legacyTx["from"] = senderAddress;
    legacyTx["gas"] = "100000";
    legacyTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    legacyTx["value"] = "20000000000000000000";  // send all fixture.coinbase.address() balance to
                                                 // cover for next calls
    legacyTx["data"] = encryptedDataPlusToAddressLegacy;
    legacyTx["nonce"] = nonce++;

    TransactionSkeleton legacyTs = toTransactionSkeleton( legacyTx );
    legacyTs = fixture.client->populateTransactionWithDefaults( legacyTs );
    pair< bool, Secret > legacyAr = fixture.accountHolder->authenticate( legacyTs );
    Transaction legacyTransaction( legacyTs, legacyAr.second );

    std::string legacyRLP = dev::toHexPrefixed( legacyTransaction.toBytes() );
    std::string legacyHash = fixture.rpcClient->eth_sendRawTransaction( legacyRLP );

    dev::eth::mineTransaction( *( fixture.client ), 1 );

    auto legacyTxReceipt = fixture.rpcClient->eth_getTransactionReceipt( legacyHash );
    BOOST_REQUIRE( legacyTxReceipt["status"].asString() == std::string( "0x1" ) );
    BOOST_REQUIRE(
        legacyTxReceipt["blockNumber"].asString() == fixture.rpcClient->eth_blockNumber() );
    BOOST_REQUIRE( legacyTxReceipt["to"].asString() == "0x" + originalToAddress.hex() );

    auto legacyEncryptedResponse = fixture.rpcClient->eth_getTransactionByHash( legacyHash );
    BOOST_REQUIRE(
        legacyEncryptedResponse["input"].asString() == encryptedDataPlusToAddressLegacy );

    auto legacyDecryptedResponse =
        fixture.rpcClient->bite_getDecryptedTransactionData( legacyHash );
    BOOST_REQUIRE( legacyDecryptedResponse["data"] == plaintext );
    BOOST_REQUIRE( legacyDecryptedResponse["to"] == "0x" + originalToAddress.hex() );

    // ---- Type1 tx -----
    /*
        transaction1['nonce'] = 0
        transaction1['gasPrice'] = 20000000000
        transaction1['gas'] = 80000
        transaction1['to'] = 0xc868af52a6549c773082a334e5ae232e0ea3b513
        transaction1['value'] = 0
        transaction1['chainId'] = 151
        transaction1['type'] = 1
        transaction1['data'] = call to SC
    */
    std::string originalToAddressType1 = "c868af52a6549c773082a334e5ae232e0ea3b513";
    // data ciphered from a single run of formEncryptedMessageMockup( plaintext, originalToAddress )
    // since it differs each run, and the RLP-encoded tx was built outside this test case (via an
    // external script), we need to set this manually Note that the encryptedData includes the 'To'
    // address already
    std::string encryptedDataPlusToAddressType1 = "0xf9015d80b901590192084354e0f043e108c255d159de7360e5a972bacdbaf3257420f66478d79b930000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005b87049973afc70916c3588f2e5bcdd65acdc73f544d201979b4ce8307815fda7281e032402b0e1d82ccc830046e5de0c7a2e8f9fd4ea6dcd9e85230a5f278373f5c873b1ed1bae995c74899927c92b8e2b41af3adfba46def6b857a4e74b7595e2bb9c84773ba4a1167fc73bd17ca65334d12eaf9401897";
    std::string type1Tx = "0x01f901ca8197808504a817c800830138809442495445204d452049274d20454e43525950544480b90160f9015d80b901590192084354e0f043e108c255d159de7360e5a972bacdbaf3257420f66478d79b930000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005b87049973afc70916c3588f2e5bcdd65acdc73f544d201979b4ce8307815fda7281e032402b0e1d82ccc830046e5de0c7a2e8f9fd4ea6dcd9e85230a5f278373f5c873b1ed1bae995c74899927c92b8e2b41af3adfba46def6b857a4e74b7595e2bb9c84773ba4a1167fc73bd17ca65334d12eaf9401897c001a05b144ba5643c7ff31cfefdeaf4043e222c0c32f5c849c21598f89e82abdea07fa048762844d57807403777d655dec566230266759e173c187e4cecebfa4579ea66";

    std::string type1Hash = fixture.rpcClient->eth_sendRawTransaction( type1Tx );

    dev::eth::mineTransaction( *( fixture.client ), 1 );

    auto type1TxReceipt = fixture.rpcClient->eth_getTransactionReceipt( type1Hash );
    BOOST_REQUIRE( type1TxReceipt["status"].asString() == std::string( "0x1" ) );
    BOOST_REQUIRE(
        type1TxReceipt["blockNumber"].asString() == fixture.rpcClient->eth_blockNumber() );
    BOOST_REQUIRE( type1TxReceipt["to"].asString() == "0x" + originalToAddressType1 );

    auto type1EncryptedResponse = fixture.rpcClient->eth_getTransactionByHash( type1Hash );
    BOOST_REQUIRE( type1EncryptedResponse["input"].asString() == encryptedDataPlusToAddressType1 );

    auto type1DecryptedResponse = fixture.rpcClient->bite_getDecryptedTransactionData( type1Hash );
    BOOST_REQUIRE( type1DecryptedResponse["data"] == plaintext );
    BOOST_REQUIRE( type1DecryptedResponse["to"] == "0x" + originalToAddressType1 );

    // ---- Type2 tx -----
    /*
        transaction1['nonce'] = 1
        transaction1['gas'] = 80000
        transaction1['maxFeePerGas'] = 20000000000
        transaction1['maxPriorityFeePerGas'] = 20000000000 - 1
        transaction1['to'] = 0xc868AF52a6549c773082A334E5AE232e0Ea3B513
        transaction1['value'] = 0
        transaction1['chainId'] = 151
        transaction1['type'] = 2
        transaction1['data'] = encryptedData
    */

    std::string originalToAddressType2 = originalToAddressType1;
    std::string encryptedDataPlusToAddressType2 = encryptedDataPlusToAddressType1;
    std::string type2Tx = "0x02f901d08197018504a817c7ff8504a817c800830138809442495445204d452049274d20454e43525950544480b90160f9015d80b901590192084354e0f043e108c255d159de7360e5a972bacdbaf3257420f66478d79b930000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005b87049973afc70916c3588f2e5bcdd65acdc73f544d201979b4ce8307815fda7281e032402b0e1d82ccc830046e5de0c7a2e8f9fd4ea6dcd9e85230a5f278373f5c873b1ed1bae995c74899927c92b8e2b41af3adfba46def6b857a4e74b7595e2bb9c84773ba4a1167fc73bd17ca65334d12eaf9401897c080a0c8512955420b554abcde1ea13d67bef9a38bf541938a8b915c514722821481f2a03f3ca19e2513078f058462c622bd07ea0b6f15b5bc28998bcc79c59c3c0400db";
    std::string type2Hash = fixture.rpcClient->eth_sendRawTransaction( type2Tx );


    dev::eth::mineTransaction( *( fixture.client ), 1 );

    auto type2TxReceipt = fixture.rpcClient->eth_getTransactionReceipt( type2Hash );
    BOOST_REQUIRE( type2TxReceipt["status"].asString() == std::string( "0x1" ) );
    BOOST_REQUIRE(
        type2TxReceipt["blockNumber"].asString() == fixture.rpcClient->eth_blockNumber() );
    BOOST_REQUIRE( type2TxReceipt["to"].asString() == "0x" + originalToAddressType2 );

    auto type2EncryptedResponse = fixture.rpcClient->eth_getTransactionByHash( type2Hash );
    BOOST_REQUIRE( type2EncryptedResponse["input"].asString() == encryptedDataPlusToAddressType2 );

    auto type2DecryptedResponse = fixture.rpcClient->bite_getDecryptedTransactionData( type2Hash );
    BOOST_REQUIRE( type2DecryptedResponse["data"] == plaintext );
    BOOST_REQUIRE( type2DecryptedResponse["to"] == "0x" + originalToAddressType2 );


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
    string bytecode =
        "608060405234801561001057600080fd5b50610155806100206000396000f3fe60806040523"
        "4801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b57"
        "80636057361d14610059575b600080fd5b610043610075565b60405161005091906100e3565"
        "b60405180910390f35b610073600480360381019061006e91906100ab565b61007e565b005b"
        "60008054905090565b80600081905550806001819055508060028190555050565b600081359"
        "0506100a581610108565b92915050565b6000602082840312156100bd57600080fd5b600061"
        "00cb84828501610096565b91505092915050565b6100dd816100fe565b82525050565b60006"
        "020820190506100f860008301846100d4565b92915050565b6000819050919050565b610111"
        "816100fe565b811461011c57600080fd5b5056fea2646970667358221220edbb1123b5e4538"
        "463747d4497720f4c0b79ff718b7bf245e6ba81dc37dc1a0364736f6c63430008040033";

    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["data"] = bytecode;  // SC creation goes in plaintext
    create["gas"] = "180000";
    std::string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == string( "0x1" ) );

    std::string contractAddress = receipt["contractAddress"].asString();
    std::string contractAddressWithout0x = contractAddress.substr( 2 );

    // verify state is empty
    Json::Value call;
    call["to"] = contractAddress;
    call["data"] = "0x2e64cec1";
    call["from"] = toJS( senderAddress );
    BOOST_REQUIRE( u256( 0 ) == dev::jsToU256( fixture.rpcClient->eth_call( call, "latest" ) ) );

    string dataStore1 = "6057361d0000000000000000000000000000000000000000000000000000000000000001";
    string dataStoreInvalid =
        "6057361e0000000000000000000000000000000000000000000000000000000000000001";

    // send txn to change state
    Json::Value store1;
    store1["to"] = toJS( "0x" + std::string( BITE_ADDRESS_AS_STRING ) );
    store1["data"] = dev::toHexPrefixed( formEncryptedMessageMockup( dev::fromHex( dataStore1 ), dev::Address( contractAddressWithout0x ) ) );
    store1["from"] = toJS( senderAddress );
    store1["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    store1["gas"] = "111000";
    txHash = fixture.rpcClient->eth_sendTransaction( store1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == std::string( "0x1" ) );

    // check that previous txn changed the state
    call["to"] = contractAddress;
    call["data"] = "0x2e64cec1";
    call["from"] = toJS( senderAddress );
    BOOST_REQUIRE( u256( 1 ) == dev::jsToU256( fixture.rpcClient->eth_call( call, "latest" ) ) );

    // send invalid call to the contract - txn should fail
    Json::Value txInvalidContractCall;
    txInvalidContractCall["to"] = toJS( "0x" + std::string( BITE_ADDRESS_AS_STRING ) );
    txInvalidContractCall["data"] = dev::toHexPrefixed( formEncryptedMessageMockup( dev::fromHex( dataStoreInvalid ), dev::Address( contractAddressWithout0x ) ) );
    txInvalidContractCall["from"] = toJS( senderAddress );
    txInvalidContractCall["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txHash = fixture.rpcClient->eth_sendTransaction( txInvalidContractCall );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == std::string( "0x0" ) );
}

#ifdef FAIR
BOOST_AUTO_TEST_CASE( committeeRotation ) {
    std::string _config = c_BITECommitteeRotationConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );

    auto firstGroupObject = ret["skaleConfig"]["sChain"]["nodes"]["1"];
    auto secondGroupObject = ret["skaleConfig"]["sChain"]["nodes"]["-1"];

    std::array< std::string, 4 > firstGroupCommonPublicKey;
    firstGroupCommonPublicKey[0] = firstGroupObject["blsKey"]["commonBLSPublicKey0"].asString();
    firstGroupCommonPublicKey[1] = firstGroupObject["blsKey"]["commonBLSPublicKey1"].asString();
    firstGroupCommonPublicKey[2] = firstGroupObject["blsKey"]["commonBLSPublicKey2"].asString();
    firstGroupCommonPublicKey[3] = firstGroupObject["blsKey"]["commonBLSPublicKey3"].asString();

    std::array< std::string, 4 > secondGroupCommonPublicKey;
    secondGroupCommonPublicKey[0] = secondGroupObject["blsKey"]["commonBLSPublicKey0"].asString();
    secondGroupCommonPublicKey[1] = secondGroupObject["blsKey"]["commonBLSPublicKey1"].asString();
    secondGroupCommonPublicKey[2] = secondGroupObject["blsKey"]["commonBLSPublicKey2"].asString();
    secondGroupCommonPublicKey[3] = secondGroupObject["blsKey"]["commonBLSPublicKey3"].asString();

    BOOST_REQUIRE( firstGroupCommonPublicKey != secondGroupCommonPublicKey );

    ret["skaleConfig"]["sChain"]["nodes"].clear();

    auto currentTime = time( nullptr );
    auto firstGroupTs = currentTime;
    auto secondGroupTs = firstGroupTs + 10;

    ret["skaleConfig"]["sChain"]["nodes"][std::to_string( firstGroupTs )] = firstGroupObject;
    ret["skaleConfig"]["sChain"]["nodes"][std::to_string( secondGroupTs )] = secondGroupObject;

    ret["skaleConfig"]["sChain"]["nodeGroups"]["0"]["finish_ts"] = secondGroupTs;

    auto blsPublicKeyStringToStringArray = [](const std::string& publicKeyStr) {
        libBLS::TEPublicKey publicKey( publicKeyStr, libBLS::Base::HEXA );
        auto rawPublicKey = publicKey.getPublicKeyRaw();
        return rawPublicKey.toStringArray( libBLS::Base::DEC );
    };

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config, false, false, true );

    Json::Value txRefill;
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["from"] = toJS( fixture.coinbase.address() );
    txRefill["gas"] = "100000";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["value"] = 100000000000000000;
    string txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    auto receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == std::string( "0x1" ) );

    auto latestBlockTs = fixture.client->blockChain().info().timestamp();
    BOOST_REQUIRE( latestBlockTs < secondGroupTs && latestBlockTs > firstGroupTs );
    BOOST_REQUIRE( fixture.client->chainParams().getCommonBlsPublicKey() == firstGroupCommonPublicKey );
    BOOST_REQUIRE( fixture.client->isCommitteeRotationSoon() );

    auto biteInfo = fixture.rpcClient->bite_getCommitteesInfo();
    BOOST_REQUIRE( biteInfo.isArray() );
    BOOST_REQUIRE_EQUAL( biteInfo.size(), 2 );
    BOOST_REQUIRE( blsPublicKeyStringToStringArray( biteInfo[0]["commonBLSPublicKey"].asString() ) == firstGroupCommonPublicKey );
    BOOST_REQUIRE_EQUAL( biteInfo[0]["epochId"].asUInt64(), 0 );
    BOOST_REQUIRE( blsPublicKeyStringToStringArray( biteInfo[1]["commonBLSPublicKey"].asString() ) == secondGroupCommonPublicKey );
    BOOST_REQUIRE_EQUAL( biteInfo[1]["epochId"].asUInt64(), 1 );

    while ( latestBlockTs++ < secondGroupTs )
        sleep( 1 );

    txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == std::string( "0x1" ) );

    BOOST_REQUIRE( latestBlockTs >= secondGroupTs );
    BOOST_REQUIRE( fixture.client->chainParams().getCommonBlsPublicKey() == secondGroupCommonPublicKey );

    biteInfo = fixture.rpcClient->bite_getCommitteesInfo();
    BOOST_REQUIRE_EQUAL( biteInfo.size(), 1 );
    BOOST_REQUIRE( blsPublicKeyStringToStringArray( biteInfo[0]["commonBLSPublicKey"].asString() ) == secondGroupCommonPublicKey );
    BOOST_REQUIRE_EQUAL( biteInfo[0]["epochId"].asUInt64(), 1 );
    BOOST_REQUIRE( !fixture.client->isCommitteeRotationSoon() );

    // HACK: currently on committee rotation skaled calls exitGracefully in consensus
    // it interferes with exit procedure
    // put sleep here to avoid collisions
    sleep( 10 );

    txHash = fixture.rpcClient->eth_sendTransaction( txRefill );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( receipt["status"] == std::string( "0x1" ) );
}


BOOST_AUTO_TEST_CASE( fetchingBlockRewardBeneficiary ) {
    std::string config = c_BITECommitteeRotationConfigString;
    Json::Value ret;
    Json::Reader().parse( config, ret );

    Json::FastWriter fastWriter;
    config = fastWriter.write( ret );
    JsonRpcFixture fixture( config, false, false, true );

    Address rewardWalletAddress = fixture.client->chainParams().getNodeBeneficiaryInHistoricGroup( 0, 1 );
    BOOST_REQUIRE( rewardWalletAddress == Address( "0x08151B8F80bfa7dEa760e461412AF24348224edf" )  );
    rewardWalletAddress = fixture.client->chainParams().getNodeBeneficiaryInHistoricGroup( 1, 1 );
    BOOST_REQUIRE( rewardWalletAddress == Address( "0x405c96D388cDFBa4f17493c875CCE9c680225276" )  );
}

BOOST_AUTO_TEST_CASE( block_author_balance ) {
    // when rewardWalletAddress is not defined

    // for testBlockRewardsActivationPatchAddress
    setenv( "TEST_BLOCK_REWARDS_ACTIVATION", "1", 1 );

    std::string _config = c_genesisConfigString;
    Json::Value ret;
    Json::Reader().parse( _config, ret );
    // Set FAIR chainID
    std::string chainID = "0x3a6";
    ret["params"]["chainID"] = chainID;
    // disable block rewards at startup
    ret["params"]["blockReward"] = "0x00";

    Json::FastWriter fastWriter;
    std::string config = fastWriter.write( ret );
    JsonRpcFixture fixture( config, false, false, true );

    string etherbase = fixture.rpcClient->eth_coinbase();

    BOOST_REQUIRE( !BlockRewardsActivationPatch::isEnabled( fixture.client->chainId() ) );

    // checksumed address: 0x0E7d7F1D34a502bD609542576941C3FCc087c588
    auto node_owner = "0x0e7d7f1d34a502bd609542576941c3fcc087c588";

    auto etherbase_address = jsToAddress( etherbase );

    u256 etherbaseBalance = fixture.client->balanceAt( etherbase_address );
    BOOST_REQUIRE_EQUAL( etherbaseBalance, 0 );

    dev::Address stakingContractAddress = fixture.client->chainParams().getStakingContractAddress();
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( stakingContractAddress ), 0 );

    // mine transaction not from testBlockRewardsActivationPatchAddress - block rewards should stay disabled
    Json::Value silentTx;
    silentTx["value"] = 1;
    // address has preset balance in config
    // silentTx["from"] = dev::Address( "0x5C4e11842E8be09264dc1976943571d7Af6d00F9" ).hex();
    silentTx["to"] = BlockRewardsActivationPatch::getMagicAddress().hex();
    silentTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    silentTx["gas"] = 30000;
    silentTx["nonce"] = 0;

    auto silentTs = toTransactionSkeleton( silentTx );
    auto silentT = dev::eth::Transaction(
        silentTs, dev::Secret( "1c2cd4b70c2b8c6cd7144bbbfbd1e5c6eacb4a5efd9c86d0e29cbbec4e8483b9" ) );

    std::string txHash = fixture.rpcClient->eth_sendRawTransaction( dev::toHex( silentT.toBytes() ) );
    BOOST_REQUIRE( !txHash.empty() );

    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"] == "0x1" );
    BOOST_REQUIRE( !BlockRewardsActivationPatch::isEnabled( fixture.client->chainId() ) );
    // staking contract balance only gets changed when block rewards are activated
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( stakingContractAddress ), 0 );
    etherbaseBalance = fixture.client->balanceAt( etherbase_address );
    BOOST_REQUIRE_EQUAL( etherbaseBalance, 0 );

    // mine transaction from testBlockRewardsActivationPatchAddress - block rewards should enable
    Json::Value activationTx;
    // activationTx["from"] = "0x5339Ef05428d1b87f4e2F2db64E782c68E9cDA56";
    activationTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    activationTx["gas"] = 30000;
    activationTx["chainId"] = "0x3a9";
    activationTx["nonce"] = 0;
    activationTx["to"] = dev::Address::random().hex();

    auto ts = toTransactionSkeleton( activationTx );
    auto t = dev::eth::Transaction(
        ts, dev::Secret( "0e394ff21db60660a27a6383aedf8c75070648965acbef7c369c1bae2141a485" ) );

    txHash = fixture.rpcClient->eth_sendRawTransaction( dev::toHex( t.toBytes() ) );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    BOOST_REQUIRE( fixture.rpcClient->eth_getTransactionReceipt( txHash )["status"] == "0x1" );
    BOOST_REQUIRE( BlockRewardsActivationPatch::isEnabled( fixture.client->chainId() ) );
    etherbaseBalance = fixture.client->balanceAt( etherbase_address );
    BOOST_REQUIRE_EQUAL( etherbaseBalance, 0 );
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( stakingContractAddress ), 0 );

    etherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );

    auto authorInitialBalance = fixture.client->balanceAt( jsToAddress( "0x0E7d7F1D34a502bD609542576941C3FCc087c588" ) );
    auto stakingContractInitialBalance = fixture.client->balanceAt( stakingContractAddress );

    auto initialBlockNumber = jsToU256( fixture.rpcClient->eth_blockNumber() );

    // push random transaction, check balance difference
    Json::Value sampleTx;
    sampleTx["value"] = 1000000;
    sampleTx["data"] = toJS( bytes() );
    sampleTx["from"] = fixture.coinbase.address().hex();
    sampleTx["to"] = fixture.account2.address().hex();
    sampleTx["gasPrice"] = 1000000000000;
    txHash = fixture.rpcClient->eth_sendTransaction( sampleTx );
    BOOST_REQUIRE( !txHash.empty() );

    dev::eth::mineTransaction( *( fixture.client ), 1 );

    fixture.client->state().getOriginalDb()->createBlockSnap( 2 );
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( fixture.account2.address() ), u256( 1000000 ) );

    auto txData = fixture.rpcClient->eth_getTransactionReceipt( txHash );

    auto blockNumAsString = fixture.rpcClient->eth_blockNumber();

    auto author = fixture.rpcClient->eth_getBlockByNumber( blockNumAsString, false )["author"];
    auto blockNumber = jsToU256( blockNumAsString );

    BOOST_REQUIRE( author == node_owner );

    auto totalReward = fixture.client->chainParams().blockReward(
        fixture.client->latestBlock().info().timestamp(), fixture.client->number() );
    auto blockAuthorReward = dev::calculateShareWithPrecision( totalReward, fixture.client->evmSchedule().shareOfBlockRewardToBlockAuthorPromille );
    auto stakingContractReward = totalReward - blockAuthorReward;

    auto feeForTx =
        jsToU256( sampleTx["gasPrice"].asString() ) * jsToU256( txData["gasUsed"].asString() );
    feeForTx = dev::calculateShareWithPrecision( feeForTx, fixture.client->evmSchedule().shareOfTransactionFeeToRewardPromille );

    auto expectedAuthorBalanceChange = ( blockNumber - initialBlockNumber ) * blockAuthorReward + feeForTx;
    auto expectedContractBalanceChange = ( blockNumber - initialBlockNumber ) * stakingContractReward;

    BOOST_REQUIRE_EQUAL(
        fixture.client->balanceAt( jsToAddress( author.asString() ) ) - authorInitialBalance,
        expectedAuthorBalanceChange );
    BOOST_REQUIRE_EQUAL(
        fixture.client->balanceAt( stakingContractAddress ) - stakingContractInitialBalance,
        expectedContractBalanceChange );
}

BOOST_AUTO_TEST_CASE( block_author_balance_reward_wallet ) {
    // when rewardWalletAddress is defined

    // checksumed address: 0xfa3fe33E351a7c60039E59D923e417A6362D1C3E
    auto node_reward_wallet = "0xfa3fe33e351a7c60039e59d923e417a6362d1c3e";
    auto node_reward_wallet_address = jsToAddress( node_reward_wallet );

    nlohmann::json configJson = nlohmann::json::parse( c_BITECommitteeRotationConfigString );
    // configJson["skaleConfig"]["sChain"]["nodeGroups"]["0"]["nodes"]["8"][3] = node_reward_wallet;
    configJson["skaleConfig"]["sChain"]["nodeGroups"]["1"]["nodes"]["8"][3] = node_reward_wallet;
    // configJson["skaleConfig"]["sChain"]["node"]["1"]["group"][0]["rewardWalletAddress"] = node_reward_wallet;

    auto noRewardWalletAddressConfig = configJson.dump();
    JsonRpcFixture fixture( noRewardWalletAddressConfig, false, false, true );

    auto authorInitialBalance = fixture.client->balanceAt( node_reward_wallet_address );

    auto initialBlockNumber = jsToU256( fixture.rpcClient->eth_blockNumber() );

    // mine block without transactions
    dev::eth::simulateMining( *( fixture.client ), 1 );
    sleep( 3 );

    // mine transaction
    Json::Value sampleTx;
    sampleTx["value"] = 1000000;
    sampleTx["data"] = toJS( bytes() );
    sampleTx["from"] = fixture.coinbase.address().hex();
    sampleTx["to"] = fixture.account2.address().hex();
    sampleTx["gasPrice"] = 1000000000000;
    std::string txHash = fixture.rpcClient->eth_sendTransaction( sampleTx );
    BOOST_REQUIRE( !txHash.empty() );

    dev::eth::mineTransaction( *( fixture.client ), 1 );

    fixture.client->state().getOriginalDb()->createBlockSnap( 2 );
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( fixture.account2.address() ), u256( 1000000 ) );

    auto txData = fixture.rpcClient->eth_getTransactionReceipt( txHash );

    auto blockNumAsString = fixture.rpcClient->eth_blockNumber();

    auto author = fixture.rpcClient->eth_getBlockByNumber( blockNumAsString, false )["author"];
    auto blockNumber = jsToU256( blockNumAsString );
    BOOST_REQUIRE( author == node_reward_wallet );

    auto totalReward = fixture.client->chainParams().blockReward(
        fixture.client->latestBlock().info().timestamp(), fixture.client->number() );
    auto blockAuthorReward =
            dev::calculateShareWithPrecision( totalReward,
                fixture.client->evmSchedule().shareOfBlockRewardToBlockAuthorPromille );

    auto feeForTx = jsToU256( sampleTx["gasPrice"].asString() ) * jsToU256( txData["gasUsed"].asString() );
    feeForTx = dev::calculateShareWithPrecision( feeForTx, fixture.client->evmSchedule().shareOfTransactionFeeToRewardPromille );
    auto expectedBalanceChange = ( blockNumber - initialBlockNumber ) * blockAuthorReward + feeForTx;

    BOOST_REQUIRE_EQUAL(
        fixture.client->balanceAt( jsToAddress( author.asString() ) ) - authorInitialBalance,
        expectedBalanceChange );
}
#endif // FAIR

#endif // #ifdef BITE

#ifndef FAIR
BOOST_AUTO_TEST_CASE( etherbase_generation2 ) {
    JsonRpcFixture fixture( c_genesisGeneration2ConfigString, false, false, true );
    string etherbase = fixture.rpcClient->eth_coinbase();

    u256 etherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );

    // mine block without transactions
    dev::eth::simulateMining( *( fixture.client ), 1 );
    sleep( 3 );
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
    dev::eth::mineTransaction( *( fixture.client ), 2 );
    fixture.client->state().getOriginalDb()->createBlockSnap( 2 );
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( fixture.account2.address() ), u256( 1000000 ) );

    // partially retrieve 1000000
    etherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );
    u256 balance =
        fixture.client->balanceAt( jsToAddress( "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F" ) );

    Json::Value partiallyRetrieveTx;
    partiallyRetrieveTx["data"] =
        "0xc6427474000000000000000000000000d2c0deface0000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000006000000000000000000000000000000000000000000000000000000000000000e4b61d"
        "27f6000000000000000000000000d2ba3e00000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000600000000000000000000000000000000000000000000000000000000000000044204a3e9300"
        "00000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000"
        "0000000000000000000000000000000f4240000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000";
    partiallyRetrieveTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    partiallyRetrieveTx["to"] = "0xD244519000000000000000000000000000000000";
    partiallyRetrieveTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    partiallyRetrieveTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( partiallyRetrieveTx );
    BOOST_REQUIRE( !txHash.empty() );
    dev::eth::mineTransaction( *( fixture.client ), 2 );

    fixture.client->state().getOriginalDb()->createBlockSnap( 3 );
    auto t = fixture.rpcClient->eth_getTransactionReceipt( txHash );
#ifdef FAIR
    // reward goes to the node owner, not etherbase
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( jsToAddress( etherbase ) ), etherbaseBalance - u256( 1000000 ) );
#else
    BOOST_REQUIRE_EQUAL( fixture.client->balanceAt( jsToAddress( etherbase ) ),
        etherbaseBalance +
            jsToU256( t["gasUsed"].asString() ) *
                jsToU256( partiallyRetrieveTx["gasPrice"].asString() ) -
            u256( 1000000 ) );
#endif
    BOOST_REQUIRE_EQUAL(
        fixture.client->balanceAt( jsToAddress( "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F" ) ),
        balance + u256( 1000000 ) );

    // retrieve all
    u256 oldEtherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );
    balance =
        fixture.client->balanceAt( jsToAddress( "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F" ) );

    Json::Value retrieveTx;
    retrieveTx["data"] =
        "0xc6427474000000000000000000000000d2c0deface0000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000006000000000000000000000000000000000000000000000000000000000000000c4b61d"
        "27f6000000000000000000000000d2ba3e00000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000006000000000000000000000000000000000000000000000000000000000000000240a79309b00"
        "00000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
    retrieveTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    retrieveTx["to"] = "0xD244519000000000000000000000000000000000";
    retrieveTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    retrieveTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( retrieveTx );
    BOOST_REQUIRE( !txHash.empty() );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    fixture.client->state().getOriginalDb()->createBlockSnap( 4 );
    t = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    etherbaseBalance = fixture.client->balanceAt( jsToAddress( etherbase ) );
#ifdef FAIR
// reward goes to the node owner, not etherbase
    BOOST_REQUIRE_EQUAL(  etherbaseBalance, 0 );
#else
    BOOST_REQUIRE_EQUAL(  etherbaseBalance,
                          jsToU256( t["gasUsed"].asString() ) *
                              jsToU256( partiallyRetrieveTx["gasPrice"].asString()  ));
#endif
    BOOST_REQUIRE_EQUAL(
        fixture.client->balanceAt( jsToAddress( "0x7aa5E36AA15E93D10F4F26357C30F052DacDde5F" ) ),
        balance + oldEtherbaseBalance );
}

BOOST_AUTO_TEST_CASE( deploy_controller_generation2 ) {
    JsonRpcFixture fixture( c_genesisGeneration2ConfigString, false, false, true );

    Json::Value hasRoleBeforeGrantingCall;
    hasRoleBeforeGrantingCall["data"] =
        "0x91d14854fc425f2263d0df187444b70e47283d622c70181c5baebb1306a01edba1ce184c0000000000000000"
        "000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    hasRoleBeforeGrantingCall["to"] = "0xD2002000000000000000000000000000000000d2";
    BOOST_REQUIRE(
        jsToInt( fixture.rpcClient->eth_call( hasRoleBeforeGrantingCall, "latest" ) ) == 0 );

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
    grantDeployerRoleTx["data"] =
        "0xc6427474000000000000000000000000d2c0deface0000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000006000000000000000000000000000000000000000000000000000000000000000c4b61d"
        "27f6000000000000000000000000d2002000000000000000000000000000000000d20000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000600000000000000000000000000000000000000000000000000000000000000024e43252d700"
        "00000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
    grantDeployerRoleTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    grantDeployerRoleTx["to"] = "0xD244519000000000000000000000000000000000";
    grantDeployerRoleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    grantDeployerRoleTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( grantDeployerRoleTx );
    BOOST_REQUIRE( !txHash.empty() );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value hasRoleCall;
    hasRoleCall["data"] =
        "0x91d14854fc425f2263d0df187444b70e47283d622c70181c5baebb1306a01edba1ce184c0000000000000000"
        "000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
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

    JsonRpcFixture fixture( config, false, false, true );
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
    JsonRpcFixture fixture( c_genesisGeneration2ConfigString, false, false, true );

    Json::Value hasRoleBeforeGrantingCall;
    hasRoleBeforeGrantingCall["data"] =
        "0x91d1485468bf109b95a5c15fb2bb99041323c27d15f8675e11bf7420a1cd6ad64c394f460000000000000000"
        "000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    hasRoleBeforeGrantingCall["to"] = "0xD3002000000000000000000000000000000000d3";
    BOOST_REQUIRE(
        jsToInt( fixture.rpcClient->eth_call( hasRoleBeforeGrantingCall, "latest" ) ) == 0 );

    Json::Value reserveSpaceBeforeGrantRoleTx;
    reserveSpaceBeforeGrantRoleTx["data"] =
        "0x1cfe4e3b0000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000"
        "000000000000000000000000000000000000000000000064";
    reserveSpaceBeforeGrantRoleTx["from"] = "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    reserveSpaceBeforeGrantRoleTx["to"] = "0xD3002000000000000000000000000000000000d3";
    reserveSpaceBeforeGrantRoleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    reserveSpaceBeforeGrantRoleTx["gas"] = toJS( "1000000" );
    std::string txHash = fixture.rpcClient->eth_sendTransaction( reserveSpaceBeforeGrantRoleTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    Json::Value receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x0" ) );

    Json::Value grantRoleTx;
    grantRoleTx["data"] =
        "0xc6427474000000000000000000000000d2c0deface0000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000006000000000000000000000000000000000000000000000000000000000000000e4b61d"
        "27f6000000000000000000000000d3002000000000000000000000000000000000d30000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000006000000000000000000000000000000000000000000000000000000000000000442f2ff15d68"
        "bf109b95a5c15fb2bb99041323c27d15f8675e11bf7420a1cd6ad64c394f460000000000000000000000007aa5"
        "e36aa15e93d10f4f26357c30f052dacdde5f000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000";
    grantRoleTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    grantRoleTx["to"] = "0xD244519000000000000000000000000000000000";
    grantRoleTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    grantRoleTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( grantRoleTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );

    Json::Value hasRoleCall;
    hasRoleCall["data"] =
        "0x91d1485468bf109b95a5c15fb2bb99041323c27d15f8675e11bf7420a1cd6ad64c394f460000000000000000"
        "000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    hasRoleCall["to"] = "0xD3002000000000000000000000000000000000d3";
    BOOST_REQUIRE( jsToInt( fixture.rpcClient->eth_call( hasRoleCall, "latest" ) ) == 1 );

    Json::Value reserveSpaceTx;
    reserveSpaceTx["data"] =
        "0x1cfe4e3b0000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000"
        "000000000000000000000000000000000000000000000064";
    reserveSpaceTx["from"] = "0x7aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    reserveSpaceTx["to"] = "0xD3002000000000000000000000000000000000d3";
    reserveSpaceTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    reserveSpaceTx["gas"] = toJS( "1000000" );
    txHash = fixture.rpcClient->eth_sendTransaction( reserveSpaceTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    receipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE_EQUAL( receipt["status"], string( "0x1" ) );

    Json::Value getReservedSpaceCall;
    hasRoleCall["data"] =
        "0xbb559d160000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f";
    hasRoleCall["to"] = "0xD3002000000000000000000000000000000000d3";
    BOOST_REQUIRE( jsToInt( fixture.rpcClient->eth_call( hasRoleCall, "latest" ) ) == 100 );
}
#endif

BOOST_AUTO_TEST_CASE( PrecompiledPrintFakeEth,
    *boost::unit_test::precondition( []( unsigned long ) -> bool { return false; } ) ) {
    JsonRpcFixture fixture( c_genesisConfigString, false, false );
    dev::eth::simulateMining( *( fixture.client ), 20 );

    fixture.accountHolder->setAccounts( { fixture.coinbase, fixture.account2,
        dev::KeyPair( dev::Secret(
            "0x1c2cd4b70c2b8c6cd7144bbbfbd1e5c6eacb4a5efd9c86d0e29cbbec4e8483b9" ) ) } );

    u256 balance =
        fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
    BOOST_REQUIRE_EQUAL( balance, 0 );

    Json::Value printFakeEthFromDisallowedAddressTx;
    printFakeEthFromDisallowedAddressTx["data"] =
        "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8000000000000000000000000000000000000000000000000"
        "0000000000000010";
    printFakeEthFromDisallowedAddressTx["from"] = fixture.coinbase.address().hex();
    printFakeEthFromDisallowedAddressTx["to"] = "0000000000000000000000000000000000000006";
    printFakeEthFromDisallowedAddressTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    fixture.rpcClient->eth_sendTransaction( printFakeEthFromDisallowedAddressTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    balance =
        fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
    BOOST_REQUIRE_EQUAL( balance, 0 );

    Json::Value printFakeEthTx;
    printFakeEthTx["data"] =
        "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8000000000000000000000000000000000000000000000000"
        "0000000000000010";
    printFakeEthTx["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    printFakeEthTx["to"] = "0000000000000000000000000000000000000006";
    printFakeEthTx["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    fixture.rpcClient->eth_sendTransaction( printFakeEthTx );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    balance =
        fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
    BOOST_REQUIRE_EQUAL( balance, 16 );

    Json::Value printFakeEthCall;

    printFakeEthCall["data"] =
        "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8000000000000000000000000000000000000000000000000"
        "0000000000000010";

    printFakeEthCall["from"] = "0x5C4e11842E8be09264dc1976943571d7Af6d00F9";
    printFakeEthCall["to"] = "0000000000000000000000000000000000000006";
    printFakeEthCall["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    fixture.rpcClient->eth_call( printFakeEthCall, "latest" );

    balance =
        fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
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

    string compiled =
        "0x6080604052348015600f57600080fd5b5060a78061001e6000396000f3006080604052600436106022576000"
        "3560e01c63ffffffff16806328b5e32b146027575b600080fd5b348015603257600080fd5b506039603b565b00"
        "5b600080600060109150735c4e11842e8be09264dc1976943571d7af6d00f89050604051818152826020820152"
        "6020816040836006600019f49350505050505600a165627a7a72305820c99b5f7e9e41fb0fee1724d382ca0f2c"
        "003087f66b3b46037ca6c7d452b076f20029";

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
    balance =
        fixture.client->balanceAt( jsToAddress( "0x5C4e11842E8Be09264DC1976943571D7AF6d00f8" ) );
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

    h256 h1 = fixture.client->importTransaction( tx1, TransactionBroadcast::DontBroadcast );
    h256 h2 = fixture.client->importTransaction( tx2, TransactionBroadcast::DontBroadcast );
    h256 h3 = fixture.client->importTransaction( tx3, TransactionBroadcast::DontBroadcast );
    BOOST_REQUIRE( h1 );
    BOOST_REQUIRE( h2 );
    BOOST_REQUIRE( h3 );
    BOOST_REQUIRE( fixture.client->transactionQueueStatus().current == 3 );
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
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 1 );

    Json::Value call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 1 );

    h256 h2 = fixture.client->importTransaction( tx3 );
    BOOST_REQUIRE( h2 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 2 );

    call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 2 );
    BOOST_REQUIRE_EQUAL( call[0]["from"], string( "0x" ) + txJson["from"].asString() );

    h256 h3 = fixture.client->importTransaction( tx2 );
    BOOST_REQUIRE( h3 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 3 );

    call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 3 );

    h256 h4 = fixture.client->importTransaction( tx1 );
    BOOST_REQUIRE( h4 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 1 );
    BOOST_REQUIRE_EQUAL( tq->status().current, 3 );

    call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 1 );

    h256 h5 = fixture.client->importTransaction( tx4 );
    BOOST_REQUIRE( h5 );
    BOOST_REQUIRE_EQUAL( tq->futureSize(), 0 );
    BOOST_REQUIRE_EQUAL( tq->status().current, 5 );

    call = fixture.rpcClient->debug_getFutureTransactions();
    BOOST_REQUIRE_EQUAL( call.size(), 0 );

    fixture.client->skaleHost()->pauseConsensus( false );
}

// TODO: Enable for multitransaction mode checking


// historic node shall ignore invalid transactions in block
BOOST_AUTO_TEST_CASE( skip_invalid_transactions ) {
    sleep( 1 );
    JsonRpcFixture fixture( c_genesisConfigString, true, true, false, true );
    dev::eth::simulateMining( *( fixture.client ), 1 );  // 2 Ether

    cout << " Balance: "
         << fixture.rpcClient->eth_getBalance(
                fixture.accountHolder->allAccounts()[0].hex(), "latest" )
         << endl;

    // 1 import 1 transaction to increase block number
    // also send some eth to account2
    // TODO repair mineMoney function! (it asserts)
    Json::Value txJson;
    txJson["from"] = fixture.coinbase.address().hex();
    txJson["gas"] = "200000";
    txJson["gasPrice"] = "5000000000000";
    txJson["to"] = fixture.account2.address().hex();
#ifdef FAIR
    txJson["value"] = "3500000000000000000";
#else
    txJson["value"] = "1000000000000000000";
#endif

    txJson["nonce"] = "0";
    TransactionSkeleton ts1 = toTransactionSkeleton( txJson );
    ts1 = fixture.client->populateTransactionWithDefaults( ts1 );
    pair< bool, Secret > ar1 = fixture.accountHolder->authenticate( ts1 );
    Transaction tx1( ts1, ar1.second );
    fixture.client->importTransaction( tx1 );

    // 1 eth left (returned to author)
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    cout << fixture.accountHolder->allAccounts()[0].hex() << " Balance2: "
         << fixture.rpcClient->eth_getBalance(
                fixture.accountHolder->allAccounts()[0].hex(), "latest" )
         << endl;

    // 2 import 4 transactions with money for 1st, 2nd, and 3rd
    // require full 1 Ether for gas+value
    txJson["gas"] = "100000";
    txJson["nonce"] = "1";
    txJson["value"] = "500000000000000000";  // take 0.5 eth out
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

    h256 h4 = fixture.client->importTransaction( tx4 );  // ok
    h256 h2 = fixture.client->importTransaction( tx2 );  // invalid
    h256 h3 = fixture.client->importTransaction( tx3 );  // ok
    h256 h1 = fixture.client->importTransaction( tx1 );  // ok

    dev::eth::mineTransaction( *( fixture.client ), 1 );
    cout << "Balance3: "
         << fixture.rpcClient->eth_getBalance(
                fixture.accountHolder->allAccounts()[0].hex(), "latest" )
         << endl;

    ( void ) h1;
    ( void ) h2;
    ( void ) h3;
    ( void ) h4;

#ifdef HISTORIC_STATE
    // 3 check that historic node sees only 3 txns

    string explicitNumberStr = to_string( fixture.client->number() );

    // 1 Block
    Json::Value block = fixture.rpcClient->eth_getBlockByNumber( "latest", "false" );

    string bh = block["hash"].asString();

    // 2 transaction count
    Json::Value cnt = fixture.rpcClient->eth_getBlockTransactionCountByNumber( "latest" );
    BOOST_REQUIRE_EQUAL( cnt.asString(), "0x3" );
    cnt = fixture.rpcClient->eth_getBlockTransactionCountByNumber( explicitNumberStr );
    BOOST_REQUIRE_EQUAL( cnt.asString(), "0x3" );
    cnt = fixture.rpcClient->eth_getBlockTransactionCountByHash( bh );
    BOOST_REQUIRE_EQUAL( cnt.asString(), "0x3" );


    BOOST_REQUIRE_EQUAL( block["transactions"].size(), 3 );
    BOOST_REQUIRE_EQUAL( block["transactions"][0]["transactionIndex"], "0x0" );
    BOOST_REQUIRE_EQUAL( block["transactions"][1]["transactionIndex"], "0x1" );
    BOOST_REQUIRE_EQUAL( block["transactions"][2]["transactionIndex"], "0x2" );

    // same with explicit number
    block = fixture.rpcClient->eth_getBlockByNumber( explicitNumberStr, "false" );

    BOOST_REQUIRE_EQUAL( block["transactions"].size(), 3 );
    BOOST_REQUIRE_EQUAL( block["transactions"][0]["transactionIndex"], "0x0" );
    BOOST_REQUIRE_EQUAL( block["transactions"][1]["transactionIndex"], "0x1" );
    BOOST_REQUIRE_EQUAL( block["transactions"][2]["transactionIndex"], "0x2" );

    // 3 receipts
    Json::Value r1, r3, r4;
    BOOST_REQUIRE_NO_THROW( r1 = fixture.rpcClient->eth_getTransactionReceipt( toJS( h1 ) ) );
    BOOST_REQUIRE_THROW(
        fixture.rpcClient->eth_getTransactionReceipt( toJS( h2 ) ), jsonrpc::JsonRpcException );
    BOOST_REQUIRE_NO_THROW( r3 = fixture.rpcClient->eth_getTransactionReceipt( toJS( h3 ) ) );
    BOOST_REQUIRE_NO_THROW( r4 = fixture.rpcClient->eth_getTransactionReceipt( toJS( h4 ) ) );

    BOOST_REQUIRE_EQUAL( r1["transactionIndex"], "0x0" );
    BOOST_REQUIRE_EQUAL( r3["transactionIndex"], "0x1" );
    BOOST_REQUIRE_EQUAL( r4["transactionIndex"], "0x2" );

    // 4 transaction by index
    Json::Value t0 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex( "latest", "0" );
    Json::Value t1 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex( "latest", "1" );
    Json::Value t2 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex( "latest", "2" );
    BOOST_REQUIRE_EQUAL( jsToFixed< 32 >( t0["hash"].asString() ), h1 );
    BOOST_REQUIRE_EQUAL( jsToFixed< 32 >( t1["hash"].asString() ), h3 );
    BOOST_REQUIRE_EQUAL( jsToFixed< 32 >( t2["hash"].asString() ), h4 );

    // same with explicit block number

    t0 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex( explicitNumberStr, "0" );
    t1 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex( explicitNumberStr, "1" );
    t2 = fixture.rpcClient->eth_getTransactionByBlockNumberAndIndex( explicitNumberStr, "2" );
    BOOST_REQUIRE_EQUAL( jsToFixed< 32 >( t0["hash"].asString() ), h1 );
    BOOST_REQUIRE_EQUAL( jsToFixed< 32 >( t1["hash"].asString() ), h3 );
    BOOST_REQUIRE_EQUAL( jsToFixed< 32 >( t2["hash"].asString() ), h4 );

    BOOST_REQUIRE_EQUAL( bh, r1["blockHash"].asString() );

    t0 = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex( bh, "0" );
    t1 = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex( bh, "1" );
    t2 = fixture.rpcClient->eth_getTransactionByBlockHashAndIndex( bh, "2" );

    BOOST_REQUIRE_EQUAL( jsToFixed< 32 >( t0["hash"].asString() ), h1 );
    BOOST_REQUIRE_EQUAL( jsToFixed< 32 >( t1["hash"].asString() ), h3 );
    BOOST_REQUIRE_EQUAL( jsToFixed< 32 >( t2["hash"].asString() ), h4 );

    // 5 transaction by hash
    BOOST_REQUIRE_THROW(
        fixture.rpcClient->eth_getTransactionByHash( toJS( h2 ) ), jsonrpc::JsonRpcException );

    // send it successfully

    // make money
    dev::eth::simulateMining( *fixture.client, 1 );

    h2 = fixture.client->importTransaction( tx2 );  // invalid

    dev::eth::mineTransaction( *( fixture.client ), 1 );

    // checks:
    Json::Value r2;
    BOOST_REQUIRE_NO_THROW( r2 = fixture.rpcClient->eth_getTransactionReceipt( toJS( h2 ) ) );
    BOOST_REQUIRE_EQUAL( r2["blockNumber"], toJS( fixture.client->number() ) );
#endif
}

BOOST_AUTO_TEST_CASE( eth_signAndSendRawTransaction,
    *boost::unit_test::precondition( dev::test::manuallyRunningTest ) ) {
    SkaledFixture fixture( skaledConfigFileName );
    fixture.setupFirstKey();
    auto firstAccount = fixture.testAccounts.begin()->second;
    auto gasPrice = fixture.getCurrentGasPrice();
    for ( uint64_t i = 0; i < 3; i++ ) {
        auto dst = SkaledAccount::generate();
        fixture.splitAccountInHalves(
            firstAccount, dst, gasPrice, TransactionWait::WAIT_FOR_COMPLETION );
    }
}

BOOST_AUTO_TEST_CASE( perf_sendManyParalelEthTransfers,
    *boost::unit_test::precondition( dev::test::manuallyRunningTest ) ) {
    SkaledFixture fixture( skaledConfigFileName );
    vector< Secret > accountPieces;

    fixture.verifyTransactions = false;
    fixture.threadsCountForTestTransactions = 8;
    fixture.mtmBatchSize = 1;

    fixture.setupFirstKey();
    fixture.deployERC20();

    fixture.setupTwoToTheNKeys( 12 );

    fixture.sendTinyTransfersForAllAccounts( 10, TransferType::NATIVE );
}


BOOST_AUTO_TEST_CASE(
    perf_calls, *boost::unit_test::precondition( dev::test::manuallyRunningTest ) ) {
    SkaledFixture fixture( skaledConfigFileName );
    vector< Secret > accountPieces;

    fixture.threadsCountForTestTransactions = 8;


    fixture.setupFirstKey();
    fixture.deployERC20();

    fixture.setupTwoToTheNKeys( 12 );

    fixture.sendCallsForAllAccounts( 1, CallType::BLOCK_BY_NUMBER, "eth_getBlockByNumber" );
    fixture.sendCallsForAllAccounts( 1, CallType::TRANSACTION_COUNT, "eth_transactionCount" );
    fixture.sendCallsForAllAccounts( 1, CallType::BALANCE, "eth_getBalance" );
    fixture.sendCallsForAllAccounts( 1, CallType::BLOCK_NUMBER, "eth_blockNumber" );
    fixture.sendCallsForAllAccounts( 1, CallType::CHAIN_ID, "eth_chainId" );
    fixture.sendCallsForAllAccounts( 1, CallType::NET_VERSION, "net_version" );
    fixture.sendCallsForAllAccounts( 1, CallType::GAS_PRICE, "eth_gasPrice" );
    fixture.sendCallsForAllAccounts( 1, CallType::HASH_RATE, "eth_hashrate" );
    fixture.sendCallsForAllAccounts( 1, CallType::MINING, "eth_mining" );
    fixture.sendCallsForAllAccounts( 1, CallType::SYNCING, "eth_syncing" );
    fixture.sendCallsForAllAccounts( 1, CallType::WEB3_CLIENT_VERSION, "web3_clientVersion" );
}


BOOST_AUTO_TEST_CASE( perf_sendManyParalelEthMTMTransfers,
    *boost::unit_test::precondition( dev::test::manuallyRunningTest ) ) {
    SkaledFixture fixture( skaledConfigFileName );
    vector< Secret > accountPieces;

    fixture.verifyTransactions = false;
    fixture.threadsCountForTestTransactions = 8;
    fixture.mtmBatchSize = 5;

    fixture.setupFirstKey();
    fixture.setupTwoToTheNKeys( 8 );

    fixture.sendTinyTransfersForAllAccounts( 10, TransferType::NATIVE );
}

BOOST_AUTO_TEST_CASE( perf_sendManyParalelEthType1Transfers,
    *boost::unit_test::precondition( dev::test::manuallyRunningTest ) ) {
    SkaledFixture fixture( skaledConfigFileName );
    vector< Secret > accountPieces;

    fixture.verifyTransactions = false;
    fixture.threadsCountForTestTransactions = 8;
    fixture.transactionType = TransactionType::Type1;

    fixture.setupFirstKey();
    fixture.deployERC20();

    fixture.setupTwoToTheNKeys( 12 );

    fixture.sendTinyTransfersForAllAccounts( 1000, TransferType::NATIVE );
}

BOOST_AUTO_TEST_CASE( perf_sendManyParalelEthType2Transfers,
    *boost::unit_test::precondition( dev::test::manuallyRunningTest ) ) {
    SkaledFixture fixture( skaledConfigFileName );
    vector< Secret > accountPieces;

    fixture.verifyTransactions = false;
    fixture.threadsCountForTestTransactions = 8;
    fixture.transactionType = TransactionType::Type2;

    fixture.setupFirstKey();
    fixture.deployERC20();

    fixture.setupTwoToTheNKeys( 12 );

    fixture.sendTinyTransfersForAllAccounts( 1000, TransferType::NATIVE );
}

BOOST_AUTO_TEST_CASE( perf_sendManyParalelEthPowTransfers,
    *boost::unit_test::precondition( dev::test::manuallyRunningTest ) ) {
    SkaledFixture fixture( skaledConfigFileName );
    vector< Secret > accountPieces;

    fixture.verifyTransactions = false;
    fixture.threadsCountForTestTransactions = 8;
#ifndef FAIR
    fixture.usePow = true;
#endif

    fixture.setupFirstKey();
    fixture.deployERC20();

    fixture.setupTwoToTheNKeys( 4 );

    fixture.sendTinyTransfersForAllAccounts( 1000, TransferType::NATIVE );
}

BOOST_AUTO_TEST_CASE( perf_sendManyParalelERC20Transfers,
    *boost::unit_test::precondition( dev::test::manuallyRunningTest ) ) {
    SkaledFixture fixture( skaledConfigFileName );
    vector< Secret > accountPieces;

    fixture.verifyTransactions = false;
    fixture.threadsCountForTestTransactions = 8;

    fixture.setupFirstKey();

    fixture.deployERC20();

    fixture.setupTwoToTheNKeys( 12 );
    fixture.mintAllKeysWithERC20();

    fixture.sendTinyTransfersForAllAccounts( 10, TransferType::ERC20 );
}


#ifndef FAIR
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
#endif

BOOST_FIXTURE_TEST_SUITE( GappedCacheSuite, JsonRpcFixture )

#ifdef HISTORIC_STATE

BOOST_AUTO_TEST_CASE( test_blocks ) {
    dev::rpc::_detail::GappedTransactionIndexCache cache( 10, *client );
    BOOST_REQUIRE_EQUAL( cache.realBlockTransactionCount( LatestBlock ), 0 );
    BOOST_REQUIRE_EQUAL( cache.realBlockTransactionCount( PendingBlock ), 0 );
    BOOST_REQUIRE_EQUAL( cache.realBlockTransactionCount( 999999999 ), 0 );
}

BOOST_AUTO_TEST_CASE( test_transactions ) {
    simulateMining( *client, 1, Address( "0xf6c2a4ba2350e58a45916a03d0faa70dcc5dcfbf" ) );

    // give it some time since testing fixture is not reliable
    // to do - move to real skaled testing
    sleep( 3 );

    dev::rpc::_detail::GappedTransactionIndexCache cache( 10, *client );

    Transaction invalid( fromHex( "0x00112233445566778899001122334455667788990011223344556677889900"
                                  "11223344556677889900112233"
                                  "4455667788990011223344556677889900112233445566778899001122334455"
                                  "66778899001122334455667788"
                                  "990011223344556677889900112233445566778899" ),
        CheckTransaction::None, true );

    Transaction valid(
        fromHex( "0xf86c808504a817c80083015f90943d7112ee86223baf0a506b9d2a77595cbbba51d1872386f26fc"
                 "10000801ca0655757fd0650a65a373c48a4dc0f3d6ac5c3831aa0cc2cb863a5909dc6c25f72a07188"
                 "2ee8633466a243c0ea64dadb3120c1ca7a5cc7433c6c0b1c861a85322265" ),
        CheckTransaction::None );
#ifndef FAIR
    valid.ignoreExternalGas();
#endif

    // give it some time since testing fixture is not reliable
    // to do - move to real skaled testing
    sleep( 3 );

    client->importTransactionsAsBlock( Transactions{ invalid, valid },
#ifdef BITE
        std::make_shared< DecryptedTransactionFieldsMap >(),
#endif

#ifdef FAIR
                                       1,
#endif
                                       1 );

#ifndef FAIR
    BOOST_REQUIRE_EQUAL( cache.realBlockTransactionCount( LatestBlock ), 2 );
    BOOST_REQUIRE_EQUAL( cache.realIndexFromGapped( LatestBlock, 0 ), 1 );
    BOOST_REQUIRE_EQUAL( cache.gappedIndexFromReal( LatestBlock, 1 ), 0 );
    BOOST_REQUIRE_THROW( cache.gappedIndexFromReal( LatestBlock, 0 ), std::out_of_range );
    BOOST_REQUIRE_EQUAL( cache.transactionPresent( LatestBlock, 0 ), false );
    BOOST_REQUIRE_EQUAL( cache.transactionPresent( LatestBlock, 1 ), true );
#else
    BOOST_REQUIRE_EQUAL( cache.realBlockTransactionCount( LatestBlock ), 1 );
    BOOST_REQUIRE_EQUAL( cache.gappedBlockTransactionCount( LatestBlock ), 1 );
    BOOST_REQUIRE_EQUAL( cache.realIndexFromGapped( LatestBlock, 0 ), 0 );
    BOOST_REQUIRE_NO_THROW( cache.gappedIndexFromReal( LatestBlock, 0 ) );
    BOOST_REQUIRE_EQUAL( cache.transactionPresent( LatestBlock, 0 ), true );
    BOOST_REQUIRE_THROW( cache.transactionPresent( LatestBlock, 1 ), std::out_of_range );
#endif
}

BOOST_AUTO_TEST_CASE( test_exceptions ) {
    simulateMining( *client, 1, Address( "0xf6c2a4ba2350e58a45916a03d0faa70dcc5dcfbf" ) );

    dev::rpc::_detail::GappedTransactionIndexCache cache( 10, *client );

    Transaction invalid( fromHex( "0x00112233445566778899001122334455667788990011223344556677889900"
                                  "11223344556677889900112233"
                                  "4455667788990011223344556677889900112233445566778899001122334455"
                                  "66778899001122334455667788"
                                  "990011223344556677889900112233445566778899" ),
        CheckTransaction::None, true );

    Transaction valid(
        fromHex( "0xf86c808504a817c80083015f90943d7112ee86223baf0a506b9d2a77595cbbba51d1872386f26fc"
                 "10000801ca0655757fd0650a65a373c48a4dc0f3d6ac5c3831aa0cc2cb863a5909dc6c25f72a07188"
                 "2ee8633466a243c0ea64dadb3120c1ca7a5cc7433c6c0b1c861a85322265" ),
        CheckTransaction::None );

#ifndef FAIR
    valid.ignoreExternalGas();
#endif

    client->importTransactionsAsBlock( Transactions{ invalid, valid },
#ifdef BITE
        std::make_shared< DecryptedTransactionFieldsMap >(),
#endif

#ifdef FAIR
                                      1,
#endif
                                       1 );
    BOOST_REQUIRE_THROW( cache.realIndexFromGapped( LatestBlock, 1 ), std::out_of_range );
    BOOST_REQUIRE_THROW( cache.realIndexFromGapped( LatestBlock, 2 ), std::out_of_range );
    BOOST_REQUIRE_THROW( cache.gappedIndexFromReal( LatestBlock, 2 ), std::out_of_range );
#ifndef FAIR
    BOOST_REQUIRE_THROW( cache.gappedIndexFromReal( LatestBlock, 0 ), std::out_of_range );
#endif
    BOOST_REQUIRE_THROW( cache.transactionPresent( LatestBlock, 2 ), std::out_of_range );
}

#endif

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

