/*
    Modifications Copyright (C) 2018 SKALE Labs

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
/** @file ChainParams.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#pragma once

#include <shared_mutex>

#include "Account.h"
#include <json_spirit/json_spirit.h>
#include <libdevcore/Common.h>
#include <libethcore/BlockHeader.h>
#include <libethcore/ChainOperationParams.h>
#include <libethcore/Common.h>

namespace dev::eth {
class SealEngineFace;

struct ChainParams : public ChainOperationParams {
    ChainParams();
    ChainParams( ChainParams const& ) = delete;
    ChainParams& operator=( ChainParams const& ) = delete;
    ChainParams( std::string const& _s );
    ChainParams( bytes const& _genesisRLP, AccountMap const& _state ) {
        populateFromGenesis( _genesisRLP, _state );
    }
    ChainParams( std::string const& _json, bytes const& _genesisRLP, AccountMap const& _state )
        : ChainParams( _json ) {
        populateFromGenesis( _genesisRLP, _state );
    }

    friend struct ::SnapshotHashingFixture;
    friend struct ::JsonRpcFixture;
    friend struct ::SkaleHostFixture;
    friend class ::ConsensusExtFaceFixture;
    friend class ::SingleNodeConsensusFixture;

    SealEngineFace* createSealEngine();

    /// Genesis block info.
    bytes genesisBlock() const;

    /// load config
    void loadConfig( std::string const& _json, const boost::filesystem::path& _configPath = {} );

    const std::string& getOriginalJson() const;
    void resetJson() { originalJSON = ""; }

    bool checkAdminOriginAllowed( const std::string& origin ) const;
    void processSkaleConfigItems( json_spirit::mObject& _obj );

    std::string getConfigForConsensus() const;

    // ONLY FOR TESTS
    void fillDefaultTestsParameters( size_t _port );
    void setArchiveMode() { nodeInfo.archiveMode = true; }

    // SETTERS

    void setSgxServerUrl( const std::string& _url ) { nodeInfo.sgxServerUrl = _url; }

    // this setter is a workaround, use it carefully
    void setSealEngineName( const std::string& _name ) { sealEngineName = _name; }

    // GENERAL CHAIN GETTERS

    bool isAllowFutureBlocks() const { return allowFutureBlocks; }

    int getNetworkId() const { return networkID; }

    dev::Address getBlockAuthor() const { return sChain.blockAuthor; }

    std::string getSchainName() const { return sChain.name; }

#ifdef FAIR
    Address getNodeBeneficiaryInHistoricGroup( const unsigned, const uint64_t ) const;

    bool updateCurrentGroupIfNeeded( uint64_t _latestBlockTimestamp );

    CurrentGroup getNewestGroup() const;
#else

    u256 getExternalGasDifficulty() const { return externalGasDifficulty; }

    s256 getContractStorageLimit() const { return sChain.contractStorageLimit; }
#endif

    u256 getGasLimit() const { return gasLimit; }

    u256 getAccountStartNonce() const { return accountStartNonce; }

    u256 getAccountInitialFunds() const { return accountInitialFunds; }

    bool isMultiTransactionModeEnabled() const { return sChain.multiTransactionMode; }

    const AccountMap& getGenesisState() const { return genesisState; }

    uint64_t getDbStorageLimit() const { return sChain.dbStorageLimit; }

    uint64_t getConsensusStorageLimit() const { return sChain.consensusStorageLimit; }

#ifdef HISTORIC_STATE
    int64_t getMaxHistoricStateDbSize() const { return sChain.maxHistoricStateDbSize; }
#endif

    // GENERAL NODE GETTERS

    u256 getSelfNodeId() const { return nodeInfo.id; }

    std::string getSelfNodeIp() const { return nodeInfo.ip; }

    std::string getSelfNodeIpV6() const { return nodeInfo.ip6; }

    uint16_t getSelfNodePort() const { return nodeInfo.port; }

    std::array< std::string, 4 > getSelfBlsPublicKey() const;

    std::array< std::string, 4 > getCommonBlsPublicKey() const;

    std::vector< sChainNode > getSchainNodes() const;

    std::vector< NodeGroup > getNodeGroups() const { return sChain.nodeGroups; }

    NodeGroup getNodeGroupByIndex( size_t _idx ) const { return sChain.nodeGroups.at( _idx ); }

    sChainNode getNodeByIndex( size_t _idx ) const;

    int64_t getLevelDbReopenIntervalMs() const { return sChain.levelDBReopenIntervalMs; }

    int getLogsBlocksLimit() const { return logsBlocksLimit; }

    int getResponseLogCountLimit() const { return responseLogCountLimit; }

    bool isSyncNode() const { return nodeInfo.syncNode; }

    bool isArchiveModeEnabled() const { return nodeInfo.archiveMode; }

    bool isSyncFromCatchupEnabled() const { return nodeInfo.syncFromCatchup; }

    size_t getNodesCount() const;

    size_t getThresholdCount() const { return sChain.t; }

#ifdef FAIR
    Address getStakingContractAddress() const;
#endif

    // SGX GETTERS

    bool isTestSignaturesEnabled() const { return nodeInfo.testSignatures; }

    std::string getSgxServerUrl() const { return nodeInfo.sgxServerUrl; }

    std::string getKeyShareName() const;

    std::string getEcdsaKeyName() const { return nodeInfo.ecdsaKeyName; }

    // HISTORIC GROUP GETTERS

    std::array< std::string, 4 > getBlsPublicKeyForHistoricGroup(
        unsigned _historicGroupIndex ) const {
        return sChain.nodeGroups.at( _historicGroupIndex ).blsPublicKey;
    }


    u256 getHistoricNodeId( unsigned _historicGroupIndex, unsigned _nodeId ) const {
        return sChain.nodeGroups.at( _historicGroupIndex ).nodes.at( _nodeId ).id;
    }

    u256 getHistoricNodeIndex( unsigned _historicGroupIndex, unsigned _nodeId ) const {
        return sChain.nodeGroups.at( _historicGroupIndex ).nodes.at( _nodeId ).schainIndex;
    }

    std::string getHistoricNodePublicKey( unsigned _historicGroupIndex, unsigned _nodeId ) const {
        return sChain.nodeGroups.at( _historicGroupIndex ).nodes.at( _nodeId ).publicKey;
    }

#ifdef FAIR
    Address getHistoricNodeRewardWalletAddress(
        unsigned _historicGroupIndex, unsigned _nodeId ) const {
        return sChain.nodeGroups.at( _historicGroupIndex ).nodes.at( _nodeId ).rewardWalletAddress;
    }
#endif


    uint64_t getHistoricGroupFinishTs( unsigned _historicGroupIndex ) const {
        return sChain.nodeGroups.at( _historicGroupIndex ).finishTs;
    }

    // SNAPSHOTS GETTERS

    int getSnapshotIntervalSec() const { return sChain.snapshotIntervalSec; }

    time_t getSnapshotDownloadInactiveTimeout() const {
        return sChain.snapshotDownloadInactiveTimeout;
    }

    time_t getSnapshotDownloadTimeout() const { return sChain.snapshotDownloadTimeout; }

private:
    int rotateAfterBlock_ = 64;

    /// Genesis params.
    h256 parentHash = h256();
    Address author = Address();
    u256 difficulty = 1;
    u256 gasLimit = 1 << 31;
    u256 gasUsed = 0;
    u256 timestamp = 0;
    bytes extraData;
    mutable h256 stateRoot;  ///< Only pre-populate if known equivalent to genesisState's root. If
                             ///< they're different Bad Things Will Happen.
    AccountMap genesisState;

    unsigned sealFields = 0;
    bytes sealRLP;

    void populateFromGenesis( bytes const& _genesisRLP, AccountMap const& _state );

    /// load genesis
    void loadGenesis( std::string const& _json );

    mutable std::string originalJSON;

#ifdef FAIR
    void switchSyncMode( const std::vector< sChainNode >& _nodes );

    std::vector< u256 > getNodeIdsForCommittee();

    bool isInCommittee( const std::vector< sChainNode >& _committee ) const;

    mutable std::shared_mutex m_mutex;

    mutable Logger m_loggerInfo{ createLogger( VerbosityInfo, "ChainParams" ) };
    mutable Logger m_loggerWarning{ createLogger( VerbosityWarning, "ChainParams" ) };
#endif
    mutable Logger m_loggerDebug{ createLogger( VerbosityDebug, "ChainParams" ) };
};

}  // namespace dev::eth
