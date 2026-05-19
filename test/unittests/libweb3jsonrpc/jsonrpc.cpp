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
#include <libethcore/BITECommon.h>
#include <libconsensus/libBLS/threshold_encryption/ThresholdEncryption.h>
#endif

#ifdef BITE
#include <libethcore/BITECommon.h>
#include <libethereum/PrecompiledHelpers.h>
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

        // this fixture is used in all tests to load config. So also init bls library as well
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
#ifdef BITE
                if ( params.count( "BITE2PatchTimestamp" ) )
                    ret["skaleConfig"]["sChain"]["Bite2PatchTimestamp"] = std::stoi( params.at( "BITE2PatchTimestamp" ) );
#endif
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
        dev::eth::g_skaleHost = client->skaleHost();
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

// P1#8: eth_maxPriorityFeePerGas is a SKALE-specific wallet-compat stub and always returns 0x0,
// independent of London activation and of receipt-level effectiveGasPrice.
BOOST_AUTO_TEST_CASE( jsonrpc_eth_maxPriorityFeePerGas ) {
    JsonRpcFixture fixture;
    BOOST_REQUIRE_EQUAL( fixture.rpcClient->eth_maxPriorityFeePerGas(), "0x0" );
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

// The raw RLP below is a pre-London-shaped header (no trailing baseFeePerGas field). Under FAIR
// LondonForkPatch is unconditionally pre-enabled, so BlockHeader::populate's strict London field
// count check (libethcore/BlockHeader.cpp) now rejects this layout. The pre-strict parser was
// silently accepting the nonce field as baseFeePerGas; the recorded blockHash assertion below
// was computed under that buggy parse. Skip in FAIR builds — the rawRLP would need to be
// regenerated as a proper Ethash-London header to be importable here.
#ifndef FAIR
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
#endif  // !FAIR

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
#ifdef FAIR
    // London (EIP-3529) reduces SSTORE refunds, so gasUsed may exceed 21000
    BOOST_REQUIRE_LT( jsToInt( clearReceipt["gasUsed"].asString() ), 25000 );
#else
    BOOST_REQUIRE_LT( jsToInt( clearReceipt["gasUsed"].asString() ), 21000 );
#endif

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
    txPushValueAndCall["gas"] = "200000";
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
    txPushValue["gas"] = "200000";
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
    txThrow["gas"] = "200000";
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
    txEraseValue["gas"] = "200000";
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
    txZeroValue["gas"] = "200000";
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
    txZeroValue1["gas"] = "200000";
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
    txValueChanged["gas"] = "200000";
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
    txValueChanged1["gas"] = "200000";
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
    txValueChanged2["gas"] = "200000";
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
    txValueChanged3["gas"] = "200000";
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
    ret["skaleConfig"]["sChain"]["LondonForkPatchTimestamp"] =
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
    u256 expectedEffectiveGasPrice =
        std::min( jsToU256( block["baseFeePerGas"].asString() ) + jsToU256( "0x4a817c800" ),
            jsToU256( "0x4a817c801" ) );
    BOOST_REQUIRE( jsToU256( receipt["effectiveGasPrice"].asString() ) == expectedEffectiveGasPrice );

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
    ret["skaleConfig"]["sChain"]["LondonForkPatchTimestamp"] =
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
                toJS( fixture.client->blockInfo( bn - i - 1 ).baseFeePerGas() ) :
                toJS( 0 );
        BOOST_REQUIRE( feeHistory["baseFeePerGas"][i].asString() == estimatedBaseFeePerGas );
        BOOST_REQUIRE_GT( feeHistory["gasUsedRatio"][i].asDouble(), 0 );
        BOOST_REQUIRE_GT( 1, feeHistory["gasUsedRatio"][i].asDouble() );
        for ( Json::Value::ArrayIndex j = 0; j < percentiles.size(); ++j ) {
            BOOST_REQUIRE_EQUAL( feeHistory["reward"][i][j].asString(), toJS( 0 ) );
        }
    }

    Json::Value floatPercentiles = Json::Value( Json::arrayValue );
    floatPercentiles.resize( 2 );
    floatPercentiles[0] = 20.5;
    floatPercentiles[1] = 80.0;

    auto feeHistoryWithFloatPercentiles =
        fixture.rpcClient->eth_feeHistory( toJS( blockCnt ), "latest", floatPercentiles );
    BOOST_REQUIRE( feeHistoryWithFloatPercentiles.isMember( "reward" ) );
    BOOST_REQUIRE( feeHistoryWithFloatPercentiles["reward"].isArray() );
    for ( Json::Value::ArrayIndex i = 0; i < blockCnt; ++i ) {
        BOOST_REQUIRE( feeHistoryWithFloatPercentiles["reward"][i].isArray() );
        BOOST_REQUIRE_EQUAL(
            feeHistoryWithFloatPercentiles["reward"][i].size(), floatPercentiles.size() );
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
                                          0,
#ifdef BITE
                                          0,
                                          dev::h256::random(),
                                          dev::ZeroAddress,
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
                                0,
#ifdef BITE
                                0,
                                dev::h256::random(),
                                dev::ZeroAddress,
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
                                0,
#ifdef BITE
                                0,
                                dev::h256::random(),
                                dev::ZeroAddress,
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
            "ContractCreationReadOnlyPatchTimestamp": 1,
            "SingleStateCommitPerBlockPatchTimestamp": 1,)"
#ifdef BITE
    R"(            "Bite2PatchTimestamp": 1,
            "currentBlockRandomPatchTimestamp": 1,)"
#endif
    R"(            "nodeGroups": {
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
#ifdef BITE
    R"(
        "000000000000000000000000000000000000001A": { "precompiled": { "name": "getRandomWalletAndSignatureForCTX", "linear": { "base": 15, "word": 0 } } },
        "000000000000000000000000000000000000001B": { "precompiled": { "name": "submitCTX", "linear": { "base": 15, "word": 0 } } },)"
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
            "ContractCreationReadOnlyPatchTimestamp": 1,
            "SingleStateCommitPerBlockPatchTimestamp": 1,)"
#ifdef BITE
    R"(            "Bite2PatchTimestamp": 1,
            "currentBlockRandomPatchTimestamp": 1,)"
#endif
    R"(            "nodeGroups": {
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
#ifdef BITE
    R"(
        "0x000000000000000000000000000000000000001A": { "precompiled": { "name": "getRandomWalletAndSignatureForCTX", "linear": { "base": 15, "word": 0 } } },
        "0x000000000000000000000000000000000000001B": { "precompiled": { "name": "submitCTX", "linear": { "base": 15, "word": 0 } } },)"
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

#ifdef BITE

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

BOOST_AUTO_TEST_CASE( rejectExplicitCTXSubmission ) {
    JsonRpcFixture fixture( c_BITEConfigString, true, true, true, true, false, -1, { { "contractStorageLimit", "100000" } } );
    string senderAddress = toJS( fixture.coinbase.address() );
    size_t nonce = 0;
    std::string onDecryptSelector = dev::toHexPrefixed( dev::bite::ON_DECRYPT_FUNCTION_SELECTOR );
    Transaction t( dev::jsToBytes( formTransactionRlp( fixture, senderAddress, onDecryptSelector, nonce, dev::Address::random().hex() ) ), CheckTransaction::Everything,
        false, false, false, true
    );
    BOOST_REQUIRE_THROW( fixture.client->importTransaction( t ), IllegalCTXSubmission );
    BOOST_REQUIRE_THROW( fixture.rpcClient->eth_sendRawTransaction( dev::toHexPrefixed( t.toBytes() ) ), jsonrpc::JsonRpcException );
}

