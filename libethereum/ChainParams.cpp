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
/** @file ChainParams.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#include "ChainParams.h"

#include <json_spirit/JsonSpiritHeaders.h>
#include <libdevcore/JsonUtils.h>
#include <libdevcore/Log.h>
#include <libdevcore/TrieDB.h>
#include <libethcore/BlockHeader.h>
#include <libethcore/Precompiled.h>
#include <libethcore/SealEngine.h>

#include "Account.h"
#include "GenesisInfo.h"
#include "ValidationSchemes.h"

using namespace std;
using namespace dev;
using namespace eth;
using namespace eth::validation;
namespace js = json_spirit;

ChainParams::ChainParams() {
    for ( unsigned i = 1; i <= 4; ++i )
        genesisState[Address( i )] = Account( 0, 1 );
    // Setup default precompiled contracts as equal to genesis of Frontier.
    precompiled.insert( make_pair( Address( 1 ),
        PrecompiledContract( 3000, 0, PrecompiledRegistrar::executor( "ecrecover" ) ) ) );
    precompiled.insert( make_pair(
        Address( 2 ), PrecompiledContract( 60, 12, PrecompiledRegistrar::executor( "sha256" ) ) ) );
    precompiled.insert( make_pair( Address( 3 ),
        PrecompiledContract( 600, 120, PrecompiledRegistrar::executor( "ripemd160" ) ) ) );
    precompiled.insert( make_pair( Address( 4 ),
        PrecompiledContract( 15, 3, PrecompiledRegistrar::executor( "identity" ) ) ) );
}

ChainParams::ChainParams( string const& _json ) {
    *this = loadConfig( _json );
}

ChainParams ChainParams::loadConfig(
    string const& _json, const boost::filesystem::path& _configPath ) const {
    ChainParams cp( *this );
    cp.originalJSON = _json;

    js::mValue val;
    json_spirit::read_string_or_throw( _json, val );
    js::mObject obj = val.get_obj();

    validateConfigJson( obj );
    cp.sealEngineName = obj[c_sealEngine].get_str();
    // params
    js::mObject params = obj[c_params].get_obj();
    //    validateFieldNames(params, c_knownParamNames);
    cp.accountStartNonce =
        u256( fromBigEndian< u256 >( fromHex( params[c_accountStartNonce].get_str() ) ) );
    cp.maximumExtraDataSize =
        u256( fromBigEndian< u256 >( fromHex( params[c_maximumExtraDataSize].get_str() ) ) );
    cp.tieBreakingGas =
        params.count( c_tieBreakingGas ) ? params[c_tieBreakingGas].get_bool() : true;
    cp.setBlockReward(
        u256( fromBigEndian< u256 >( fromHex( params[c_blockReward].get_str() ) ) ) );

    /// skale
    if ( obj.count( c_skaleConfig ) ) {
        auto skaleObj = obj[c_skaleConfig].get_obj();

        auto infoObj = skaleObj.at( "nodeInfo" ).get_obj();

        auto nodeName = infoObj.at( "nodeName" ).get_str();
        auto nodeID = infoObj.at( "nodeID" ).get_uint64();
        auto ip = infoObj.at( "bindIP" ).get_str();
        auto port = infoObj.at( "basePort" ).get_int();

        cp.nodeInfo = {nodeName, nodeID, ip, static_cast< uint16_t >( port )};

        auto sChainObj = skaleObj.at( "sChain" ).get_obj();
        SChain s{};
        s.nodes.clear();

        s.name = sChainObj.at( "schainName" ).get_str();
        s.id = sChainObj.at( "schainID" ).get_uint64();

        for ( auto nodeConf : sChainObj.at( "nodes" ).get_array() ) {
            auto nodeConfObj = nodeConf.get_obj();
            sChainNode node{};
            node.id = nodeConfObj.at( "nodeID" ).get_uint64();
            node.ip = nodeConfObj.at( "ip" ).get_str();
            node.port = nodeConfObj.at( "basePort" ).get_uint64();
            node.sChainIndex = nodeConfObj.at( "schainIndex" ).get_uint64();
            s.nodes.push_back( node );
        }
        cp.sChain = s;
    }  // if skale

    auto setOptionalU256Parameter = [&params]( u256& _destination, string const& _name ) {
        if ( params.count( _name ) )
            _destination = u256( fromBigEndian< u256 >( fromHex( params.at( _name ).get_str() ) ) );
    };
    setOptionalU256Parameter( cp.minGasLimit, c_minGasLimit );
    setOptionalU256Parameter( cp.maxGasLimit, c_maxGasLimit );
    setOptionalU256Parameter( cp.gasLimitBoundDivisor, c_gasLimitBoundDivisor );
    setOptionalU256Parameter( cp.homesteadForkBlock, c_homesteadForkBlock );
    setOptionalU256Parameter( cp.EIP150ForkBlock, c_EIP150ForkBlock );
    setOptionalU256Parameter( cp.EIP158ForkBlock, c_EIP158ForkBlock );
    setOptionalU256Parameter( cp.byzantiumForkBlock, c_byzantiumForkBlock );
    setOptionalU256Parameter( cp.eWASMForkBlock, c_eWASMForkBlock );
    setOptionalU256Parameter( cp.constantinopleForkBlock, c_constantinopleForkBlock );
    setOptionalU256Parameter( cp.experimentalForkBlock, c_experimentalForkBlock );
    setOptionalU256Parameter( cp.daoHardforkBlock, c_daoHardforkBlock );
    setOptionalU256Parameter( cp.minimumDifficulty, c_minimumDifficulty );
    setOptionalU256Parameter( cp.difficultyBoundDivisor, c_difficultyBoundDivisor );
    setOptionalU256Parameter( cp.durationLimit, c_durationLimit );
    setOptionalU256Parameter( cp.accountInitialFunds, c_accountInitialFunds );
    setOptionalU256Parameter( cp.externalGasDifficulty, c_externalGasDifficulty );

    if ( params.count( c_chainID ) )
        cp.chainID =
            int( u256( fromBigEndian< u256 >( fromHex( params.at( c_chainID ).get_str() ) ) ) );
    if ( params.count( c_networkID ) )
        cp.networkID =
            int( u256( fromBigEndian< u256 >( fromHex( params.at( c_networkID ).get_str() ) ) ) );
    cp.allowFutureBlocks = params.count( c_allowFutureBlocks );
    if ( cp.externalGasDifficulty == 0 ) {
        cp.externalGasDifficulty = -1;
    }

    // genesis
    string genesisStr = json_spirit::write_string( obj[c_genesis], false );
    cp = cp.loadGenesis( genesisStr );
    // genesis state
    string genesisStateStr = json_spirit::write_string( obj[c_accounts], false );

    cp.genesisState = jsonToAccountMap(
        genesisStateStr, cp.accountStartNonce, nullptr, &cp.precompiled, _configPath );

    return cp;
}

ChainParams ChainParams::loadGenesis( string const& _json ) const {
    ChainParams cp( *this );

    js::mValue val;
    js::read_string( _json, val );
    js::mObject genesis = val.get_obj();

    cp.parentHash = h256( 0 );  // required by the YP
    cp.author = genesis.count( c_coinbase ) ? h160( genesis[c_coinbase].get_str() ) :
                                              h160( genesis[c_author].get_str() );
    cp.difficulty =
        genesis.count( c_difficulty ) ?
            u256( fromBigEndian< u256 >( fromHex( genesis[c_difficulty].get_str() ) ) ) :
            cp.minimumDifficulty;
    cp.gasLimit = u256( fromBigEndian< u256 >( fromHex( genesis[c_gasLimit].get_str() ) ) );
    cp.gasUsed = genesis.count( c_gasUsed ) ?
                     u256( fromBigEndian< u256 >( fromHex( genesis[c_gasUsed].get_str() ) ) ) :
                     0;
    cp.timestamp = u256( fromBigEndian< u256 >( fromHex( genesis[c_timestamp].get_str() ) ) );
    cp.extraData = bytes( fromHex( genesis[c_extraData].get_str() ) );

    if ( genesis.count( c_stateRoot ) )
        cp.stateRoot = h256( fromHex( genesis[c_stateRoot].get_str() ) );

    // magic code for handling ethash stuff:
    if ( genesis.count( c_mixHash ) && genesis.count( c_nonce ) ) {
        h256 mixHash( genesis[c_mixHash].get_str() );
        h64 nonce( genesis[c_nonce].get_str() );
        cp.sealFields = 2;
        cp.sealRLP = rlp( mixHash ) + rlp( nonce );
    }

    return cp;
}

SealEngineFace* ChainParams::createSealEngine() {
    SealEngineFace* ret = SealEngineRegistrar::create( sealEngineName );
    assert( ret && "Seal engine not found" );
    if ( !ret )
        return nullptr;
    ret->setChainParams( *this );
    if ( sealRLP.empty() ) {
        sealFields = ret->sealFields();
        sealRLP = ret->sealRLP();
    }
    return ret;
}

void ChainParams::populateFromGenesis( bytes const& _genesisRLP, AccountMap const& _state ) {
    BlockHeader bi( _genesisRLP, RLP( &_genesisRLP )[0].isList() ? BlockData : HeaderData );
    parentHash = bi.parentHash();
    author = bi.author();
    difficulty = bi.difficulty();
    gasLimit = bi.gasLimit();
    gasUsed = bi.gasUsed();
    timestamp = bi.timestamp();
    extraData = bi.extraData();
    genesisState = _state;
    RLP r( _genesisRLP );
    sealFields = r[0].itemCount() - BlockHeader::BasicFields;
    sealRLP.clear();
    for ( unsigned i = BlockHeader::BasicFields; i < r[0].itemCount(); ++i )
        sealRLP += r[0][i].data();

    this->stateRoot = bi.stateRoot();

    auto b = genesisBlock();
    if ( b != _genesisRLP ) {
        cdebug << "Block passed:" << bi.hash() << bi.hash( WithoutSeal );
        cdebug << "Genesis now:" << BlockHeader::headerHashFromBlock( b );
        cdebug << RLP( b );
        cdebug << RLP( _genesisRLP );
        throw 0;
    }
}

bytes ChainParams::genesisBlock() const {
    RLPStream block( 3 );

    block.appendList( BlockHeader::BasicFields + sealFields )
        << parentHash << EmptyListSHA3       // sha3(uncles)
        << author << stateRoot << EmptyTrie  // transactions
        << EmptyTrie                         // receipts
        << LogBloom() << difficulty << 0     // number
        << gasLimit << gasUsed               // gasUsed
        << timestamp << extraData;
    block.appendRaw( sealRLP, sealFields );
    block.appendRaw( RLPEmptyList );
    block.appendRaw( RLPEmptyList );
    return block.out();
}

const std::string& ChainParams::getOriginalJson() const {
    if ( !originalJSON.empty() )
        return originalJSON;

    js::mObject obj;
    obj[c_sealEngine] = sealEngineName;

    // params
    js::mObject params;
    params[c_accountStartNonce] = toHex( toBigEndian( accountStartNonce ) );
    params[c_maximumExtraDataSize] = toHex( toBigEndian( maximumExtraDataSize ) );
    params[c_tieBreakingGas] = tieBreakingGas;
    params[c_blockReward] = toHex( toBigEndian( blockReward( DefaultSchedule ) ) );

    //    auto setOptionalU256Parameter = [&params](u256 &_destination, string const &_name) {
    //        if (params.count(_name))
    //            _destination = u256(fromBigEndian<u256>(fromHex(params.at(_name).get_str())));
    //    };
    params[c_minGasLimit] = toHex( toBigEndian( minGasLimit ) );
    params[c_maxGasLimit] = toHex( toBigEndian( maxGasLimit ) );
    params[c_gasLimitBoundDivisor] = toHex( toBigEndian( gasLimitBoundDivisor ) );
    params[c_homesteadForkBlock] = toHex( toBigEndian( homesteadForkBlock ) );
    params[c_EIP150ForkBlock] = toHex( toBigEndian( EIP150ForkBlock ) );
    params[c_EIP158ForkBlock] = toHex( toBigEndian( EIP158ForkBlock ) );
    params[c_byzantiumForkBlock] = toHex( toBigEndian( byzantiumForkBlock ) );
    params[c_eWASMForkBlock] = toHex( toBigEndian( eWASMForkBlock ) );
    params[c_constantinopleForkBlock] = toHex( toBigEndian( constantinopleForkBlock ) );
    params[c_eWASMForkBlock] = toHex( toBigEndian( eWASMForkBlock ) );
    params[c_daoHardforkBlock] = toHex( toBigEndian( daoHardforkBlock ) );
    params[c_minimumDifficulty] = toHex( toBigEndian( minimumDifficulty ) );
    params[c_difficultyBoundDivisor] = toHex( toBigEndian( difficultyBoundDivisor ) );
    params[c_durationLimit] = toHex( toBigEndian( durationLimit ) );

    params[c_chainID] = toHex( toBigEndian( u256( chainID ) ) );
    params[c_networkID] = toHex( toBigEndian( u256( networkID ) ) );
    params[c_allowFutureBlocks] = allowFutureBlocks;

    obj[c_params] = params;

    /////skale
    js::mObject skaleObj;

    js::mObject infoObj;

    infoObj["nodeName"] = nodeInfo.name;
    infoObj["nodeID"] = ( int64_t ) nodeInfo.id;
    infoObj["bindIP"] = nodeInfo.ip;
    infoObj["basePort"] = ( int64_t ) nodeInfo.port;  // TODO not so many bits!
    infoObj["logLevel"] = "trace";
    infoObj["logLevelProposal"] = "trace";
    infoObj["emptyBlockIntervalMs"] = nodeInfo.emptyBlockIntervalMs;

    skaleObj["nodeInfo"] = infoObj;

    js::mObject sChainObj;
    SChain s{};

    sChainObj["schainName"] = sChain.name;
    sChainObj["schainID"] = ( int64_t ) sChain.id;

    js::mArray nodes;

    for ( auto node : sChain.nodes ) {
        js::mObject nodeConfObj;
        nodeConfObj["nodeID"] = ( int64_t ) node.id;
        nodeConfObj["ip"] = node.ip;
        nodeConfObj["basePort"] = ( int64_t ) node.port;
        nodeConfObj["schainIndex"] = ( int64_t ) node.sChainIndex;

        nodes.push_back( nodeConfObj );
    }
    sChainObj["nodes"] = nodes;

    skaleObj["sChain"] = sChainObj;

    if ( !nodeInfo.ip.empty() )  // just test some meaningful field
        obj[c_skaleConfig] = skaleObj;

    //// genesis
    js::mObject genesis;

    genesis[c_author] = toHex( author.asBytes() );
    genesis[c_coinbase] = toHex( h256( author ).asBytes() );

    genesis[c_difficulty] = toHex( toBigEndian( difficulty ) );
    genesis[c_gasLimit] = toHex( toBigEndian( gasLimit ) );
    genesis[c_gasUsed] = toHex( toBigEndian( gasUsed ) );
    genesis[c_timestamp] = toHex( toBigEndian( timestamp ) );
    genesis[c_extraData] = toHex( extraData );
    genesis[c_mixHash] =
        "0x0000000000000000000000000000000000000000000000000000000000000000";  // HACK
    genesis[c_nonce] = "0x0000000000000042";

    obj[c_genesis] = genesis;

    // XXX ignore the account map for now

    originalJSON = js::write_string( ( js::mValue ) obj, true );

    return originalJSON;
}
