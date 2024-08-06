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
/** @file ChainOperationsParams.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#pragma once

#include <string>
#include <vector>

#include <libdevcore/Common.h>
#include <libethereum/Precompiled.h>
#include <libethereum/SchainPatchEnum.h>

#include "libethcore/Common.h"
#include "libethcore/EVMSchedule.h"

namespace dev {
namespace eth {

class PrecompiledContract {
public:
    PrecompiledContract() = default;
    PrecompiledContract( PrecompiledPricer const& _cost, PrecompiledExecutor const& _exec,
        u256 const& _startingBlock = 0, h160Set const& _allowedAddresses = h160Set() )
        : m_cost( _cost ),
          m_execute( _exec ),
          m_startingBlock( _startingBlock ),
          m_allowed_addresses( _allowedAddresses ) {}
    PrecompiledContract( unsigned _base, unsigned _word, PrecompiledExecutor const& _exec,
        u256 const& _startingBlock = 0, h160Set const& _allowedAddresses = h160Set() );

    bigint cost( bytesConstRef _in, ChainOperationParams const& _chainParams,
        u256 const& _blockNumber ) const {
        return m_cost( _in, _chainParams, _blockNumber );
    }
    std::pair< bool, bytes > execute(
        bytesConstRef _in, skale::OverlayFS* _overlayFS = nullptr ) const {
        return m_execute( _in, _overlayFS );
    }

    u256 const& startingBlock() const { return m_startingBlock; }

    bool executionAllowedFrom( const Address& _from, bool _readOnly ) const {
        return m_allowed_addresses.empty() ||
               ( m_allowed_addresses.count( _from ) != 0 && !_readOnly );
    }

private:
    PrecompiledPricer m_cost;
    PrecompiledExecutor m_execute;
    u256 m_startingBlock = 0;
    h160Set m_allowed_addresses;
};

static constexpr int64_t c_infiniteBlockNumber = std::numeric_limits< int64_t >::max();
// default value for leveldbReopenIntervalMs is 1 day
// negative value means reopenings are disabled
static constexpr int64_t c_defaultLevelDBReopenIntervalMs = 24 * 60 * 60 * 1000;

/// skale
struct NodeInfo {
public:
    std::string name;
    u256 id;
    std::string ip;
    uint16_t port;
    std::string ip6;
    uint16_t port6;
    std::string sgxServerUrl;
    std::string keyShareName;
    std::string ecdsaKeyName;
    std::array< std::string, 4 > BLSPublicKeys;
    std::array< std::string, 4 > commonBLSPublicKeys;
    bool syncNode;
    bool archiveMode;
    bool syncFromCatchup;
    bool testSignatures;

    NodeInfo( std::string _name = "TestNode", u256 _id = 1, std::string _ip = "127.0.0.11",
        uint16_t _port = 11111, std::string _ip6 = "::1", uint16_t _port6 = 11111,
        std::string _sgxServerUrl = "", std::string _ecdsaKeyName = "",
        std::string _keyShareName = "",
        const std::array< std::string, 4 >&
            _BLSPublicKeys = { "1085704699902305713594457076223282948137075635957851808699051999328"
                               "5655852781",
                "11559732032986387107991004021392285783925812861821192530917403151452391805634",
                "8495653923123431417604973247489272438418190587263600148770280649306958101930",
                "4082367875863433681332203403145435568316851327593401208105741076214120093531" },
        const std::array< std::string, 4 >&
            _commonBLSPublicKeys = { "1085704699902305713594457076223282948137075635957851808699051"
                                     "9993285655852781",
                "11559732032986387107991004021392285783925812861821192530917403151452391805634",
                "8495653923123431417604973247489272438418190587263600148770280649306958101930",
                "4082367875863433681332203403145435568316851327593401208105741076214120093531" },
        bool _syncNode = false, bool _archiveMode = false, bool _syncFromCatchup = false,
        bool _testSignatures = true ) {
        name = _name;
        id = _id;
        ip = _ip;
        port = _port;
        ip6 = _ip6;
        port6 = _port6;
        sgxServerUrl = _sgxServerUrl;
        ecdsaKeyName = _ecdsaKeyName;
        keyShareName = _keyShareName;
        BLSPublicKeys = _BLSPublicKeys;
        commonBLSPublicKeys = _commonBLSPublicKeys;
        syncNode = _syncNode;
        archiveMode = _archiveMode;
        syncFromCatchup = _syncFromCatchup;
        testSignatures = _testSignatures;
    }
};

/// skale
struct sChainNode {
public:
    u256 id;
    std::string ip;
    u256 port;
    std::string ip6;
    u256 port6;
    u256 sChainIndex;
    std::string publicKey;
    std::array< std::string, 4 > blsPublicKey;
};

/// skale
/// one node from previous or current group
struct GroupNode {
    u256 id;
    u256 schainIndex;
    std::string publicKey;
};

/// skale
/// previous or current group information
struct NodeGroup {
    std::vector< GroupNode > nodes;
    uint64_t finishTs;
    std::array< std::string, 4 > blsPublicKey;
};

/// skale
struct SChain {
public:
    std::string name;
    u256 id;
    Address owner;
    Address blockAuthor;
    std::vector< sChainNode > nodes;
    std::vector< NodeGroup > nodeGroups;
    s256 contractStorageLimit = 1000000000;
    uint64_t dbStorageLimit = 0;
    uint64_t consensusStorageLimit = 5000000000;  // default consensus storage limit
    int snapshotIntervalSec = -1;
    time_t snapshotDownloadTimeout = 3600;
    time_t snapshotDownloadInactiveTimeout = 60;
    bool freeContractDeployment = false;
    bool multiTransactionMode = false;
    int emptyBlockIntervalMs = -1;
    int64_t levelDBReopenIntervalMs = -1;
    size_t t = 1;

    // key is patch name
    // public - for tests, don't access it directly
    std::vector< time_t > _patchTimestamps =
        std::vector< time_t >( static_cast< int >( SchainPatchEnum::PatchesCount ) );
    time_t getPatchTimestamp( SchainPatchEnum _patchEnum ) const;

    SChain() {
        name = "TestChain";
        id = 1;

        // HACK This creates one node and allows to run tests - BUT when loading config we need to
        // delete this explicitly!!
        sChainNode me = { u256( 1 ), "127.0.0.11", u256( 11111 ), "::1", u256( 11111 ), u256( 1 ),
            "0xfa", { "0", "1", "0", "1" } };
        nodes.push_back( me );
    }
};


struct ChainOperationParams {
    ChainOperationParams();

    explicit operator bool() const { return accountStartNonce != Invalid256; }

    /// The chain sealer name: e.g. Ethash, NoProof, BasicAuthority
    std::string sealEngineName = "NoProof";

    /// General chain params.
private:
    u256 m_blockReward;

public:
    EVMSchedule const makeEvmSchedule(
        time_t _committedBlockTimestamp, u256 const& _workingBlockNumber ) const;
    u256 blockReward( EVMSchedule const& _schedule ) const;
    u256 blockReward( time_t _committedBlockTimestamp, u256 const& _workingBlockNumber ) const;
    void setBlockReward( u256 const& _newBlockReward );
    u256 maximumExtraDataSize = 1024;
    u256 accountStartNonce = 0;
    bool tieBreakingGas = true;
    u256 minGasLimit;
    u256 maxGasLimit;
    u256 gasLimitBoundDivisor;
    u256 homesteadForkBlock = c_infiniteBlockNumber;
    u256 EIP150ForkBlock = c_infiniteBlockNumber;
    u256 EIP158ForkBlock = c_infiniteBlockNumber;
    u256 byzantiumForkBlock = c_infiniteBlockNumber;
    u256 eWASMForkBlock = c_infiniteBlockNumber;
    u256 constantinopleForkBlock = c_infiniteBlockNumber;
    u256 constantinopleFixForkBlock = c_infiniteBlockNumber;
    u256 daoHardforkBlock = c_infiniteBlockNumber;
    u256 experimentalForkBlock = c_infiniteBlockNumber;
    u256 istanbulForkBlock = c_infiniteBlockNumber;
    u256 skale16ForkBlock = c_infiniteBlockNumber;
    u256 skale32ForkBlock = c_infiniteBlockNumber;
    u256 skale64ForkBlock = c_infiniteBlockNumber;
    u256 skale128ForkBlock = c_infiniteBlockNumber;
    u256 skale256ForkBlock = c_infiniteBlockNumber;
    u256 skale512ForkBlock = c_infiniteBlockNumber;
    u256 skale1024ForkBlock = c_infiniteBlockNumber;
    u256 skaleUnlimitedForkBlock = c_infiniteBlockNumber;
    bool skaleDisableChainIdCheck = false;
    uint64_t chainID = 0;  // Distinguishes different chains (mainnet, Ropsten, etc).
    int networkID = 0;     // Distinguishes different sub protocols.

    u256 minimumDifficulty;
    u256 difficultyBoundDivisor;
    u256 durationLimit;
    bool allowFutureBlocks = false;

    /// Precompiled contracts as specified in the chain params.
    std::unordered_map< Address, PrecompiledContract > precompiled;

    /// skale
    /// Skale additional config
    NodeInfo nodeInfo;
    SChain sChain;
    u256 accountInitialFunds = 0;
    u256 externalGasDifficulty = ~u256( 0 );
    typedef std::vector< std::string > vecAdminOrigins_t;
    vecAdminOrigins_t vecAdminOrigins;  // wildcard based folters for IP addresses
    int getLogsBlocksLimit = -1;

    time_t getPatchTimestamp( SchainPatchEnum _patchEnum ) const;

    bool isPrecompiled( Address const& _a, u256 const& _blockNumber ) const {
        return precompiled.count( _a ) != 0 && _blockNumber >= precompiled.at( _a ).startingBlock();
    }
    bigint costOfPrecompiled(
        Address const& _a, bytesConstRef _in, u256 const& _blockNumber ) const {
        return precompiled.at( _a ).cost( _in, *this, _blockNumber );
    }
    std::pair< bool, bytes > executePrecompiled( Address const& _a, bytesConstRef _in, u256 const&,
        skale::OverlayFS* _overlayFS = nullptr ) const {
        return precompiled.at( _a ).execute( _in, _overlayFS );
    }
    bool precompiledExecutionAllowedFrom(
        Address const& _a, Address const& _from, bool _readOnly ) const {
        return precompiled.at( _a ).executionAllowedFrom( _from, _readOnly );
    }
};

}  // namespace eth
}  // namespace dev