#ifndef FAIR  // ConsensusStub gasPrice(1000) < London baseFee in FAIR builds
BOOST_AUTO_TEST_CASE( submitCTX ) {
    JsonRpcFixture fixture( c_BITEConfigString, true, true, true, true, false, -1, {{ "contractStorageLimit", "100000" }} );

    string senderAddress = toJS( fixture.coinbase.address() );

    std::vector< dev::bytes > pregeneratedDecryptedValues{ dev::fromHex( "5b221ee6b5c5751ff5808beddbc0644dc4fdda6b5efb13dbb49d698cb0e3f172" ),
                                                           dev::fromHex( "006aa7d63edcfb03635a2ecf5064a9eec076c2466fb2a6c35d59b5f1039f2535" ) };
    std::vector< dev::bytes > pregeneratedPlaintextValues{ dev::asBytes( "plaintext1" ), dev::asBytes( "plaintext2" ) };
// pragma solidity ^0.8.13;

// contract Precompile0x1BCaller {
//     bytes[] decrypted = new bytes[](1);
//     bytes[] plaintext = new bytes[](1);
//     constructor() payable {}

//     function submitCTX() public {
//         uint256 randomNumber = uint256(keccak256(abi.encodePacked(block.timestamp, block.number))) % 2500000 + 1000000;
//         bytes[] memory args1 = new bytes[](2);
//         // Use pre-generated args1 values instead of generating them dynamically
//         args1[0] = hex"f9015880b9015401cc5504bac92b5ccafa0c3202372d7bb0b8cb6861795deddafae0ed7be924ff170000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006360e4a05b2e03056b2d61c7ad2deb47b0be0084ffab44bf506bfff07b951fb0bf37c171584f74d80c96306e124152458183a7a2c570a136099f7c4ffc9dde340cbed4f87133200fc4e425946925eaac958209aba78e190feeb5c9f31182ec8d458260279adb3976c158471b932bbee5bb320c";
//         args1[1] = hex"f9015880b9015401154918854780593f1c6bf620684b29ab3d4c4a4f5996dcbd1c1d0b48c06d56b40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000751884c80374b7b5d6d0ef740dacbea91b3d53ee5243eeacf94a970f131185c69dfd44e868e370602c72484bc2f34e9466255ef50ca817d34d61a46bff368318b6fff300eb566dac8d1c569a270c6d9c1f99664643582cafcb276fea83d5564cd38f4b1be0e8ee6c06e10f10f10dc39120884";

//         bytes[] memory args2 = new bytes[](2);
//         args2[0] = abi.encodePacked("plaintext1");
//         args2[1] = abi.encodePacked("plaintext2");

//         bytes memory randomBytes = abi.encode(args1, args2);
//         bytes memory input = abi.encode(randomNumber, randomBytes);

//         (bool success, bytes memory result) = address(0x1B).staticcall(input);
//         require(success, "0x1B call failed");

//         // Extract address from first 20 bytes of result and transfer
//         address walletAddress = address(bytes20(result));
//         payable(walletAddress).transfer(400000000000);
//     }

//     function submitCTXWithInput(bytes calldata input) public {
//         (bool success, bytes memory result) = address(0x1B).staticcall(input);
//         require(success, "0x1B call failed");

//         // Extract address from first 20 bytes of result and transfer
//         address walletAddress = address(bytes20(result));
//         payable(walletAddress).transfer(400000000000 );
//     }

//     function onDecrypt(bytes[] calldata decryptedArguments, bytes[] calldata plaintextArguments) public {
//         delete decrypted;
//         decrypted = new bytes[](decryptedArguments.length);
//         for (uint i = 0; i < decryptedArguments.length; ++i) {
//             decrypted[i] = decryptedArguments[i];
//         }
//         delete  plaintext;
//         plaintext = new bytes[](plaintextArguments.length);
//         for (uint i = 0; i < plaintextArguments.length; ++i) {
//             plaintext[i] = plaintextArguments[i];
//         }
//         return;
//     }

//     function getDecrypted() public view returns (bytes[] memory) {
//         return decrypted;
//     }

//     function getPlaintext() public view returns (bytes[] memory) {
//         return plaintext;
//     }
// }
    std::string bytecode = "6080604052600167ffffffffffffffff81111561001f5761001e6101ad565b5b60405190808252806020026020018201604052801561005257816020015b606081526020019060019003908161003d5790505b50600090805190602001906100689291906100d3565b50600167ffffffffffffffff811115610084576100836101ad565b5b6040519080825280602002602001820160405280156100b757816020015b60608152602001906001900390816100a25790505b50600190805190602001906100cd9291906100d3565b506104cf565b82805482825590600052602060002090810192821561011b579160200282015b8281111561011a57825182908161010a91906103fd565b50916020019190600101906100f3565b5b509050610128919061012c565b5090565b5b8082111561014c57600081816101439190610150565b5060010161012d565b5090565b50805461015c90610216565b6000825580601f1061016e575061018d565b601f01602090049060005260206000209081019061018c9190610190565b5b50565b5b808211156101a9576000816000905550600101610191565b5090565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b600081519050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052602260045260246000fd5b6000600282049050600182168061022e57607f821691505b602082108103610241576102406101e7565b5b50919050565b60008190508160005260206000209050919050565b60006020601f8301049050919050565b600082821b905092915050565b6000600883026102a97fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff8261026c565b6102b3868361026c565b95508019841693508086168417925050509392505050565b6000819050919050565b6000819050919050565b60006102fa6102f56102f0846102cb565b6102d5565b6102cb565b9050919050565b6000819050919050565b610314836102df565b61032861032082610301565b848454610279565b825550505050565b600090565b61033d610330565b61034881848461030b565b505050565b5b8181101561036c57610361600082610335565b60018101905061034e565b5050565b601f8211156103b15761038281610247565b61038b8461025c565b8101602085101561039a578190505b6103ae6103a68561025c565b83018261034d565b50505b505050565b600082821c905092915050565b60006103d4600019846008026103b6565b1980831691505092915050565b60006103ed83836103c3565b9150826002028217905092915050565b610406826101dc565b67ffffffffffffffff81111561041f5761041e6101ad565b5b6104298254610216565b610434828285610370565b600060209050601f8311600181146104675760008415610455578287015190505b61045f85826103e1565b8655506104c7565b601f19841661047586610247565b60005b8281101561049d57848901518255600182019150602085019450602081019050610478565b868310156104ba57848901516104b6601f8916826103c3565b8355505b6001600288020188555050505b505050505050565b6118b5806104de6000396000f3fe608060405234801561001057600080fd5b50600436106100575760003560e01c806338d5a3121461005c57806357983ac81461007a5780636040c1fb146100965780637372aa26146100b2578063cc159120146100bc575b600080fd5b6100646100da565b6040516100719190610af5565b60405180910390f35b610094600480360381019061008f9190610b86565b6101b3565b005b6100b060048036038101906100ab9190610c5d565b610378565b005b6100ba61048c565b005b6100c46107cf565b6040516100d19190610af5565b60405180910390f35b60606000805480602002602001604051908101604052809291908181526020016000905b828210156101aa57838290600052602060002001805461011d90610cd9565b80601f016020809104026020016040519081016040528092919081815260200182805461014990610cd9565b80156101965780601f1061016b57610100808354040283529160200191610196565b820191906000526020600020905b81548152906001019060200180831161017957829003601f168201915b5050505050815260200190600101906100fe565b50505050905090565b6000806101c091906108a8565b8383905067ffffffffffffffff8111156101dd576101dc610d0a565b5b60405190808252806020026020018201604052801561021057816020015b60608152602001906001900390816101fb5790505b50600090805190602001906102269291906108c9565b5060005b848490508110156102915784848281811061024857610247610d39565b5b905060200281019061025a9190610d77565b6000838154811061026e5761026d610d39565b5b906000526020600020019182610285929190610f9b565b5080600101905061022a565b50600160006102a091906108a8565b8181905067ffffffffffffffff8111156102bd576102bc610d0a565b5b6040519080825280602002602001820160405280156102f057816020015b60608152602001906001900390816102db5790505b50600190805190602001906103069291906108c9565b5060005b828290508110156103715782828281811061032857610327610d39565b5b905060200281019061033a9190610d77565b6001838154811061034e5761034d610d39565b5b906000526020600020019182610365929190610f9b565b5080600101905061030a565b5050505050565b600080601b73ffffffffffffffffffffffffffffffffffffffff1684846040516103a39291906110aa565b600060405180830381855afa9150503d80600081146103de576040519150601f19603f3d011682016040523d82523d6000602084013e6103e3565b606091505b509150915081610428576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161041f90611120565b60405180910390fd5b60008161043490611191565b60601c90508073ffffffffffffffffffffffffffffffffffffffff166108fc645d21dba0009081150290604051600060405180830381858888f19350505050158015610484573d6000803e3d6000fd5b505050505050565b6000620f4240622625a042436040516020016104a9929190611219565b6040516020818303038152906040528051906020012060001c6104cc9190611274565b6104d691906112d4565b90506000600267ffffffffffffffff8111156104f5576104f4610d0a565b5b60405190808252806020026020018201604052801561052857816020015b60608152602001906001900390816105135790505b50905060405180610180016040528061015b81526020016115b061015b91398160008151811061055b5761055a610d39565b5b602002602001018190525060405180610180016040528061015b815260200161170b61015b91398160018151811061059657610595610d39565b5b60200260200101819052506000600267ffffffffffffffff8111156105be576105bd610d0a565b5b6040519080825280602002602001820160405280156105f157816020015b60608152602001906001900390816105dc5790505b5090506040516020016106039061135f565b6040516020818303038152906040528160008151811061062657610625610d39565b5b6020026020010181905250604051602001610640906113c0565b6040516020818303038152906040528160018151811061066357610662610d39565b5b6020026020010181905250600082826040516020016106839291906113d5565b6040516020818303038152906040529050600084826040516020016106a9929190611465565b6040516020818303038152906040529050600080601b73ffffffffffffffffffffffffffffffffffffffff16836040516106e391906114c6565b600060405180830381855afa9150503d806000811461071e576040519150601f19603f3d011682016040523d82523d6000602084013e610723565b606091505b509150915081610768576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161075f90611120565b60405180910390fd5b60008161077490611191565b60601c90508073ffffffffffffffffffffffffffffffffffffffff166108fc645d21dba0009081150290604051600060405180830381858888f193505050501580156107c4573d6000803e3d6000fd5b505050505050505050565b60606001805480602002602001604051908101604052809291908181526020016000905b8282101561089f57838290600052602060002001805461081290610cd9565b80601f016020809104026020016040519081016040528092919081815260200182805461083e90610cd9565b801561088b5780601f106108605761010080835404028352916020019161088b565b820191906000526020600020905b81548152906001019060200180831161086e57829003601f168201915b5050505050815260200190600101906107f3565b50505050905090565b50805460008255906000526020600020908101906108c69190610922565b50565b828054828255906000526020600020908101928215610911579160200282015b8281111561091057825182908161090091906114dd565b50916020019190600101906108e9565b5b50905061091e9190610922565b5090565b5b8082111561094257600081816109399190610946565b50600101610923565b5090565b50805461095290610cd9565b6000825580601f106109645750610983565b601f0160209004906000526020600020908101906109829190610986565b5b50565b5b8082111561099f576000816000905550600101610987565b5090565b600081519050919050565b600082825260208201905092915050565b6000819050602082019050919050565b600081519050919050565b600082825260208201905092915050565b60005b83811015610a095780820151818401526020810190506109ee565b60008484015250505050565b6000601f19601f8301169050919050565b6000610a31826109cf565b610a3b81856109da565b9350610a4b8185602086016109eb565b610a5481610a15565b840191505092915050565b6000610a6b8383610a26565b905092915050565b6000602082019050919050565b6000610a8b826109a3565b610a9581856109ae565b935083602082028501610aa7856109bf565b8060005b85811015610ae35784840389528151610ac48582610a5f565b9450610acf83610a73565b925060208a01995050600181019050610aab565b50829750879550505050505092915050565b60006020820190508181036000830152610b0f8184610a80565b905092915050565b600080fd5b600080fd5b600080fd5b600080fd5b600080fd5b60008083601f840112610b4657610b45610b21565b5b8235905067ffffffffffffffff811115610b6357610b62610b26565b5b602083019150836020820283011115610b7f57610b7e610b2b565b5b9250929050565b60008060008060408587031215610ba057610b9f610b17565b5b600085013567ffffffffffffffff811115610bbe57610bbd610b1c565b5b610bca87828801610b30565b9450945050602085013567ffffffffffffffff811115610bed57610bec610b1c565b5b610bf987828801610b30565b925092505092959194509250565b60008083601f840112610c1d57610c1c610b21565b5b8235905067ffffffffffffffff811115610c3a57610c39610b26565b5b602083019150836001820283011115610c5657610c55610b2b565b5b9250929050565b60008060208385031215610c7457610c73610b17565b5b600083013567ffffffffffffffff811115610c9257610c91610b1c565b5b610c9e85828601610c07565b92509250509250929050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052602260045260246000fd5b60006002820490506001821680610cf157607f821691505b602082108103610d0457610d03610caa565b5b50919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b7f4e487b7100000000000000000000000000000000000000000000000000000000600052603260045260246000fd5b600080fd5b600080fd5b600080fd5b60008083356001602003843603038112610d9457610d93610d68565b5b80840192508235915067ffffffffffffffff821115610db657610db5610d6d565b5b602083019250600182023603831315610dd257610dd1610d72565b5b509250929050565b600082905092915050565b60008190508160005260206000209050919050565b60006020601f8301049050919050565b600082821b905092915050565b600060088302610e477fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff82610e0a565b610e518683610e0a565b95508019841693508086168417925050509392505050565b6000819050919050565b6000819050919050565b6000610e98610e93610e8e84610e69565b610e73565b610e69565b9050919050565b6000819050919050565b610eb283610e7d565b610ec6610ebe82610e9f565b848454610e17565b825550505050565b600090565b610edb610ece565b610ee6818484610ea9565b505050565b5b81811015610f0a57610eff600082610ed3565b600181019050610eec565b5050565b601f821115610f4f57610f2081610de5565b610f2984610dfa565b81016020851015610f38578190505b610f4c610f4485610dfa565b830182610eeb565b50505b505050565b600082821c905092915050565b6000610f7260001984600802610f54565b1980831691505092915050565b6000610f8b8383610f61565b9150826002028217905092915050565b610fa58383610dda565b67ffffffffffffffff811115610fbe57610fbd610d0a565b5b610fc88254610cd9565b610fd3828285610f0e565b6000601f8311600181146110025760008415610ff0578287013590505b610ffa8582610f7f565b865550611062565b601f19841661101086610de5565b60005b8281101561103857848901358255600182019150602085019450602081019050611013565b868310156110555784890135611051601f891682610f61565b8355505b6001600288020188555050505b50505050505050565b600081905092915050565b82818337600083830152505050565b6000611091838561106b565b935061109e838584611076565b82840190509392505050565b60006110b7828486611085565b91508190509392505050565b600082825260208201905092915050565b7f307831422063616c6c206661696c656400000000000000000000000000000000600082015250565b600061110a6010836110c3565b9150611115826110d4565b602082019050919050565b60006020820190508181036000830152611139816110fd565b9050919050565b6000819050602082019050919050565b60007fffffffffffffffffffffffffffffffffffffffff00000000000000000000000082169050919050565b60006111888251611150565b80915050919050565b600061119c826109cf565b826111a684611140565b90506111b18161117c565b925060148210156111f1576111ec7fffffffffffffffffffffffffffffffffffffffff00000000000000000000000083601403600802610e0a565b831692505b5050919050565b6000819050919050565b61121361120e82610e69565b6111f8565b82525050565b60006112258285611202565b6020820191506112358284611202565b6020820191508190509392505050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601260045260246000fd5b600061127f82610e69565b915061128a83610e69565b92508261129a57611299611245565b5b828206905092915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b60006112df82610e69565b91506112ea83610e69565b9250828201905080821115611302576113016112a5565b5b92915050565b600081905092915050565b7f706c61696e746578743100000000000000000000000000000000000000000000600082015250565b6000611349600a83611308565b915061135482611313565b600a82019050919050565b600061136a8261133c565b9150819050919050565b7f706c61696e746578743200000000000000000000000000000000000000000000600082015250565b60006113aa600a83611308565b91506113b582611374565b600a82019050919050565b60006113cb8261139d565b9150819050919050565b600060408201905081810360008301526113ef8185610a80565b905081810360208301526114038184610a80565b90509392505050565b61141581610e69565b82525050565b600082825260208201905092915050565b6000611437826109cf565b611441818561141b565b93506114518185602086016109eb565b61145a81610a15565b840191505092915050565b600060408201905061147a600083018561140c565b818103602083015261148c818461142c565b90509392505050565b60006114a0826109cf565b6114aa818561106b565b93506114ba8185602086016109eb565b80840191505092915050565b60006114d28284611495565b915081905092915050565b6114e6826109cf565b67ffffffffffffffff8111156114ff576114fe610d0a565b5b6115098254610cd9565b611514828285610f0e565b600060209050601f8311600181146115475760008415611535578287015190505b61153f8582610f7f565b8655506115a7565b601f19841661155586610de5565b60005b8281101561157d57848901518255600182019150602085019450602081019050611558565b8683101561159a5784890151611596601f891682610f61565b8355505b6001600288020188555050505b50505050505056fef9015880b9015401cc5504bac92b5ccafa0c3202372d7bb0b8cb6861795deddafae0ed7be924ff170000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006360e4a05b2e03056b2d61c7ad2deb47b0be0084ffab44bf506bfff07b951fb0bf37c171584f74d80c96306e124152458183a7a2c570a136099f7c4ffc9dde340cbed4f87133200fc4e425946925eaac958209aba78e190feeb5c9f31182ec8d458260279adb3976c158471b932bbee5bb320cf9015880b9015401154918854780593f1c6bf620684b29ab3d4c4a4f5996dcbd1c1d0b48c06d56b40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000751884c80374b7b5d6d0ef740dacbea91b3d53ee5243eeacf94a970f131185c69dfd44e868e370602c72484bc2f34e9466255ef50ca817d34d61a46bff368318b6fff300eb566dac8d1c569a270c6d9c1f99664643582cafcb276fea83d5564cd38f4b1be0e8ee6c06e10f10f10dc39120884a26469706673582212209f594371591c2c5cc2a967353f54d26d3d236ad0e4c36a1598ff39cef70eae1664736f6c63781c302e382e33312d7072652e312b636f6d6d69742e6235393536366636004d";

    // deploy contract
    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = bytecode;
    create["gas"] = "1800000";
    create["value"] = "10000000000000000000";
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

    BOOST_REQUIRE( fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions()->size() == 1 );
    auto bite2Txn = fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions()->front();
    BOOST_REQUIRE( !bite2Txn.isInvalid() );
    BOOST_REQUIRE_NE( bite2Txn.sender(), dev::ZeroAddress );
    auto to = bite2Txn.to();
    BOOST_REQUIRE_EQUAL( to, dev::Address( contractAddress ) );

    Json::Value craftedCTXs = fixture.rpcClient->bite_getCraftedCtxs( txHash );
    BOOST_REQUIRE_EQUAL( craftedCTXs.size(), 1 );
    BOOST_REQUIRE_EQUAL( craftedCTXs[0].asString(), bite2Txn.sha3().hex() );

    dev::u256 randomGasLimit = dev::h256::Arith( dev::h256::random() ) % 2500000 + 1000000;
    dev::bytes randomGasLimitBytes = dev::toBigEndian( randomGasLimit );

    std::vector< dev::bytes > originalValues{ dev::h256::random().asBytes(), dev::h256::random().asBytes() };

    dev::bytes encryptedArg1 = formEncryptedMessageMockup( originalValues[0], dev::Address( contractAddress ) );
    dev::bytes encryptedArg2 = formEncryptedMessageMockup( originalValues[1], dev::Address( contractAddress ) );

    // Build abi.encode(bytes[] args1, bytes[] args2) with 2 elements each
    // args1 elements must be at least BITE_CIPHERTEXT_MIN_LEN bytes (encrypted data)
    std::vector<dev::bytes> args1 = {
        encryptedArg1, encryptedArg2
    };
    std::vector<dev::bytes> args2 = {
        dev::fromHex("706c61696e746578743122"),  // "plaintext1"
        dev::fromHex("706c61696e746578743222")   // "plaintext2"
    };

    dev::bytes randomData = buildAbiEncodedArrays( args1, args2 );

    // Build ABI-encoded input: abi.encode(address, uint256, bytes)
    // Format: gasLimit(32) + offset_to_bytes(32) + bytes_length(32) + bytes_data
    dev::bytes resultData;
    // gasLimit value (32 bytes)
    resultData.insert( resultData.end(), randomGasLimitBytes.begin(), randomGasLimitBytes.end() );

    // offset to bytes data (points to position 64 = 2 * 32)
    dev::bytes dataOffset = dev::toBigEndian( dev::u256( 64 ) );
    resultData.insert( resultData.end(), dataOffset.begin(), dataOffset.end() );
    // bytes data (length + content)
    dev::bytes dataLength = dev::toBigEndian( dev::u256( randomData.size() ) );
    resultData.insert( resultData.end(), dataLength.begin(), dataLength.end() );
    resultData.insert( resultData.end(), randomData.begin(), randomData.end() );

    txGenerate["to"] = contractAddress;
    txGenerate["data"] = "0x6040c1fb" + dev::toHex( dev::u256( 32 ) ) + dev::toHex( dev::u256( resultData.size() ) ) + dev::toHex( resultData );
    txGenerate["from"] = toJS( senderAddress );
    txGenerate["nonce"] = 2;
    std::string txGenerateHash = fixture.rpcClient->eth_sendTransaction( txGenerate );
    BOOST_REQUIRE_EQUAL( fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions()->size(), 1 );
    BOOST_REQUIRE_EQUAL( fixture.client->pending().size(), 1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    PrecompiledExecutor submitCTXExecutor = PrecompiledRegistrar::executor( "submitCTX" );
    dev::eth::PrecompiledCallContext ctx( fixture.client->number(), 1, 1, dev::h256::random(),
                                          dev::Address( contractAddress ), true );

    dev::bytesConstRef input( resultData.data(), resultData.size() );
    auto res = submitCTXExecutor( input, ctx );
    BOOST_REQUIRE( res.first );

    dev::Address addressFromPrecompiled( dev::bytes( res.second.begin(), res.second.begin() + dev::Address::size ) );

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
        dev::bite::ON_DECRYPT_FUNCTION_SELECTOR.begin(),
        dev::bite::ON_DECRYPT_FUNCTION_SELECTOR.end() );

    // Create expected transaction for signature verification using RLP-encoded data
    Transaction expectedTransaction( 0, gasPrice, randomGasLimit, dev::Address( contractAddress ), rlpEncodedData, 0 );
    dev::h256 expectedTxnHash = expectedTransaction.sha3( dev::eth::WithoutSignature );
    dev::Public expectedPublicKey = recover( vrs, expectedTxnHash );
    dev::Address expectedWalletAddress = dev::toAddress( expectedPublicKey );

    auto bn = fixture.client->number();
    BOOST_REQUIRE_EQUAL( fixture.client->transactions( bn ).size(), 2 );
    BOOST_REQUIRE( fixture.client->transactions( bn )[0].isCTX() );

    // call getDecrypted()
    Json::Value callDecrypted;
    callDecrypted["to"] = contractAddress;
    callDecrypted["data"] = "0x38d5a312";
    dev::bytes result = dev::fromHex( fixture.rpcClient->eth_call( callDecrypted, "latest" ) );
    auto [rlpStreamDecrypted, decryptedLength] = dev::bite::parseAbiEncodedBytesArray( dev::bytesConstRef( result.data(), result.size() ), 32, "" );
    BOOST_REQUIRE_EQUAL( decryptedLength, pregeneratedDecryptedValues.size() );
    dev::RLP rlpDecrypted( rlpStreamDecrypted.out() );
    for (size_t i = 0; i < decryptedLength; ++i) {
        dev::RLP decryptedPayload( rlpDecrypted[i].payload() );
        BOOST_REQUIRE( decryptedPayload[0].toBytes() == pregeneratedDecryptedValues[i] );
        BOOST_REQUIRE_EQUAL( dev::toHexPrefixed( decryptedPayload[1].toBytes() ), contractAddress );
    }

    // call getPlaintext()
    Json::Value callPlaintext;
    callPlaintext["to"] = contractAddress;
    callPlaintext["data"] = "0xcc159120";
    result = dev::fromHex( fixture.rpcClient->eth_call( callPlaintext, "latest" ) );
    auto [rlpStreamPlaintext, plaintextLength] = dev::bite::parseAbiEncodedBytesArray( dev::bytesConstRef( result.data(), result.size() ), 32, "" );
    BOOST_REQUIRE_EQUAL( plaintextLength, pregeneratedPlaintextValues.size() );
    dev::RLP rlpPlaintext( rlpStreamPlaintext.out() );
    for (size_t i = 0; i < plaintextLength; ++i) {
        BOOST_REQUIRE_EQUAL( dev::toHex( rlpPlaintext[i].toBytes() ), dev::toHex( pregeneratedPlaintextValues[i] ) );
    }

    BOOST_REQUIRE_EQUAL( fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions()->size(), 1 );

    auto pendingCTXs = fixture.client->blockChain().pendingCTXsList();
    BOOST_REQUIRE_EQUAL( pendingCTXs.size(), 1 );
    auto pendingCTX = pendingCTXs[0];
    BOOST_REQUIRE( pendingCTX.isCTX() );
    BOOST_REQUIRE_EQUAL( pendingCTX.to(), dev::Address( contractAddress ) );
    BOOST_REQUIRE_EQUAL( pendingCTX.gas(), randomGasLimit );
    BOOST_REQUIRE_EQUAL( pendingCTX.sender(), expectedWalletAddress );
    BOOST_REQUIRE_EQUAL( "0x" + pendingCTX.getCTXOrigin().hex(), txGenerateHash );

    dev::eth::mineTransaction( *( fixture.client ), 1 );
    bn = fixture.client->number();
    BOOST_REQUIRE_EQUAL( fixture.client->transactions( bn ).size(), 1 );
    Transaction ctxFromBlockchain = fixture.client->transaction( fixture.client->blockInfo( bn ).hash(), unsigned( 0 ) );

    BOOST_REQUIRE( ctxFromBlockchain.isCTX() );
    BOOST_REQUIRE_EQUAL( ctxFromBlockchain.to(), dev::Address( contractAddress ) );
    BOOST_REQUIRE_EQUAL( ctxFromBlockchain.gas(), randomGasLimit );
    BOOST_REQUIRE( ctxFromBlockchain.data() == rlpEncodedData );
    BOOST_REQUIRE( ctxFromBlockchain.signature() == vrs );
    BOOST_REQUIRE_EQUAL( ctxFromBlockchain.sender(), expectedWalletAddress );

    auto ctxOrigin = fixture.rpcClient->bite_getCtxOrigin( "0x" + ctxFromBlockchain.sha3().hex() );
    BOOST_REQUIRE_EQUAL( "0x" + ctxOrigin, txGenerateHash );

    // call getDecrypted()
    result = dev::fromHex( fixture.rpcClient->eth_call( callDecrypted, "latest" ) );
    auto [rlpStreamDecrypted1, decryptedLength1] = dev::bite::parseAbiEncodedBytesArray( dev::bytesConstRef( result.data(), result.size() ), 32, "" );
    BOOST_REQUIRE_EQUAL( decryptedLength1, pregeneratedDecryptedValues.size() );
    dev::RLP rlpDecrypted1( rlpStreamDecrypted1.out() );
    for (size_t i = 0; i < decryptedLength1; ++i) {
        dev::RLP decryptedPayload( rlpDecrypted1[i].payload() );
        BOOST_REQUIRE( decryptedPayload[0].toBytes() == originalValues[i] );
        BOOST_REQUIRE_EQUAL( dev::toHexPrefixed( decryptedPayload[1].toBytes() ), contractAddress );
    }

    // call getPlaintext()
    result = dev::fromHex( fixture.rpcClient->eth_call( callPlaintext, "latest" ) );
    auto [rlpStreamPlaintext1, plaintextLength1] = dev::bite::parseAbiEncodedBytesArray( dev::bytesConstRef( result.data(), result.size() ), 32, "" );
    BOOST_REQUIRE_EQUAL( plaintextLength1, pregeneratedPlaintextValues.size() );
    dev::RLP rlpPlaintext1( rlpStreamPlaintext1.out() );
    for (size_t i = 0; i < plaintextLength1; ++i) {
        BOOST_REQUIRE_EQUAL( dev::toHex( rlpPlaintext1[i].toBytes() ), dev::toHex( args2[i] ) );
    }

    // test submitCTXWithInput with randomGasLimit >> lastBlockGasLimit
    dev::u256 lastBlockGasLimit = fixture.client->blockInfo( fixture.client->number() ).gasLimit();
    dev::u256 randomGasLimit2 = lastBlockGasLimit * 10;
    dev::bytes randomGasLimitBytes2 = dev::toBigEndian( randomGasLimit2 );

    std::vector< dev::bytes > originalValues2{ dev::h256::random().asBytes(), dev::h256::random().asBytes() };

    dev::bytes encryptedArg1_2 = formEncryptedMessageMockup( originalValues2[0], dev::Address( contractAddress ) );
    dev::bytes encryptedArg2_2 = formEncryptedMessageMockup( originalValues2[1], dev::Address( contractAddress ) );

    std::vector<dev::bytes> args1_2 = {
        encryptedArg1_2, encryptedArg2_2
    };
    std::vector<dev::bytes> args2_2 = {
        dev::fromHex("706c61696e746578743122"),  // "plaintext1"
        dev::fromHex("706c61696e746578743222")   // "plaintext2"
    };

    dev::bytes randomData2 = buildAbiEncodedArrays( args1_2, args2_2 );

    dev::bytes resultData2;
    // gasLimit value (32 bytes) - much greater than block gas limit
    resultData2.insert( resultData2.end(), randomGasLimitBytes2.begin(), randomGasLimitBytes2.end() );

    // offset to bytes data (points to position 64 = 2 * 32)
    dev::bytes dataOffset2 = dev::toBigEndian( dev::u256( 64 ) );
    resultData2.insert( resultData2.end(), dataOffset2.begin(), dataOffset2.end() );
    // bytes data (length + content)
    dev::bytes dataLength2 = dev::toBigEndian( dev::u256( randomData2.size() ) );
    resultData2.insert( resultData2.end(), dataLength2.begin(), dataLength2.end() );
    resultData2.insert( resultData2.end(), randomData2.begin(), randomData2.end() );

    txGenerate["to"] = contractAddress;
    txGenerate["data"] = "0x6040c1fb" + dev::toHex( dev::u256( 32 ) ) + dev::toHex( dev::u256( resultData2.size() ) ) + dev::toHex( resultData2 );
    txGenerate["from"] = toJS( senderAddress );
    txGenerate["nonce"] = 3;
    std::string txGenerateHash2 = fixture.rpcClient->eth_sendTransaction( txGenerate );
    BOOST_REQUIRE_EQUAL( fixture.client->pending().size(), 1 );
    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txGenerateHash2 );
    BOOST_REQUIRE( txReceipt["status"] == "0x0" );

    auto bn2 = fixture.client->number();
    BOOST_REQUIRE_EQUAL( fixture.client->transactions( bn2 ).size(), 1 );

    BOOST_REQUIRE_EQUAL( fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions()->size(), 0 );
}
#endif  // !FAIR

