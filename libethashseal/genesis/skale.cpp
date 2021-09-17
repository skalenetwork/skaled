/*
    Copyright (C) 2018 SKALE Labs

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
 * @file skale.cpp
 * @author Bogdan Bliznyuk
 * @date 2018
 */

#include "../GenesisInfo.h"

static std::string const c_genesisInfoSkale = std::string() +
                                              R"E(
{
	"sealEngine": "Ethash",
	"params": {
        "accountStartNonce": "0x00",
        "homesteadForkBlock": "0x118c30",
        "daoHardforkBlock": "0x1d4c00",
        "EIP150ForkBlock": "0x259518",
        "EIP158ForkBlock": "0x28d138",
        "byzantiumForkBlock": "0x42ae50",
        "constantinopleForkBlock": "0x500000",
        "networkID" : "12313219",
        "chainID": "0x01",
        "maximumExtraDataSize": "0x20",
        "tieBreakingGas": false,
        "minGasLimit": "0x1388",
        "maxGasLimit": "7fffffffffffffff",
        "gasLimitBoundDivisor": "0x0400",
        "minimumDifficulty": "0x020000",
        "difficultyBoundDivisor": "0x0800",
        "durationLimit": "0x0d",
        "blockReward": "0x4563918244F40000"
    },
    "genesis": {
        "nonce": "0x0000000000000042",
        "difficulty": "0x020000",
        "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "author": "0x0000000000000000000000000000000000000000",
        "timestamp": "0x00",
        "parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "extraData": "0x11bbe8db4e347b4e8c937c1c8370e4b5ed33adb3db69cbdb7a38e1e50b1b82fa",
        "gasLimit": "0x47E7C4"
    },
   "skaleConfig": {
    "nodeInfo": {
      "nodeName": "Node1",
      "nodeID": 1112,
      "bindIP": "127.0.0.1",
      "basePort": 1231,
	  "bindIP6": "::1",
	  "basePort6": 1231,
      "logLevel": "trace",
      "logLevelProposal": "trace",
      "adminOrigins": [ "*" ],
	  "ipc": false,
	  "ipcpath": "./ipcx",
	  "db-path": "./node",
	  "httpRpcPort": 7000,
	  "httpsRpcPort": 7010,
	  "wsRpcPort": 7020,
	  "wssRpcPort": 7030,
      "httpRpcPort6": 7000,
      "httpsRpcPort6": 7010,
      "wsRpcPort6": 7040,
      "wssRpcPort6": 7050,
      "acceptors": 1,
      "infoHttpRpcPort": 8000,
      "infoHttpsRpcPort": 8010,
      "infoWsRpcPort": 8020,
      "infoWssRpcPort": 8030,
      "infoHttpRpcPort6": 8000,
      "infoHttpsRpcPort6": 8010,
      "infoWsRpcPort6": 8040,
      "infoWssRpcPort6": 8050,
      "pgHttpRpcPort": 9000,
      "pgHttpsRpcPort": 9010,
      "pgHttpRpcPort6": 9000,
      "pgHttpsRpcPort6": 9010,
      "infoPgHttpRpcPort": 10000,
      "infoPgHttpsRpcPort": 10010,
      "infoPgHttpRpcPort6": 10000,
      "infoPgHttpsRpcPort6": 10010,
      "info-acceptors": 0,
      "max-connections": 0,
	  "ws-mode": "simple",
	  "ws-log": "none",
	  "web3-trace": true,
	  "enable-debug-behavior-apis": false,
	  "unsafe-transactions": false
    },
    "sChain": {
        "schainName": "TestChain",
        "schainID": 1,
        "emptyBlockIntervalMs": 1000,
        "nodes": [
          { "nodeID": 1112, "ip": "127.0.0.1", "basePort": 1231, "ip6": "::1", "basePort6": 1231, "schainIndex" : 1}
        ]
    }
  },
    "accounts": {
        "0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
        "0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
        "0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
        "0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
        "0000000000000000000000000000000000000005": { "precompiled": { "name": "modexp", "startingBlock" : "0x2dc6c0" } },
        "0000000000000000000000000000000000000006": { "precompiled": { "name": "alt_bn128_G1_add", "startingBlock" : "0x2dc6c0", "linear": { "base": 500, "word": 0 } } },
        "0000000000000000000000000000000000000007": { "precompiled": { "name": "alt_bn128_G1_mul", "startingBlock" : "0x2dc6c0", "linear": { "base": 40000, "word": 0 } } },
        "0000000000000000000000000000000000000008": { "precompiled": { "name": "alt_bn128_pairing_product", "startingBlock" : "0x2dc6c0" } },
        "0xca4409573a5129a72edf85d6c51e26760fc9c903": { "balance": "100000000000000000000000" }
    }
}
)E";
