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

#include <libdevcore/Common.h>
#include <libethcore/Precompiled.h>

#include "Common.h"
#include "EVMSchedule.h"

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

    bigint cost( bytesConstRef _in ) const { return m_cost( _in ); }
    std::pair< bool, bytes > execute( bytesConstRef _in ) const { return m_execute( _in ); }

    u256 const& startingBlock() const { return m_startingBlock; }

    bool executionAllowedFrom( const Address& _from ) const {
        return m_allowed_addresses.empty() || m_allowed_addresses.count( _from ) != 0;
    }

private:
    PrecompiledPricer m_cost;
    PrecompiledExecutor m_execute;
    u256 m_startingBlock = 0;
    h160Set m_allowed_addresses;
};

static constexpr int64_t c_infiniteBlockNumer = std::numeric_limits< int64_t >::max();

/// skale
struct NodeInfo {
public:
    std::string name;
    u256 id;
    std::string ip;
    uint16_t port;
    int emptyBlockIntervalMs = -1;

    NodeInfo( std::string _name = "TestNode", u256 _id = 1, std::string _ip = "127.0.0.11",
        uint16_t _port = 11111 ) {
        name = _name;
        id = _id;
        ip = _ip;
        port = _port;
    }
};

/// skale
struct sChainNode {
public:
    u256 id;
    std::string ip;
    u256 port;
    u256 sChainIndex;
};

/// skale
struct SChain {
public:
    std::string name;
    u256 id;
    std::vector< sChainNode > nodes;

    SChain() {
        name = "TestChain";
        id = 1;

        // HACK This creates one node and allows to run tests - BUT when loading config we need to
        // delete this explicitly!!
        sChainNode me = {u256( 1 ), "127.0.0.11", u256( 11111 ), u256( 1 )};
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
    EVMSchedule const& scheduleForBlockNumber( u256 const& _blockNumber ) const;
    u256 blockReward( EVMSchedule const& _schedule ) const;
    void setBlockReward( u256 const& _newBlockReward );
    u256 maximumExtraDataSize = 1024;
    u256 accountStartNonce = 0;
    bool tieBreakingGas = true;
    u256 minGasLimit;
    u256 maxGasLimit;
    u256 gasLimitBoundDivisor;
    u256 homesteadForkBlock = c_infiniteBlockNumer;
    u256 EIP150ForkBlock = c_infiniteBlockNumer;
    u256 EIP158ForkBlock = c_infiniteBlockNumer;
    u256 byzantiumForkBlock = c_infiniteBlockNumer;
    u256 eWASMForkBlock = c_infiniteBlockNumer;
    u256 constantinopleForkBlock = c_infiniteBlockNumer;
    u256 daoHardforkBlock = c_infiniteBlockNumer;
    u256 experimentalForkBlock = c_infiniteBlockNumer;
    int chainID = 0;    // Distinguishes different chains (mainnet, Ropsten, etc).
    int networkID = 0;  // Distinguishes different sub protocols.

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
};

}  // namespace eth
}  // namespace dev