BOOST_AUTO_TEST_CASE( submitCTXInContractConstructor ) {
    JsonRpcFixture fixture( c_BITEConfigString, true, true, true, true, false, -1, {{ "contractStorageLimit", "100000"}} );

    string senderAddress = toJS( fixture.coinbase.address() );

    std::vector< dev::bytes > pregeneratedDecryptedValues{ dev::fromHex( "5b221ee6b5c5751ff5808beddbc0644dc4fdda6b5efb13dbb49d698cb0e3f172" ),
                                                           dev::fromHex( "006aa7d63edcfb03635a2ecf5064a9eec076c2466fb2a6c35d59b5f1039f2535" ) };
    std::vector< dev::bytes > pregeneratedPlaintextValues{ dev::asBytes( "plaintext1" ), dev::asBytes( "plaintext2" ) };
//    pragma solidity ^0.8.13;

//    contract submitCTX {
//        bytes[] decrypted = new bytes[](1);
//        bytes[] plaintext = new bytes[](1);
//        constructor() payable {
//            uint256 randomNumber = uint256(keccak256(abi.encodePacked(block.timestamp, block.number))) % 2500000 + 1000000;
//            bytes[] memory args1 = new bytes[](2);
//            // Use pre-generated args1 values instead of generating them dynamically
//            args1[0] = hex"f9015880b9015401cc5504bac92b5ccafa0c3202372d7bb0b8cb6861795deddafae0ed7be924ff170000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006360e4a05b2e03056b2d61c7ad2deb47b0be0084ffab44bf506bfff07b951fb0bf37c171584f74d80c96306e124152458183a7a2c570a136099f7c4ffc9dde340cbed4f87133200fc4e425946925eaac958209aba78e190feeb5c9f31182ec8d458260279adb3976c158471b932bbee5bb320c";
//            args1[1] = hex"f9015880b9015401154918854780593f1c6bf620684b29ab3d4c4a4f5996dcbd1c1d0b48c06d56b40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000751884c80374b7b5d6d0ef740dacbea91b3d53ee5243eeacf94a970f131185c69dfd44e868e370602c72484bc2f34e9466255ef50ca817d34d61a46bff368318b6fff300eb566dac8d1c569a270c6d9c1f99664643582cafcb276fea83d5564cd38f4b1be0e8ee6c06e10f10f10dc39120884";

//            bytes[] memory args2 = new bytes[](2);
//            args2[0] = abi.encodePacked("plaintext1");
//            args2[1] = abi.encodePacked("plaintext2");

//            bytes memory randomBytes = abi.encode(args1, args2);
//            bytes memory input = abi.encode(randomNumber, randomBytes);

//            (bool success, bytes memory result) = address(0x1B).staticcall(input);
//            require(success, "0x1B call failed");

//            // Extract address from first 20 bytes of result and transfer
//            address walletAddress = address(bytes20(result));
//            payable(walletAddress).transfer(400000000000);
//        }
//    }
    std::string bytecode = "6080604052600167ffffffffffffffff81111561001f5761001e6104ee565b5b60405190808252806020026020018201604052801561005257816020015b606081526020019060019003908161003d5790505b5060009080519060200190610068929190610414565b50600167ffffffffffffffff811115610084576100836104ee565b5b6040519080825280602002602001820160405280156100b757816020015b60608152602001906001900390816100a25790505b50600190805190602001906100cd929190610414565b506000620f4240622625a042436040516020016100eb929190610548565b6040516020818303038152906040528051906020012060001c61010e91906105a3565b6101189190610603565b90506000600267ffffffffffffffff811115610137576101366104ee565b5b60405190808252806020026020018201604052801561016a57816020015b60608152602001906001900390816101555790505b50905060405180610180016040528061015b8152602001610df861015b91398160008151811061019d5761019c610637565b5b602002602001018190525060405180610180016040528061015b8152602001610f5361015b9139816001815181106101d8576101d7610637565b5b60200260200101819052506000600267ffffffffffffffff811115610200576101ff6104ee565b5b60405190808252806020026020018201604052801561023357816020015b606081526020019060019003908161021e5790505b509050604051602001610245906106bd565b6040516020818303038152906040528160008151811061026857610267610637565b5b60200260200101819052506040516020016102829061071e565b604051602081830303815290604052816001815181106102a5576102a4610637565b5b6020026020010181905250600082826040516020016102c5929190610885565b6040516020818303038152906040529050600084826040516020016102eb929190610915565b6040516020818303038152906040529050600080601b73ffffffffffffffffffffffffffffffffffffffff16836040516103259190610981565b600060405180830381855afa9150503d8060008114610360576040519150601f19603f3d011682016040523d82523d6000602084013e610365565b606091505b5091509150816103aa576040517f08c379a00000000000000000000000000000000000000000000000000000000081526004016103a1906109f5565b60405180910390fd5b6000816103b690610a73565b60601c90508073ffffffffffffffffffffffffffffffffffffffff166108fc645d21dba0009081150290604051600060405180830381858888f19350505050158015610406573d6000803e3d6000fd5b505050505050505050610dab565b82805482825590600052602060002090810192821561045c579160200282015b8281111561045b57825182908161044b9190610cd9565b5091602001919060010190610434565b5b509050610469919061046d565b5090565b5b8082111561048d57600081816104849190610491565b5060010161046e565b5090565b50805461049d90610b09565b6000825580601f106104af57506104ce565b601f0160209004906000526020600020908101906104cd91906104d1565b5b50565b5b808211156104ea5760008160009055506001016104d2565b5090565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b6000819050919050565b6000819050919050565b61054261053d8261051d565b610527565b82525050565b60006105548285610531565b6020820191506105648284610531565b6020820191508190509392505050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601260045260246000fd5b60006105ae8261051d565b91506105b98361051d565b9250826105c9576105c8610574565b5b828206905092915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b600061060e8261051d565b91506106198361051d565b9250828201905080821115610631576106306105d4565b5b92915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052603260045260246000fd5b600081905092915050565b7f706c61696e746578743100000000000000000000000000000000000000000000600082015250565b60006106a7600a83610666565b91506106b282610671565b600a82019050919050565b60006106c88261069a565b9150819050919050565b7f706c61696e746578743200000000000000000000000000000000000000000000600082015250565b6000610708600a83610666565b9150610713826106d2565b600a82019050919050565b6000610729826106fb565b9150819050919050565b600081519050919050565b600082825260208201905092915050565b6000819050602082019050919050565b600081519050919050565b600082825260208201905092915050565b60005b8381101561079957808201518184015260208101905061077e565b60008484015250505050565b6000601f19601f8301169050919050565b60006107c18261075f565b6107cb818561076a565b93506107db81856020860161077b565b6107e4816107a5565b840191505092915050565b60006107fb83836107b6565b905092915050565b6000602082019050919050565b600061081b82610733565b610825818561073e565b9350836020820285016108378561074f565b8060005b85811015610873578484038952815161085485826107ef565b945061085f83610803565b925060208a0199505060018101905061083b565b50829750879550505050505092915050565b6000604082019050818103600083015261089f8185610810565b905081810360208301526108b38184610810565b90509392505050565b6108c58161051d565b82525050565b600082825260208201905092915050565b60006108e78261075f565b6108f181856108cb565b935061090181856020860161077b565b61090a816107a5565b840191505092915050565b600060408201905061092a60008301856108bc565b818103602083015261093c81846108dc565b90509392505050565b600081905092915050565b600061095b8261075f565b6109658185610945565b935061097581856020860161077b565b80840191505092915050565b600061098d8284610950565b915081905092915050565b600082825260208201905092915050565b7f307831422063616c6c206661696c656400000000000000000000000000000000600082015250565b60006109df601083610998565b91506109ea826109a9565b602082019050919050565b60006020820190508181036000830152610a0e816109d2565b9050919050565b6000819050602082019050919050565b60007fffffffffffffffffffffffffffffffffffffffff00000000000000000000000082169050919050565b6000610a5d8251610a25565b80915050919050565b600082821b905092915050565b6000610a7e8261075f565b82610a8884610a15565b9050610a9381610a51565b92506014821015610ad357610ace7fffffffffffffffffffffffffffffffffffffffff00000000000000000000000083601403600802610a66565b831692505b5050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052602260045260246000fd5b60006002820490506001821680610b2157607f821691505b602082108103610b3457610b33610ada565b5b50919050565b60008190508160005260206000209050919050565b60006020601f8301049050919050565b600060088302610b8f7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff82610a66565b610b998683610a66565b95508019841693508086168417925050509392505050565b6000819050919050565b6000610bd6610bd1610bcc8461051d565b610bb1565b61051d565b9050919050565b6000819050919050565b610bf083610bbb565b610c04610bfc82610bdd565b848454610b5f565b825550505050565b600090565b610c19610c0c565b610c24818484610be7565b505050565b5b81811015610c4857610c3d600082610c11565b600181019050610c2a565b5050565b601f821115610c8d57610c5e81610b3a565b610c6784610b4f565b81016020851015610c76578190505b610c8a610c8285610b4f565b830182610c29565b50505b505050565b600082821c905092915050565b6000610cb060001984600802610c92565b1980831691505092915050565b6000610cc98383610c9f565b9150826002028217905092915050565b610ce28261075f565b67ffffffffffffffff811115610cfb57610cfa6104ee565b5b610d058254610b09565b610d10828285610c4c565b600060209050601f831160018114610d435760008415610d31578287015190505b610d3b8582610cbd565b865550610da3565b601f198416610d5186610b3a565b60005b82811015610d7957848901518255600182019150602085019450602081019050610d54565b86831015610d965784890151610d92601f891682610c9f565b8355505b6001600288020188555050505b505050505050565b603f80610db96000396000f3fe6080604052600080fdfea26469706673582212207f8d440a8b67a3a94ec70a43d822504c120782b181a52230ee20ee672f58781364736f6c634300081f0033f9015880b9015401cc5504bac92b5ccafa0c3202372d7bb0b8cb6861795deddafae0ed7be924ff170000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006360e4a05b2e03056b2d61c7ad2deb47b0be0084ffab44bf506bfff07b951fb0bf37c171584f74d80c96306e124152458183a7a2c570a136099f7c4ffc9dde340cbed4f87133200fc4e425946925eaac958209aba78e190feeb5c9f31182ec8d458260279adb3976c158471b932bbee5bb320cf9015880b9015401154918854780593f1c6bf620684b29ab3d4c4a4f5996dcbd1c1d0b48c06d56b40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000751884c80374b7b5d6d0ef740dacbea91b3d53ee5243eeacf94a970f131185c69dfd44e868e370602c72484bc2f34e9466255ef50ca817d34d61a46bff368318b6fff300eb566dac8d1c569a270c6d9c1f99664643582cafcb276fea83d5564cd38f4b1be0e8ee6c06e10f10f10dc39120884";

    // deploy contract
    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = bytecode;
    create["gas"] = "1800000";
    create["value"] = "10000000000000000000";
    create["nonce"] = 0;
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    auto txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    std::string contractAddress = txReceipt["contractAddress"].asString();
    BOOST_REQUIRE( txReceipt["status"] == "0x1" );

    // BITE2 txn queue should become non-empty
    BOOST_REQUIRE( fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions()->size() == 1 );
    auto bite2Txn = fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions()->front();
    BOOST_REQUIRE( !bite2Txn.isInvalid() );
    BOOST_REQUIRE_NE( bite2Txn.sender(), dev::ZeroAddress );
    auto to = bite2Txn.to();
    BOOST_REQUIRE_EQUAL( to, dev::Address( contractAddress ) );
}

BOOST_AUTO_TEST_CASE( CTXTransactionAfterRevert ) {
    JsonRpcFixture fixture( c_BITEConfigString, true, true, true, true, false, -1, {{ "contractStorageLimit", "100000" }} );

    string senderAddress = toJS( fixture.coinbase.address() );

//     pragma solidity ^0.8.13;

// contract submitCTXCaller {
//     bytes[] decrypted = new bytes[](1);
//     bytes[] plaintext = new bytes[](1);
//     constructor() payable {}

//     function submitCTX() public {
//         uint256 randomNumber = uint256(keccak256(abi.encodePacked(block.timestamp, block.number))) % 2500000 + 1000000;
//         bytes[] memory args1 = new bytes[](2);
//         // Use pre-generated args1 values instead of generating them dynamically
//         args1[0] = hex"f9015880b9015401cc5504bac92b5ccafa0c3202372d7bb0b8cb6861795deddafae0ed7be924ff170000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006360e4a05b2e03056b2d61c7ad2deb47b0be0084ffab44bf506bfff07b951fb0bf37c171584f74d80c96306e124152458183a7a2c570a136099f7c4ffc9dde340cbed4f87133200fc4e425946925eaac958209aba78e190feeb5c9f31182ec8d458260279adb3976c158471b932bbee5bb320c";
//         args1[1] = hex"f9015880b9015401154918854780593f1c6bf620684b29ab3d4c4a4f5996dcbd1c1d0b48c06d56b40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000751884c80374b7b5d6d0ef740dacbea91b3d53ee5243eeacf94a970f131185c69dfd44e868e370602c72484bc2f34e9466255ef50ca817d34d61a46bff368318b6fff300eb566dac8d1c569a270c6d9c1f99664643582cafcb276fea83d5564cd38f4b1be0e8ee6c06e10f10f10dc39120884";

//         bytes[] memory args2 = new bytes[](2);
//         args2[0] = abi.encodePacked("plaintext1");
//         args2[1] = abi.encodePacked("plaintext2");

//         bytes memory randomBytes = abi.encode(args1, args2);
//         bytes memory input = abi.encode(randomNumber, randomBytes);

//         (bool success, bytes memory result) = address(0x1B).call(input);
//         require(success, "0x1B call failed");

//         // Extract address from first 20 bytes of result and transfer
//         address walletAddress = address(bytes20(result));
//         payable(walletAddress).transfer(400000000000);
//     }

//     function submitCTXWithRevert() public {
//         uint256 randomNumber = uint256(keccak256(abi.encodePacked(block.timestamp, block.number))) % 2500000 + 1000000;
//         bytes[] memory args1 = new bytes[](2);
//         // Use pre-generated args1 values instead of generating them dynamically
//         args1[0] = hex"f9015880b9015401cc5504bac92b5ccafa0c3202372d7bb0b8cb6861795deddafae0ed7be924ff170000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006360e4a05b2e03056b2d61c7ad2deb47b0be0084ffab44bf506bfff07b951fb0bf37c171584f74d80c96306e124152458183a7a2c570a136099f7c4ffc9dde340cbed4f87133200fc4e425946925eaac958209aba78e190feeb5c9f31182ec8d458260279adb3976c158471b932bbee5bb320c";
//         args1[1] = hex"f9015880b9015401154918854780593f1c6bf620684b29ab3d4c4a4f5996dcbd1c1d0b48c06d56b40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000751884c80374b7b5d6d0ef740dacbea91b3d53ee5243eeacf94a970f131185c69dfd44e868e370602c72484bc2f34e9466255ef50ca817d34d61a46bff368318b6fff300eb566dac8d1c569a270c6d9c1f99664643582cafcb276fea83d5564cd38f4b1be0e8ee6c06e10f10f10dc39120884";

//         bytes[] memory args2 = new bytes[](2);
//         args2[0] = abi.encodePacked("plaintext1");
//         args2[1] = abi.encodePacked("plaintext2");

//         bytes memory randomBytes = abi.encode(args1, args2);
//         bytes memory input = abi.encode(randomNumber, randomBytes);

//         (bool success, bytes memory result) = address(0x1B).staticcall(input);
//         require(success, "0x1B call failed");

//         // Extract address from first 20 bytes of result and transfer
//         address walletAddress = address(bytes20(result));
//         payable(walletAddress).transfer(400000000000);
//         require(false);
//     }

//     function onDecrypt(bytes[] calldata decryptedArguments, bytes[] calldata plaintextArguments) public {
//         delete decrypted;
//         decrypted = new bytes[](decryptedArguments.length);
//         for (uint i = 0; i < decryptedArguments.length; ++i) {
//             decrypted[i] = decryptedArguments[i];
//         }
//         delete  plaintext;
//         plaintext = new bytes[](plaintextArguments.length);
//         for (uint i = 0; i < plaintextArguments.length; ++i) {
//             plaintext[i] = plaintextArguments[i];
//         }
//         return;
//     }

//     function getDecrypted() public view returns (bytes[] memory) {
//         return decrypted;
//     }

//     function getPlaintext() public view returns (bytes[] memory) {
//         return plaintext;
//     }
// }
    std::string bytecode = "6080604052600167ffffffffffffffff81111561001f5761001e6101ad565b5b60405190808252806020026020018201604052801561005257816020015b606081526020019060019003908161003d5790505b50600090805190602001906100689291906100d3565b50600167ffffffffffffffff811115610084576100836101ad565b5b6040519080825280602002602001820160405280156100b757816020015b60608152602001906001900390816100a25790505b50600190805190602001906100cd9291906100d3565b506104cf565b82805482825590600052602060002090810192821561011b579160200282015b8281111561011a57825182908161010a91906103fd565b50916020019190600101906100f3565b5b509050610128919061012c565b5090565b5b8082111561014c57600081816101439190610150565b5060010161012d565b5090565b50805461015c90610216565b6000825580601f1061016e575061018d565b601f01602090049060005260206000209081019061018c9190610190565b5b50565b5b808211156101a9576000816000905550600101610191565b5090565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b600081519050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052602260045260246000fd5b6000600282049050600182168061022e57607f821691505b602082108103610241576102406101e7565b5b50919050565b60008190508160005260206000209050919050565b60006020601f8301049050919050565b600082821b905092915050565b6000600883026102a97fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff8261026c565b6102b3868361026c565b95508019841693508086168417925050509392505050565b6000819050919050565b6000819050919050565b60006102fa6102f56102f0846102cb565b6102d5565b6102cb565b9050919050565b6000819050919050565b610314836102df565b61032861032082610301565b848454610279565b825550505050565b600090565b61033d610330565b61034881848461030b565b505050565b5b8181101561036c57610361600082610335565b60018101905061034e565b5050565b601f8211156103b15761038281610247565b61038b8461025c565b8101602085101561039a578190505b6103ae6103a68561025c565b83018261034d565b50505b505050565b600082821c905092915050565b60006103d4600019846008026103b6565b1980831691505092915050565b60006103ed83836103c3565b9150826002028217905092915050565b610406826101dc565b67ffffffffffffffff81111561041f5761041e6101ad565b5b6104298254610216565b610434828285610370565b600060209050601f8311600181146104675760008415610455578287015190505b61045f85826103e1565b8655506104c7565b601f19841661047586610247565b60005b8281101561049d57848901518255600182019150602085019450602081019050610478565b868310156104ba57848901516104b6601f8916826103c3565b8355505b6001600288020188555050505b505050505050565b6119ef806104de6000396000f3fe608060405234801561001057600080fd5b50600436106100575760003560e01c806338d5a3121461005c57806357983ac81461007a5780637372aa2614610096578063a2934a8c146100a0578063cc159120146100aa575b600080fd5b6100646100c8565b6040516100719190610d1f565b60405180910390f35b610094600480360381019061008f9190610db0565b6101a1565b005b61009e610366565b005b6100a86106ab565b005b6100b26109f9565b6040516100bf9190610d1f565b60405180910390f35b60606000805480602002602001604051908101604052809291908181526020016000905b8282101561019857838290600052602060002001805461010b90610e60565b80601f016020809104026020016040519081016040528092919081815260200182805461013790610e60565b80156101845780601f1061015957610100808354040283529160200191610184565b820191906000526020600020905b81548152906001019060200180831161016757829003601f168201915b5050505050815260200190600101906100ec565b50505050905090565b6000806101ae9190610ad2565b8383905067ffffffffffffffff8111156101cb576101ca610e91565b5b6040519080825280602002602001820160405280156101fe57816020015b60608152602001906001900390816101e95790505b5060009080519060200190610214929190610af3565b5060005b8484905081101561027f5784848281811061023657610235610ec0565b5b90506020028101906102489190610efe565b6000838154811061025c5761025b610ec0565b5b906000526020600020019182610273929190611122565b50806001019050610218565b506001600061028e9190610ad2565b8181905067ffffffffffffffff8111156102ab576102aa610e91565b5b6040519080825280602002602001820160405280156102de57816020015b60608152602001906001900390816102c95790505b50600190805190602001906102f4929190610af3565b5060005b8282905081101561035f5782828281811061031657610315610ec0565b5b90506020028101906103289190610efe565b6001838154811061033c5761033b610ec0565b5b906000526020600020019182610353929190611122565b508060010190506102f8565b5050505050565b6000620f4240622625a04243604051602001610383929190611213565b6040516020818303038152906040528051906020012060001c6103a6919061126e565b6103b091906112ce565b90506000600267ffffffffffffffff8111156103cf576103ce610e91565b5b60405190808252806020026020018201604052801561040257816020015b60608152602001906001900390816103ed5790505b50905060405180610180016040528061015b81526020016116ea61015b91398160008151811061043557610434610ec0565b5b602002602001018190525060405180610180016040528061015b815260200161184561015b9139816001815181106104705761046f610ec0565b5b60200260200101819052506000600267ffffffffffffffff81111561049857610497610e91565b5b6040519080825280602002602001820160405280156104cb57816020015b60608152602001906001900390816104b65790505b5090506040516020016104dd90611359565b60405160208183030381529060405281600081518110610500576104ff610ec0565b5b602002602001018190525060405160200161051a906113ba565b6040516020818303038152906040528160018151811061053d5761053c610ec0565b5b60200260200101819052506000828260405160200161055d9291906113cf565b60405160208183030381529060405290506000848260405160200161058392919061145f565b6040516020818303038152906040529050600080601b73ffffffffffffffffffffffffffffffffffffffff16836040516105bd91906114cb565b6000604051808303816000865af19150503d80600081146105fa576040519150601f19603f3d011682016040523d82523d6000602084013e6105ff565b606091505b509150915081610644576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161063b9061153f565b60405180910390fd5b600081610650906115b0565b60601c90508073ffffffffffffffffffffffffffffffffffffffff166108fc645d21dba0009081150290604051600060405180830381858888f193505050501580156106a0573d6000803e3d6000fd5b505050505050505050565b6000620f4240622625a042436040516020016106c8929190611213565b6040516020818303038152906040528051906020012060001c6106eb919061126e565b6106f591906112ce565b90506000600267ffffffffffffffff81111561071457610713610e91565b5b60405190808252806020026020018201604052801561074757816020015b60608152602001906001900390816107325790505b50905060405180610180016040528061015b81526020016116ea61015b91398160008151811061077a57610779610ec0565b5b602002602001018190525060405180610180016040528061015b815260200161184561015b9139816001815181106107b5576107b4610ec0565b5b60200260200101819052506000600267ffffffffffffffff8111156107dd576107dc610e91565b5b60405190808252806020026020018201604052801561081057816020015b60608152602001906001900390816107fb5790505b50905060405160200161082290611359565b6040516020818303038152906040528160008151811061084557610844610ec0565b5b602002602001018190525060405160200161085f906113ba565b6040516020818303038152906040528160018151811061088257610881610ec0565b5b6020026020010181905250600082826040516020016108a29291906113cf565b6040516020818303038152906040529050600084826040516020016108c892919061145f565b6040516020818303038152906040529050600080601b73ffffffffffffffffffffffffffffffffffffffff168360405161090291906114cb565b600060405180830381855afa9150503d806000811461093d576040519150601f19603f3d011682016040523d82523d6000602084013e610942565b606091505b509150915081610987576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161097e9061153f565b60405180910390fd5b600081610993906115b0565b60601c90508073ffffffffffffffffffffffffffffffffffffffff166108fc645d21dba0009081150290604051600060405180830381858888f193505050501580156109e3573d6000803e3d6000fd5b5060006109ef57600080fd5b5050505050505050565b60606001805480602002602001604051908101604052809291908181526020016000905b82821015610ac9578382906000526020600020018054610a3c90610e60565b80601f0160208091040260200160405190810160405280929190818152602001828054610a6890610e60565b8015610ab55780601f10610a8a57610100808354040283529160200191610ab5565b820191906000526020600020905b815481529060010190602001808311610a9857829003601f168201915b505050505081526020019060010190610a1d565b50505050905090565b5080546000825590600052602060002090810190610af09190610b4c565b50565b828054828255906000526020600020908101928215610b3b579160200282015b82811115610b3a578251829081610b2a9190611617565b5091602001919060010190610b13565b5b509050610b489190610b4c565b5090565b5b80821115610b6c5760008181610b639190610b70565b50600101610b4d565b5090565b508054610b7c90610e60565b6000825580601f10610b8e5750610bad565b601f016020900490600052602060002090810190610bac9190610bb0565b5b50565b5b80821115610bc9576000816000905550600101610bb1565b5090565b600081519050919050565b600082825260208201905092915050565b6000819050602082019050919050565b600081519050919050565b600082825260208201905092915050565b60005b83811015610c33578082015181840152602081019050610c18565b60008484015250505050565b6000601f19601f8301169050919050565b6000610c5b82610bf9565b610c658185610c04565b9350610c75818560208601610c15565b610c7e81610c3f565b840191505092915050565b6000610c958383610c50565b905092915050565b6000602082019050919050565b6000610cb582610bcd565b610cbf8185610bd8565b935083602082028501610cd185610be9565b8060005b85811015610d0d5784840389528151610cee8582610c89565b9450610cf983610c9d565b925060208a01995050600181019050610cd5565b50829750879550505050505092915050565b60006020820190508181036000830152610d398184610caa565b905092915050565b600080fd5b600080fd5b600080fd5b600080fd5b600080fd5b60008083601f840112610d7057610d6f610d4b565b5b8235905067ffffffffffffffff811115610d8d57610d8c610d50565b5b602083019150836020820283011115610da957610da8610d55565b5b9250929050565b60008060008060408587031215610dca57610dc9610d41565b5b600085013567ffffffffffffffff811115610de857610de7610d46565b5b610df487828801610d5a565b9450945050602085013567ffffffffffffffff811115610e1757610e16610d46565b5b610e2387828801610d5a565b925092505092959194509250565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052602260045260246000fd5b60006002820490506001821680610e7857607f821691505b602082108103610e8b57610e8a610e31565b5b50919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b7f4e487b7100000000000000000000000000000000000000000000000000000000600052603260045260246000fd5b600080fd5b600080fd5b600080fd5b60008083356001602003843603038112610f1b57610f1a610eef565b5b80840192508235915067ffffffffffffffff821115610f3d57610f3c610ef4565b5b602083019250600182023603831315610f5957610f58610ef9565b5b509250929050565b600082905092915050565b60008190508160005260206000209050919050565b60006020601f8301049050919050565b600082821b905092915050565b600060088302610fce7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff82610f91565b610fd88683610f91565b95508019841693508086168417925050509392505050565b6000819050919050565b6000819050919050565b600061101f61101a61101584610ff0565b610ffa565b610ff0565b9050919050565b6000819050919050565b61103983611004565b61104d61104582611026565b848454610f9e565b825550505050565b600090565b611062611055565b61106d818484611030565b505050565b5b818110156110915761108660008261105a565b600181019050611073565b5050565b601f8211156110d6576110a781610f6c565b6110b084610f81565b810160208510156110bf578190505b6110d36110cb85610f81565b830182611072565b50505b505050565b600082821c905092915050565b60006110f9600019846008026110db565b1980831691505092915050565b600061111283836110e8565b9150826002028217905092915050565b61112c8383610f61565b67ffffffffffffffff81111561114557611144610e91565b5b61114f8254610e60565b61115a828285611095565b6000601f8311600181146111895760008415611177578287013590505b6111818582611106565b8655506111e9565b601f19841661119786610f6c565b60005b828110156111bf5784890135825560018201915060208501945060208101905061119a565b868310156111dc57848901356111d8601f8916826110e8565b8355505b6001600288020188555050505b50505050505050565b6000819050919050565b61120d61120882610ff0565b6111f2565b82525050565b600061121f82856111fc565b60208201915061122f82846111fc565b6020820191508190509392505050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601260045260246000fd5b600061127982610ff0565b915061128483610ff0565b9250826112945761129361123f565b5b828206905092915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b60006112d982610ff0565b91506112e483610ff0565b92508282019050808211156112fc576112fb61129f565b5b92915050565b600081905092915050565b7f706c61696e746578743100000000000000000000000000000000000000000000600082015250565b6000611343600a83611302565b915061134e8261130d565b600a82019050919050565b600061136482611336565b9150819050919050565b7f706c61696e746578743200000000000000000000000000000000000000000000600082015250565b60006113a4600a83611302565b91506113af8261136e565b600a82019050919050565b60006113c582611397565b9150819050919050565b600060408201905081810360008301526113e98185610caa565b905081810360208301526113fd8184610caa565b90509392505050565b61140f81610ff0565b82525050565b600082825260208201905092915050565b600061143182610bf9565b61143b8185611415565b935061144b818560208601610c15565b61145481610c3f565b840191505092915050565b60006040820190506114746000830185611406565b81810360208301526114868184611426565b90509392505050565b600081905092915050565b60006114a582610bf9565b6114af818561148f565b93506114bf818560208601610c15565b80840191505092915050565b60006114d7828461149a565b915081905092915050565b600082825260208201905092915050565b7f307831422063616c6c206661696c656400000000000000000000000000000000600082015250565b60006115296010836114e2565b9150611534826114f3565b602082019050919050565b600060208201905081810360008301526115588161151c565b9050919050565b6000819050602082019050919050565b60007fffffffffffffffffffffffffffffffffffffffff00000000000000000000000082169050919050565b60006115a7825161156f565b80915050919050565b60006115bb82610bf9565b826115c58461155f565b90506115d08161159b565b925060148210156116105761160b7fffffffffffffffffffffffffffffffffffffffff00000000000000000000000083601403600802610f91565b831692505b5050919050565b61162082610bf9565b67ffffffffffffffff81111561163957611638610e91565b5b6116438254610e60565b61164e828285611095565b600060209050601f831160018114611681576000841561166f578287015190505b6116798582611106565b8655506116e1565b601f19841661168f86610f6c565b60005b828110156116b757848901518255600182019150602085019450602081019050611692565b868310156116d457848901516116d0601f8916826110e8565b8355505b6001600288020188555050505b50505050505056fef9015880b9015401cc5504bac92b5ccafa0c3202372d7bb0b8cb6861795deddafae0ed7be924ff170000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006360e4a05b2e03056b2d61c7ad2deb47b0be0084ffab44bf506bfff07b951fb0bf37c171584f74d80c96306e124152458183a7a2c570a136099f7c4ffc9dde340cbed4f87133200fc4e425946925eaac958209aba78e190feeb5c9f31182ec8d458260279adb3976c158471b932bbee5bb320cf9015880b9015401154918854780593f1c6bf620684b29ab3d4c4a4f5996dcbd1c1d0b48c06d56b40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000751884c80374b7b5d6d0ef740dacbea91b3d53ee5243eeacf94a970f131185c69dfd44e868e370602c72484bc2f34e9466255ef50ca817d34d61a46bff368318b6fff300eb566dac8d1c569a270c6d9c1f99664643582cafcb276fea83d5564cd38f4b1be0e8ee6c06e10f10f10dc39120884a2646970667358221220fc4b359f946612f95cce9c035d3fb72316657813aa43c0d7e630b942efcc079e64736f6c63781c302e382e33312d7072652e312b636f6d6d69742e6235393536366636004d";

    // deploy contract
    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = bytecode;
    create["gas"] = "1800000";
    create["value"] = "10000000000000000000";
    create["nonce"] = 0;
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    auto txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    std::string contractAddress = txReceipt["contractAddress"].asString();
    BOOST_REQUIRE( txReceipt["status"] == "0x1" );

    // call submitCTX()
    // BITE2 queue should become non-empty
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
    BOOST_REQUIRE_EQUAL( fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions()->size(), 1 );


    // call submitCTXWithRevert()
    // BITE2 queue size should remain empty
    Json::Value txFailGenerate;
    txFailGenerate["to"] = contractAddress;
    txFailGenerate["gas"] = "1000000";
    txFailGenerate["data"] = "0xa2934a8c";
    txFailGenerate["from"] = toJS( senderAddress );
    txFailGenerate["nonce"] = 2;
    txHash = fixture.rpcClient->eth_sendTransaction( txFailGenerate );

    dev::eth::mineTransaction( *( fixture.client ), 1 );

    txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    BOOST_REQUIRE( txReceipt["status"] == "0x0" );

    BOOST_REQUIRE_EQUAL( fixture.client->debugGetTransactionQueue()->pendingBITE2Transactions()->size(), 0 );
}

#ifndef FAIR  // ConsensusStub gasPrice(1000) < London baseFee in FAIR builds
BOOST_AUTO_TEST_CASE( CTXOutOfBlockGasLimit ) {
    JsonRpcFixture fixture( c_BITEConfigString, true, true, true, true, false, -1, {{ "contractStorageLimit", "100000" }} );

    dev::eth::g_skaleHost = fixture.client->skaleHost();

    string senderAddress = toJS( fixture.coinbase.address() );

   // pragma solidity ^0.8.13;

   // contract Precompile0x07Caller {
   //     bytes[] decrypted = new bytes[](1);
   //     bytes[] plaintext = new bytes[](1);
   //     constructor() payable {}

   //     function submitCTX(uint256 gasAmount) public {
   //         uint256 randomNumber = gasAmount + 100000;
   //         bytes[] memory args1 = new bytes[](2);
   //         // Use pre-generated args1 values instead of generating them dynamically
   //         args1[0] = hex"f9015880b9015401cc5504bac92b5ccafa0c3202372d7bb0b8cb6861795deddafae0ed7be924ff170000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006360e4a05b2e03056b2d61c7ad2deb47b0be0084ffab44bf506bfff07b951fb0bf37c171584f74d80c96306e124152458183a7a2c570a136099f7c4ffc9dde340cbed4f87133200fc4e425946925eaac958209aba78e190feeb5c9f31182ec8d458260279adb3976c158471b932bbee5bb320c";
   //         args1[1] = hex"f9015880b9015401154918854780593f1c6bf620684b29ab3d4c4a4f5996dcbd1c1d0b48c06d56b40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000751884c80374b7b5d6d0ef740dacbea91b3d53ee5243eeacf94a970f131185c69dfd44e868e370602c72484bc2f34e9466255ef50ca817d34d61a46bff368318b6fff300eb566dac8d1c569a270c6d9c1f99664643582cafcb276fea83d5564cd38f4b1be0e8ee6c06e10f10f10dc39120884";

   //         bytes[] memory args2 = new bytes[](1);
   //         args2[0] = abi.encode(gasAmount);

   //         bytes memory randomBytes = abi.encode(args1, args2);
   //         bytes memory input = abi.encode(randomNumber, randomBytes);

   //         (bool success, bytes memory result) = address(0x1B).staticcall(input);
   //         require(success, "0x1B call failed");

   //         // Extract address from first 20 bytes of result and transfer
   //         address walletAddress = address(bytes20(result));
   //         payable(walletAddress).transfer(25169190900000);
   //     }

   //     function onDecrypt(bytes[] calldata decryptedArguments, bytes[] calldata plaintextArguments) public {
   //         delete decrypted;
   //         decrypted = new bytes[](decryptedArguments.length);
   //         for (uint i = 0; i < decryptedArguments.length; ++i) {
   //             decrypted[i] = decryptedArguments[i];
   //         }
   //         delete plaintext;
   //         plaintext = new bytes[](plaintextArguments.length);
   //         for (uint i = 0; i < plaintextArguments.length; ++i) {
   //             plaintext[i] = plaintextArguments[i];
   //         }
   //         uint256 gasAmount = abi.decode(plaintext[0], (uint256));
   //         uint256 startGas = gasleft();
   //         while (startGas - gasleft() < gasAmount && gasleft() > 50000) {
   //             plaintext.push(abi.encode(gasleft()));
   //         }
   //         return;
   //     }

   //     function getDecrypted() public view returns (bytes[] memory) {
   //         return decrypted;
   //     }

   //     function getPlaintext() public view returns (bytes[] memory) {
   //         return plaintext;
   //     }
   // }

    std::string bytecode = "6080604052600167ffffffffffffffff81111561001f5761001e6101ad565b5b60405190808252806020026020018201604052801561005257816020015b606081526020019060019003908161003d5790505b50600090805190602001906100689291906100d3565b50600167ffffffffffffffff811115610084576100836101ad565b5b6040519080825280602002602001820160405280156100b757816020015b60608152602001906001900390816100a25790505b50600190805190602001906100cd9291906100d3565b506104cf565b82805482825590600052602060002090810192821561011b579160200282015b8281111561011a57825182908161010a91906103fd565b50916020019190600101906100f3565b5b509050610128919061012c565b5090565b5b8082111561014c57600081816101439190610150565b5060010161012d565b5090565b50805461015c90610216565b6000825580601f1061016e575061018d565b601f01602090049060005260206000209081019061018c9190610190565b5b50565b5b808211156101a9576000816000905550600101610191565b5090565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b600081519050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052602260045260246000fd5b6000600282049050600182168061022e57607f821691505b602082108103610241576102406101e7565b5b50919050565b60008190508160005260206000209050919050565b60006020601f8301049050919050565b600082821b905092915050565b6000600883026102a97fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff8261026c565b6102b3868361026c565b95508019841693508086168417925050509392505050565b6000819050919050565b6000819050919050565b60006102fa6102f56102f0846102cb565b6102d5565b6102cb565b9050919050565b6000819050919050565b610314836102df565b61032861032082610301565b848454610279565b825550505050565b600090565b61033d610330565b61034881848461030b565b505050565b5b8181101561036c57610361600082610335565b60018101905061034e565b5050565b601f8211156103b15761038281610247565b61038b8461025c565b8101602085101561039a578190505b6103ae6103a68561025c565b83018261034d565b50505b505050565b600082821c905092915050565b60006103d4600019846008026103b6565b1980831691505092915050565b60006103ed83836103c3565b9150826002028217905092915050565b610406826101dc565b67ffffffffffffffff81111561041f5761041e6101ad565b5b6104298254610216565b610434828285610370565b600060209050601f8311600181146104675760008415610455578287015190505b61045f85826103e1565b8655506104c7565b601f19841661047586610247565b60005b8281101561049d57848901518255600182019150602085019450602081019050610478565b868310156104ba57848901516104b6601f8916826103c3565b8355505b6001600288020188555050505b505050505050565b6116da806104de6000396000f3fe608060405234801561001057600080fd5b506004361061004c5760003560e01c806338d5a312146100515780634c6f6c221461006f57806357983ac81461008b578063cc159120146100a7575b600080fd5b6100596100c5565b6040516100669190610a9a565b60405180910390f35b61008960048036038101906100849190610afc565b61019e565b005b6100a560048036038101906100a09190610b8e565b61046f565b005b6100af610774565b6040516100bc9190610a9a565b60405180910390f35b60606000805480602002602001604051908101604052809291908181526020016000905b8282101561019557838290600052602060002001805461010890610c3e565b80601f016020809104026020016040519081016040528092919081815260200182805461013490610c3e565b80156101815780601f1061015657610100808354040283529160200191610181565b820191906000526020600020905b81548152906001019060200180831161016457829003601f168201915b5050505050815260200190600101906100e9565b50505050905090565b6000620186a0826101af9190610c9e565b90506000600267ffffffffffffffff8111156101ce576101cd610cd2565b5b60405190808252806020026020018201604052801561020157816020015b60608152602001906001900390816101ec5790505b50905060405180610180016040528061015b81526020016113d561015b91398160008151811061023457610233610d01565b5b602002602001018190525060405180610180016040528061015b815260200161153061015b91398160018151811061026f5761026e610d01565b5b60200260200101819052506000600167ffffffffffffffff81111561029757610296610cd2565b5b6040519080825280602002602001820160405280156102ca57816020015b60608152602001906001900390816102b55790505b509050836040516020016102de9190610d3f565b6040516020818303038152906040528160008151811061030157610300610d01565b5b602002602001018190525060008282604051602001610321929190610d5a565b604051602081830303815290604052905060008482604051602001610347929190610ddb565b6040516020818303038152906040529050600080601b73ffffffffffffffffffffffffffffffffffffffff16836040516103819190610e47565b600060405180830381855afa9150503d80600081146103bc576040519150601f19603f3d011682016040523d82523d6000602084013e6103c1565b606091505b509150915081610406576040517f08c379a00000000000000000000000000000000000000000000000000000000081526004016103fd90610ebb565b60405180910390fd5b60008161041290610f39565b60601c90508073ffffffffffffffffffffffffffffffffffffffff166108fc6516e428aed1209081150290604051600060405180830381858888f19350505050158015610463573d6000803e3d6000fd5b50505050505050505050565b60008061047c919061084d565b8383905067ffffffffffffffff81111561049957610498610cd2565b5b6040519080825280602002602001820160405280156104cc57816020015b60608152602001906001900390816104b75790505b50600090805190602001906104e292919061086e565b5060005b8484905081101561054d5784848281811061050457610503610d01565b5b90506020028101906105169190610faf565b6000838154811061052a57610529610d01565b5b9060005260206000200191826105419291906111bc565b508060010190506104e6565b506001600061055c919061084d565b8181905067ffffffffffffffff81111561057957610578610cd2565b5b6040519080825280602002602001820160405280156105ac57816020015b60608152602001906001900390816105975790505b50600190805190602001906105c292919061086e565b5060005b8282905081101561062d578282828181106105e4576105e3610d01565b5b90506020028101906105f69190610faf565b6001838154811061060a57610609610d01565b5b9060005260206000200191826106219291906111bc565b508060010190506105c6565b506000600160008154811061064557610644610d01565b5b90600052602060002001805461065a90610c3e565b80601f016020809104026020016040519081016040528092919081815260200182805461068690610c3e565b80156106d35780601f106106a8576101008083540402835291602001916106d3565b820191906000526020600020905b8154815290600101906020018083116106b657829003601f168201915b50505050508060200190518101906106eb91906112a1565b905060005a90505b815a8261070091906112ce565b10801561070e575061c3505a115b1561076c5760015a6040516020016107269190610d3f565b6040516020818303038152906040529080600181540180825580915050600190039060005260206000200160009091909190915090816107669190611302565b506106f3565b505050505050565b60606001805480602002602001604051908101604052809291908181526020016000905b828210156108445783829060005260206000200180546107b790610c3e565b80601f01602080910402602001604051908101604052809291908181526020018280546107e390610c3e565b80156108305780601f1061080557610100808354040283529160200191610830565b820191906000526020600020905b81548152906001019060200180831161081357829003601f168201915b505050505081526020019060010190610798565b50505050905090565b508054600082559060005260206000209081019061086b91906108c7565b50565b8280548282559060005260206000209081019282156108b6579160200282015b828111156108b55782518290816108a59190611302565b509160200191906001019061088e565b5b5090506108c391906108c7565b5090565b5b808211156108e757600081816108de91906108eb565b506001016108c8565b5090565b5080546108f790610c3e565b6000825580601f106109095750610928565b601f016020900490600052602060002090810190610927919061092b565b5b50565b5b8082111561094457600081600090555060010161092c565b5090565b600081519050919050565b600082825260208201905092915050565b6000819050602082019050919050565b600081519050919050565b600082825260208201905092915050565b60005b838110156109ae578082015181840152602081019050610993565b60008484015250505050565b6000601f19601f8301169050919050565b60006109d682610974565b6109e0818561097f565b93506109f0818560208601610990565b6109f9816109ba565b840191505092915050565b6000610a1083836109cb565b905092915050565b6000602082019050919050565b6000610a3082610948565b610a3a8185610953565b935083602082028501610a4c85610964565b8060005b85811015610a885784840389528151610a698582610a04565b9450610a7483610a18565b925060208a01995050600181019050610a50565b50829750879550505050505092915050565b60006020820190508181036000830152610ab48184610a25565b905092915050565b600080fd5b600080fd5b6000819050919050565b610ad981610ac6565b8114610ae457600080fd5b50565b600081359050610af681610ad0565b92915050565b600060208284031215610b1257610b11610abc565b5b6000610b2084828501610ae7565b91505092915050565b600080fd5b600080fd5b600080fd5b60008083601f840112610b4e57610b4d610b29565b5b8235905067ffffffffffffffff811115610b6b57610b6a610b2e565b5b602083019150836020820283011115610b8757610b86610b33565b5b9250929050565b60008060008060408587031215610ba857610ba7610abc565b5b600085013567ffffffffffffffff811115610bc657610bc5610ac1565b5b610bd287828801610b38565b9450945050602085013567ffffffffffffffff811115610bf557610bf4610ac1565b5b610c0187828801610b38565b925092505092959194509250565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052602260045260246000fd5b60006002820490506001821680610c5657607f821691505b602082108103610c6957610c68610c0f565b5b50919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b6000610ca982610ac6565b9150610cb483610ac6565b9250828201905080821115610ccc57610ccb610c6f565b5b92915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b7f4e487b7100000000000000000000000000000000000000000000000000000000600052603260045260246000fd5b610d3981610ac6565b82525050565b6000602082019050610d546000830184610d30565b92915050565b60006040820190508181036000830152610d748185610a25565b90508181036020830152610d888184610a25565b90509392505050565b600082825260208201905092915050565b6000610dad82610974565b610db78185610d91565b9350610dc7818560208601610990565b610dd0816109ba565b840191505092915050565b6000604082019050610df06000830185610d30565b8181036020830152610e028184610da2565b90509392505050565b600081905092915050565b6000610e2182610974565b610e2b8185610e0b565b9350610e3b818560208601610990565b80840191505092915050565b6000610e538284610e16565b915081905092915050565b600082825260208201905092915050565b7f307831422063616c6c206661696c656400000000000000000000000000000000600082015250565b6000610ea5601083610e5e565b9150610eb082610e6f565b602082019050919050565b60006020820190508181036000830152610ed481610e98565b9050919050565b6000819050602082019050919050565b60007fffffffffffffffffffffffffffffffffffffffff00000000000000000000000082169050919050565b6000610f238251610eeb565b80915050919050565b600082821b905092915050565b6000610f4482610974565b82610f4e84610edb565b9050610f5981610f17565b92506014821015610f9957610f947fffffffffffffffffffffffffffffffffffffffff00000000000000000000000083601403600802610f2c565b831692505b5050919050565b600080fd5b600080fd5b600080fd5b60008083356001602003843603038112610fcc57610fcb610fa0565b5b80840192508235915067ffffffffffffffff821115610fee57610fed610fa5565b5b60208301925060018202360383131561100a57611009610faa565b5b509250929050565b600082905092915050565b60008190508160005260206000209050919050565b60006020601f8301049050919050565b6000600883026110727fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff82610f2c565b61107c8683610f2c565b95508019841693508086168417925050509392505050565b6000819050919050565b60006110b96110b46110af84610ac6565b611094565b610ac6565b9050919050565b6000819050919050565b6110d38361109e565b6110e76110df826110c0565b848454611042565b825550505050565b600090565b6110fc6110ef565b6111078184846110ca565b505050565b5b8181101561112b576111206000826110f4565b60018101905061110d565b5050565b601f821115611170576111418161101d565b61114a84611032565b81016020851015611159578190505b61116d61116585611032565b83018261110c565b50505b505050565b600082821c905092915050565b600061119360001984600802611175565b1980831691505092915050565b60006111ac8383611182565b9150826002028217905092915050565b6111c68383611012565b67ffffffffffffffff8111156111df576111de610cd2565b5b6111e98254610c3e565b6111f482828561112f565b6000601f8311600181146112235760008415611211578287013590505b61121b85826111a0565b865550611283565b601f1984166112318661101d565b60005b8281101561125957848901358255600182019150602085019450602081019050611234565b868310156112765784890135611272601f891682611182565b8355505b6001600288020188555050505b50505050505050565b60008151905061129b81610ad0565b92915050565b6000602082840312156112b7576112b6610abc565b5b60006112c58482850161128c565b91505092915050565b60006112d982610ac6565b91506112e483610ac6565b92508282039050818111156112fc576112fb610c6f565b5b92915050565b61130b82610974565b67ffffffffffffffff81111561132457611323610cd2565b5b61132e8254610c3e565b61133982828561112f565b600060209050601f83116001811461136c576000841561135a578287015190505b61136485826111a0565b8655506113cc565b601f19841661137a8661101d565b60005b828110156113a25784890151825560018201915060208501945060208101905061137d565b868310156113bf57848901516113bb601f891682611182565b8355505b6001600288020188555050505b50505050505056fef9015880b9015401cc5504bac92b5ccafa0c3202372d7bb0b8cb6861795deddafae0ed7be924ff170000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006360e4a05b2e03056b2d61c7ad2deb47b0be0084ffab44bf506bfff07b951fb0bf37c171584f74d80c96306e124152458183a7a2c570a136099f7c4ffc9dde340cbed4f87133200fc4e425946925eaac958209aba78e190feeb5c9f31182ec8d458260279adb3976c158471b932bbee5bb320cf9015880b9015401154918854780593f1c6bf620684b29ab3d4c4a4f5996dcbd1c1d0b48c06d56b40000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000751884c80374b7b5d6d0ef740dacbea91b3d53ee5243eeacf94a970f131185c69dfd44e868e370602c72484bc2f34e9466255ef50ca817d34d61a46bff368318b6fff300eb566dac8d1c569a270c6d9c1f99664643582cafcb276fea83d5564cd38f4b1be0e8ee6c06e10f10f10dc39120884a26469706673582212207709b91a00629c07db20a39f6c7636d35cc5abbd73e615e9843332b487d0f11664736f6c63781c302e382e33312d7072652e312b636f6d6d69742e6235393536366636004d";

    // deploy contract
    Json::Value create;
    create["from"] = toJS( senderAddress );
    create["code"] = bytecode;
    create["gas"] = "1800000";
    create["value"] = "10000000000000000000";
    create["nonce"] = 0;
    string txHash = fixture.rpcClient->eth_sendTransaction( create );
    dev::eth::mineTransaction( *( fixture.client ), 1 );
    auto txReceipt = fixture.rpcClient->eth_getTransactionReceipt( txHash );
    std::string contractAddress = txReceipt["contractAddress"].asString();
    BOOST_REQUIRE( txReceipt["status"] == "0x1" );

    fixture.rpcClient->debug_pauseConsensus( true );

    // get block gas limit
    dev::u256 blockGasLimit = fixture.client->blockInfo( fixture.client->number() ).gasLimit();

    // send 2 submitCTXWithInput transactions in one block
    // total gasLimit specified in the payload of these transactions should extend block gas limit
    // gasUsed of these transactions corresponds to their gasLimit
    // regular txns should not be processed until CTX queue is not empty
    dev::u256 highGasLimit = (blockGasLimit * 90) / 100;
    dev::bytes highGasLimitBytes = dev::toBigEndian( highGasLimit );

    dev::u256 gasAmountForTx = highGasLimit;

    std::string calldata = "0x4c6f6c22" + dev::toHex( gasAmountForTx );

    auto startBlockNumber = fixture.client->number();

    // send a transaction -> BITE2 txn queue should become non-empty
    Json::Value txGenerate;
    txGenerate["to"] = contractAddress;
    txGenerate["gas"] = "1000000";
    txGenerate["data"] = calldata;
    txGenerate["from"] = toJS( senderAddress );
    txGenerate["nonce"] = 1;
    fixture.rpcClient->eth_sendTransaction( txGenerate );

    txGenerate["from"] = toJS( senderAddress );
    txGenerate["nonce"] = 2;
    fixture.rpcClient->eth_sendTransaction( txGenerate );

    fixture.rpcClient->debug_pauseConsensus( false );

    // sleep 50 ms - enough for pendingTransactions() to complete
    usleep( 50000 );

    // stop again to freeze the following pending queue state: 2 CTXs + 1 regular txn
    fixture.rpcClient->debug_pauseConsensus( true );

    while ( fixture.client->number() != startBlockNumber + 1 )
        usleep( 10000 );

    // sample regular txn
    Json::Value txRefill;
    txRefill["from"] = toJS( senderAddress );
    txRefill["to"] = "0xc868AF52a6549c773082A334E5AE232e0Ea3B513";
    txRefill["gasPrice"] = fixture.rpcClient->eth_gasPrice();
    txRefill["gas"] = 30000;
    txRefill["nonce"] = 3;
    fixture.rpcClient->eth_sendTransaction( txRefill );

    fixture.rpcClient->debug_pauseConsensus( false );

    // call mineTransaction to create a block
    dev::eth::mineTransaction( *( fixture.client ), 2 );

    // wait for 3 blocks to appear
    auto endBlockNumber = fixture.client->number();
    BOOST_REQUIRE_EQUAL( endBlockNumber, startBlockNumber + 3 );

    // check two last block - each should contain 1 CTX
    // last block also contains a regular txn
    auto beforeLastBlock = fixture.client->blockInfo( endBlockNumber - 1 );
    auto lastBlock = fixture.client->blockInfo( endBlockNumber );
    BOOST_REQUIRE_LT( lastBlock.gasUsed(), blockGasLimit );
    BOOST_REQUIRE_LT( beforeLastBlock.gasUsed(), blockGasLimit );

    auto beforeLastBlockTxns = fixture.client->transactions( endBlockNumber - 1 );
    auto lastBlockTxns = fixture.client->transactions( endBlockNumber );
    BOOST_REQUIRE_EQUAL( beforeLastBlockTxns.size(), 1 );
    BOOST_REQUIRE( beforeLastBlockTxns[0].isCTX() );
    BOOST_REQUIRE_EQUAL( lastBlockTxns.size(), 2 );
    BOOST_REQUIRE( lastBlockTxns[0].isCTX() );
    BOOST_REQUIRE( !lastBlockTxns[1].isCTX() );

    // check transactions status
    auto txnHashes = fixture.client->transactionHashes( endBlockNumber - 1 );
    auto receipt = fixture.rpcClient->eth_getTransactionReceipt( "0x" + txnHashes[0].hex() );
    BOOST_REQUIRE( receipt["status"] == "0x1" );

    txnHashes = fixture.client->transactionHashes( endBlockNumber );
    BOOST_REQUIRE_EQUAL( txnHashes.size(), 2 );
    for ( const auto& hash: txnHashes ) {
        auto receipt = fixture.rpcClient->eth_getTransactionReceipt( "0x" + hash.hex() );
        BOOST_REQUIRE( receipt["status"] == "0x1" );
    }
}
#endif  // !FAIR

#endif // BITE

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

    dev::bite::isCiphertextValidationEnabled = true;

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
#ifdef FAIR
    store1["gas"] = "200000";  // EIP-2929: need more gas for cold access costs
#else
    store1["gas"] = "111000";
#endif
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
                                       DecryptedTransactions{
#ifdef BITE
                                               std::make_shared< DecryptedCTXTxsMap >(),
#endif // BITE
                                               std::make_shared< DecryptedRegularTxsMap >()
                                           },
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
                                       DecryptedTransactions{
#ifdef BITE
                                               std::make_shared< DecryptedCTXTxsMap >(),
#endif // BITE
                                               std::make_shared< DecryptedRegularTxsMap >()
                                           },
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
