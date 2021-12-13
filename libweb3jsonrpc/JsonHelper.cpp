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
/** @file JsonHelper.cpp
 * @authors:
 *   Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "JsonHelper.h"

#include <rapidjson/prettywriter.h>

#include <jsonrpccpp/common/exception.h>
#include <libethcore/CommonJS.h>
#include <libethcore/SealEngine.h>
#include <libethereum/Client.h>

using namespace std;
using namespace dev;
using namespace eth;

namespace dev {

Json::Value toJson( unordered_map< u256, u256 > const& _storage ) {
    Json::Value res( Json::objectValue );
    for ( auto i : _storage )
        res[toJS( i.first )] = toJS( i.second );
    return res;
}

Json::Value toJson( map< h256, pair< u256, u256 > > const& _storage ) {
    Json::Value res( Json::objectValue );
    for ( auto i : _storage )
        res[toJS( u256( i.second.first ) )] = toJS( i.second.second );
    return res;
}

Json::Value toJson( Address const& _address ) {
    return toJS( _address );
}

// ////////////////////////////////////////////////////////////////////////////////
// eth
// ////////////////////////////////////////////////////////////////////////////////

namespace eth {

Json::Value toJson( dev::eth::BlockHeader const& _bi, SealEngineFace* _sealer ) {
    Json::Value res;
    if ( _bi ) {
        DEV_IGNORE_EXCEPTIONS( res["hash"] = toJS( _bi.hash() ) );
        res["parentHash"] = toJS( _bi.parentHash() );
        res["sha3Uncles"] = toJS( _bi.sha3Uncles() );
        res["author"] = toJS( _bi.author() );
        res["stateRoot"] = toJS( _bi.stateRoot() );
        res["transactionsRoot"] = toJS( _bi.transactionsRoot() );
        res["receiptsRoot"] = toJS( _bi.receiptsRoot() );
        res["number"] = toJS( _bi.number() );
        res["gasUsed"] = toJS( _bi.gasUsed() );
        res["gasLimit"] = toJS( _bi.gasLimit() );
        res["extraData"] = toJS( _bi.extraData() );
        res["logsBloom"] = toJS( _bi.logBloom() );
        res["timestamp"] = toJS( _bi.timestamp() );
        // TODO: remove once JSONRPC spec is updated to use "author" over "miner".
        res["miner"] = toJS( _bi.author() );
        if ( _sealer )
            for ( auto const& i : _sealer->jsInfo( _bi ) )
                res[i.first] = i.second;
    }
    return res;
}

Json::Value toJson( dev::eth::Transaction const& _t, std::pair< h256, unsigned > _location,
    BlockNumber _blockNumber ) {
    Json::Value res;
    if ( _t ) {
        res["hash"] = toJS( _t.sha3() );
        res["input"] = toJS( _t.data() );
        res["to"] = _t.isCreation() ? Json::Value() : toJS( _t.receiveAddress() );
        res["from"] = toJS( _t.safeSender() );
        res["gas"] = toJS( _t.gas() );
        res["gasPrice"] = toJS( _t.gasPrice() );
        res["nonce"] = toJS( _t.nonce() );
        res["value"] = toJS( _t.value() );
        res["blockHash"] = toJS( _location.first );
        res["transactionIndex"] = toJS( _location.second );
        res["blockNumber"] = toJS( _blockNumber );
        res["v"] = toJS( _t.signature().v );
        res["r"] = toJS( _t.signature().r );
        res["s"] = toJS( _t.signature().s );
    }
    return res;
}

Json::Value toJson( dev::eth::BlockHeader const& _bi, BlockDetails const& _bd,
    UncleHashes const& _us, Transactions const& _ts, SealEngineFace* _face ) {
    Json::Value res = toJson( _bi, _face );
    if ( _bi ) {
        res["totalDifficulty"] = toJS( _bd.totalDifficulty );
        res["size"] = toJS( _bd.blockSizeBytes );
        res["uncles"] = Json::Value( Json::arrayValue );
        for ( h256 h : _us )
            res["uncles"].append( toJS( h ) );
        res["transactions"] = Json::Value( Json::arrayValue );
        for ( unsigned i = 0; i < _ts.size(); i++ )
            res["transactions"].append(
                toJson( _ts[i], std::make_pair( _bi.hash(), i ), ( BlockNumber ) _bi.number() ) );
    }
    return res;
}

Json::Value toJson( dev::eth::BlockHeader const& _bi, BlockDetails const& _bd,
    UncleHashes const& _us, TransactionHashes const& _ts, SealEngineFace* _face ) {
    Json::Value res = toJson( _bi, _face );
    if ( _bi ) {
        res["totalDifficulty"] = toJS( _bd.totalDifficulty );
        res["size"] = toJS( _bd.blockSizeBytes );
        res["uncles"] = Json::Value( Json::arrayValue );
        for ( h256 h : _us )
            res["uncles"].append( toJS( h ) );
        res["transactions"] = Json::Value( Json::arrayValue );
        for ( h256 const& t : _ts )
            res["transactions"].append( toJS( t ) );
    }
    return res;
}

Json::Value toJson( dev::eth::TransactionSkeleton const& _t ) {
    Json::Value res;
    res["to"] = _t.creation ? Json::Value() : toJS( _t.to );
    res["from"] = toJS( _t.from );
    res["gas"] = toJS( _t.gas );
    res["gasPrice"] = toJS( _t.gasPrice );
    res["value"] = toJS( _t.value );
    res["data"] = toJS( _t.data, 32 );
    return res;
}

Json::Value toJson( dev::eth::TransactionReceipt const& _t ) {
    Json::Value res;
    if ( _t.hasStatusCode() )
        res["status"] = toString0x< uint8_t >( _t.statusCode() );  // toString( _t.statusCode() );
    else
        res["stateRoot"] = toJS( _t.stateRoot() );
    res["gasUsed"] = toJS( _t.cumulativeGasUsed() );
    res["bloom"] = toJS( _t.bloom() );
    res["log"] = dev::toJson( _t.log() );
    //
    std::string strRevertReason = _t.getRevertReason();
    if ( !strRevertReason.empty() )
        res["revertReason"] = strRevertReason;
    return res;
}

Json::Value toJson( dev::eth::LocalisedTransactionReceipt const& _t ) {
    Json::Value res;

    res["from"] = toJS( _t.from() );
    res["to"] = toJS( _t.to() );

    res["transactionHash"] = toJS( _t.hash() );
    res["transactionIndex"] = toJS( _t.transactionIndex() );
    res["blockHash"] = toJS( _t.blockHash() );
    res["blockNumber"] = toJS( _t.blockNumber() );
    res["cumulativeGasUsed"] = toJS( _t.cumulativeGasUsed() );
    res["gasUsed"] = toJS( _t.gasUsed() );
    //
    // The "contractAddress" field must be null for all types of trasactions but contract deployment
    // ones. The contract deployment transaction is special because it's the only type of
    // transaction with "to" filed set to null.
    //
    dev::Address contractAddress = _t.contractAddress();
    if ( contractAddress == dev::Address( 0 ) )
        res["contractAddress"] = Json::Value::nullRef;
    else
        res["contractAddress"] = toJS( contractAddress );
    //
    //
    res["logs"] = dev::toJson( _t.localisedLogs() );
    res["logsBloom"] = toJS( _t.bloom() );
    if ( _t.hasStatusCode() )
        res["status"] = toString0x< uint8_t >( _t.statusCode() );  // toString( _t.statusCode() );
    else
        res["stateRoot"] = toJS( _t.stateRoot() );
    //
    std::string strRevertReason = _t.getRevertReason();
    if ( !strRevertReason.empty() )
        res["revertReason"] = strRevertReason;
    return res;
}

#define ADD_FIELD_TO_RAPIDJSON( res, field, value, allocator )  \
    {                                                           \
        rapidjson::Value vv;                                    \
        vv.SetString( value.c_str(), value.size(), allocator ); \
        res.AddMember( field, vv, allocator );                  \
    }

rapidjson::Document toRapidJson(
    dev::eth::LogEntry const& _e, rapidjson::Document::AllocatorType& allocator ) {
    rapidjson::Document res;
    res.SetObject();

    ADD_FIELD_TO_RAPIDJSON( res, "data", toJS( _e.data ), allocator );
    ADD_FIELD_TO_RAPIDJSON( res, "address", toJS( _e.address ), allocator );
    rapidjson::Document jsonArray;
    jsonArray.SetArray();
    for ( auto const& t : _e.topics ) {
        rapidjson::Document d;
        rapidjson::Value v;
        std::string topic = toJS( t );
        v.SetString( topic.c_str(), topic.size(), allocator );
        d.CopyFrom( v, allocator );
        jsonArray.PushBack( d, allocator );
    }
    res.AddMember( "topics", jsonArray, allocator );
    return res;
}

rapidjson::Document toRapidJson(
    dev::eth::LocalisedLogEntry const& _e, rapidjson::Document::AllocatorType& allocator ) {
    rapidjson::Document res;
    res.SetObject();

    if ( _e.isSpecial ) {
        //        res = toJS( _e.special );
        rapidjson::Value v;
        std::string topic = toJS( _e.special );
        v.SetString( topic.c_str(), topic.size(), allocator );
        res.CopyFrom( v, allocator );
    } else {
        res = toRapidJson( static_cast< dev::eth::LogEntry const& >( _e ), allocator );
        res.AddMember( "polarity", _e.polarity == BlockPolarity::Live ? true : false, allocator );
        if ( _e.mined ) {
            res.AddMember( "type", "mined", allocator );
            ADD_FIELD_TO_RAPIDJSON( res, "blockNumber", toJS( _e.blockNumber ), allocator );
            ADD_FIELD_TO_RAPIDJSON( res, "blockHash", toJS( _e.blockHash ), allocator );
            ADD_FIELD_TO_RAPIDJSON( res, "logIndex", toJS( _e.logIndex ), allocator );
            ADD_FIELD_TO_RAPIDJSON( res, "transactionHash", toJS( _e.transactionHash ), allocator );
            ADD_FIELD_TO_RAPIDJSON(
                res, "transactionIndex", toJS( _e.transactionIndex ), allocator );
        } else {
            res["type"] = "pending";
            res.AddMember( "type", "pending", allocator );
            res.AddMember( "blockNumber", rapidjson::Value(), allocator );
            res.AddMember( "blockHash", rapidjson::Value(), allocator );
            res.AddMember( "logIndex", rapidjson::Value(), allocator );
            res.AddMember( "transactionHash", rapidjson::Value(), allocator );
            res.AddMember( "transactionIndex", rapidjson::Value(), allocator );
        }
    }
    return res;
}

rapidjson::Document toRapidJson( dev::eth::LocalisedTransactionReceipt const& _t,
    rapidjson::Document::AllocatorType& allocator ) {
    rapidjson::Document res;
    res.SetObject();

    ADD_FIELD_TO_RAPIDJSON( res, "from", toJS( _t.from() ), allocator );
    ADD_FIELD_TO_RAPIDJSON( res, "to", toJS( _t.to() ), allocator );

    ADD_FIELD_TO_RAPIDJSON( res, "transactionHash", toJS( _t.hash() ), allocator );
    ADD_FIELD_TO_RAPIDJSON( res, "transactionIndex", toJS( _t.transactionIndex() ), allocator );
    ADD_FIELD_TO_RAPIDJSON( res, "blockHash", toJS( _t.blockHash() ), allocator );
    ADD_FIELD_TO_RAPIDJSON( res, "blockNumber", toJS( _t.blockNumber() ), allocator );
    ADD_FIELD_TO_RAPIDJSON( res, "cumulativeGasUsed", toJS( _t.cumulativeGasUsed() ), allocator );
    ADD_FIELD_TO_RAPIDJSON( res, "gasUsed", toJS( _t.gasUsed() ), allocator );
    //
    // The "contractAddress" field must be null for all types of trasactions but contract deployment
    // ones. The contract deployment transaction is special because it's the only type of
    // transaction with "to" filed set to null.
    //
    dev::Address contractAddress = _t.contractAddress();
    if ( contractAddress == dev::Address( 0 ) )
        res.AddMember( "contractAddress", rapidjson::Value(), allocator );
    else
        ADD_FIELD_TO_RAPIDJSON( res, "contractAddress", toJS( contractAddress ), allocator );
    //
    //
    res.AddMember( "logs", dev::toRapidJson( _t.localisedLogs(), allocator ), allocator );
    ADD_FIELD_TO_RAPIDJSON( res, "logsBloom", toJS( _t.bloom() ), allocator );
    if ( _t.hasStatusCode() ) {
        ADD_FIELD_TO_RAPIDJSON(
            res, "status", toString0x< uint8_t >( _t.statusCode() ), allocator );
    } else {
        ADD_FIELD_TO_RAPIDJSON( res, "stateRoot", toJS( _t.stateRoot() ), allocator );
    }
    //

    std::string strRevertReason = _t.getRevertReason();
    if ( !strRevertReason.empty() ) {
        ADD_FIELD_TO_RAPIDJSON( res, "revertReason", strRevertReason, allocator );
    }

    return res;
}

void wrapJsonRpcException( const rapidjson::Document& /*joRequest*/,
    const jsonrpc::JsonRpcException& exception, rapidjson::Document& joResponse ) {
    if ( joResponse.HasMember( "result" ) ) {
        joResponse.RemoveMember( "result" );
    }

    rapidjson::Value joError;
    joError.SetObject();

    joError.AddMember( "code", exception.GetCode(), joResponse.GetAllocator() );

    std::string message = exception.GetMessage();
    joError.AddMember( "message", rapidjson::Value(), joResponse.GetAllocator() );
    joError["message"].SetString( message.c_str(), message.size(), joResponse.GetAllocator() );

    Json::Value joData = exception.GetData();
    joError.AddMember( "data", rapidjson::Value(), joResponse.GetAllocator() );
    if ( joData != Json::nullValue ) {
        Json::FastWriter fastWriter;
        std::string data = fastWriter.write( joData );
        joError["data"].SetString( data.c_str(), data.size(), joResponse.GetAllocator() );
    }

    joResponse.AddMember( "error", joError, joResponse.GetAllocator() );
}

Json::Value toJson( dev::eth::Transaction const& _t ) {
    Json::Value res;
    if ( _t ) {
        res["to"] = _t.isCreation() ? Json::Value() : toJS( _t.to() );
        res["from"] = toJS( _t.from() );
        res["gas"] = toJS( _t.gas() );
        res["gasPrice"] = toJS( _t.gasPrice() );
        res["value"] = toJS( _t.value() );
        res["data"] = toJS( _t.data(), 32 );
        res["nonce"] = toJS( _t.nonce() );
        res["r"] = toJS( _t.signature().r );
        res["s"] = toJS( _t.signature().s );
        res["v"] = toJS( _t.signature().v );
    }

    res["hash"] = toJS( _t.sha3( WithSignature ) );
    res["sighash"] = toJS( _t.sha3( WithoutSignature ) );

    return res;
}

Json::Value toJson( dev::eth::Transaction const& _t, bytes const& _rlp ) {
    Json::Value res;
    res["raw"] = toJS( _rlp );
    res["tx"] = toJson( _t );
    return res;
}

Json::Value toJson( dev::eth::LocalisedTransaction const& _t ) {
    Json::Value res;
    if ( _t ) {
        res["hash"] = toJS( _t.sha3() );
        res["input"] = toJS( _t.data() );
        res["to"] = _t.isCreation() ? Json::Value() : toJS( _t.receiveAddress() );
        res["from"] = toJS( _t.safeSender() );
        res["gas"] = toJS( _t.gas() );
        res["gasPrice"] = toJS( _t.gasPrice() );
        res["nonce"] = toJS( _t.nonce() );
        res["value"] = toJS( _t.value() );
        res["blockHash"] = toJS( _t.blockHash() );
        res["transactionIndex"] = toJS( _t.transactionIndex() );
        res["blockNumber"] = toJS( _t.blockNumber() );
    }
    return res;
}

Json::Value toJson( dev::eth::LocalisedLogEntry const& _e ) {
    Json::Value res;

    if ( _e.isSpecial )
        res = toJS( _e.special );
    else {
        res = toJson( static_cast< dev::eth::LogEntry const& >( _e ) );
        res["polarity"] = _e.polarity == BlockPolarity::Live ? true : false;
        if ( _e.mined ) {
            res["type"] = "mined";
            res["blockNumber"] = toJS( _e.blockNumber );
            res["blockHash"] = toJS( _e.blockHash );
            res["logIndex"] = toJS( _e.logIndex );
            res["transactionHash"] = toJS( _e.transactionHash );
            res["transactionIndex"] = toJS( _e.transactionIndex );
        } else {
            res["type"] = "pending";
            res["blockNumber"] = Json::Value( Json::nullValue );
            res["blockHash"] = Json::Value( Json::nullValue );
            res["logIndex"] = Json::Value( Json::nullValue );
            res["transactionHash"] = Json::Value( Json::nullValue );
            res["transactionIndex"] = Json::Value( Json::nullValue );
        }
    }
    return res;
}

Json::Value toJson( dev::eth::LogEntry const& _e ) {
    Json::Value res;
    res["data"] = toJS( _e.data );
    res["address"] = toJS( _e.address );
    res["topics"] = Json::Value( Json::arrayValue );
    for ( auto const& t : _e.topics )
        res["topics"].append( toJS( t ) );
    return res;
}

Json::Value toJson(
    std::unordered_map< h256, dev::eth::LocalisedLogEntries > const& _entriesByBlock,
    vector< h256 > const& _order ) {
    Json::Value res( Json::arrayValue );
    for ( auto const& i : _order ) {
        auto entries = _entriesByBlock.at( i );
        Json::Value currentBlock( Json::objectValue );
        LocalisedLogEntry entry = entries[0];
        if ( entry.mined ) {
            currentBlock["blockNumber"] = entry.blockNumber;
            currentBlock["blockHash"] = toJS( entry.blockHash );
            currentBlock["type"] = "mined";
        } else
            currentBlock["type"] = "pending";

        currentBlock["polarity"] = entry.polarity == BlockPolarity::Live ? true : false;
        currentBlock["logs"] = Json::Value( Json::arrayValue );

        for ( LocalisedLogEntry const& e : entries ) {
            Json::Value log( Json::objectValue );
            log["logIndex"] = e.logIndex;
            log["transactionIndex"] = e.transactionIndex;
            log["transactionHash"] = toJS( e.transactionHash );
            log["address"] = toJS( e.address );
            log["data"] = toJS( e.data );
            log["topics"] = Json::Value( Json::arrayValue );
            for ( auto const& t : e.topics )
                log["topics"].append( toJS( t ) );

            currentBlock["logs"].append( log );
        }

        res.append( currentBlock );
    }

    return res;
}

Json::Value toJsonByBlock( LocalisedLogEntries const& _entries ) {
    vector< h256 > order;
    unordered_map< h256, LocalisedLogEntries > entriesByBlock;

    for ( dev::eth::LocalisedLogEntry const& e : _entries ) {
        if ( e.isSpecial )  // skip special log
            continue;

        if ( entriesByBlock.count( e.blockHash ) == 0 ) {
            entriesByBlock[e.blockHash] = LocalisedLogEntries();
            order.push_back( e.blockHash );
        }

        entriesByBlock[e.blockHash].push_back( e );
    }

    return toJson( entriesByBlock, order );
}

TransactionSkeleton toTransactionSkeleton( Json::Value const& _json ) {
    TransactionSkeleton ret;
    if ( !_json.isObject() || _json.empty() )
        return ret;

    if ( !_json["from"].empty() )
        ret.from = jsToAddress( _json["from"].asString() );
    if ( !_json["to"].empty() && _json["to"].asString() != "0x" && !_json["to"].asString().empty() )
        ret.to = jsToAddress( _json["to"].asString() );
    else
        ret.creation = true;

    if ( !_json["value"].empty() )
        ret.value = jsToU256( _json["value"].asString() );

    if ( !_json["gas"].empty() )
        ret.gas = jsToU256( _json["gas"].asString() );

    if ( !_json["gasPrice"].empty() )
        ret.gasPrice = jsToU256( _json["gasPrice"].asString() );

    if ( !_json["data"].empty() )  // ethereum.js has preconstructed the data array
        ret.data = jsToBytes( _json["data"].asString(), OnFailed::Throw );

    if ( !_json["code"].empty() )
        ret.data = jsToBytes( _json["code"].asString(), OnFailed::Throw );

    if ( !_json["nonce"].empty() )
        ret.nonce = jsToU256( _json["nonce"].asString() );
    return ret;
}

TransactionSkeleton rapidJsonToTransactionSkeleton( rapidjson::Value const& _json ) {
    TransactionSkeleton ret;
    if ( !_json.IsObject() )
        return ret;

    if ( _json.HasMember( "from" ) ) {
        if ( !_json["from"].IsString() )
            throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
        ret.from = jsToAddress( _json["from"].GetString() );
    }

    if ( _json.HasMember( "to" ) ) {
        if ( _json["to"].IsString() ) {
            if ( strncmp( _json["to"].GetString(), "0x", 3 ) == 0 ||
                 strncmp( _json["to"].GetString(), "", 2 ) == 0 )
                ret.creation = true;
            else
                ret.to = jsToAddress( _json["to"].GetString() );
        } else {
            throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
        }
    } else
        ret.creation = true;

    if ( _json.HasMember( "value" ) ) {
        if ( !_json["value"].IsString() )
            throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
        ret.value = jsToU256( _json["value"].GetString() );
    }

    if ( _json.HasMember( "gas" ) ) {
        if ( !_json["gas"].IsString() )
            throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
        ret.gas = jsToU256( _json["gas"].GetString() );
    }

    if ( _json.HasMember( "gasPrice" ) ) {
        if ( !_json["gasPrice"].IsString() )
            throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
        ret.gasPrice = jsToU256( _json["gasPrice"].GetString() );
    }

    if ( _json.HasMember( "data" ) ) {  // ethereum.js has preconstructed
                                        // the data array
        if ( !_json["data"].IsString() )
            throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
        ret.data = jsToBytes( _json["data"].GetString(), OnFailed::Throw );
    }

    if ( _json.HasMember( "code" ) ) {
        if ( !_json["code"].IsString() )
            throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
        ret.data = jsToBytes( _json["code"].GetString(), OnFailed::Throw );
    }

    if ( _json.HasMember( "nonce" ) ) {
        if ( !_json["nonce"].IsString() )
            throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
        ret.nonce = jsToU256( _json["nonce"].GetString() );
    }

    return ret;
}
/*
dev::eth::LogFilter toLogFilter( Json::Value const& _json ) {
    dev::eth::LogFilter filter;
    if ( !_json.isObject() || _json.empty() )
        return filter;

    // check only !empty. it should throw exceptions if input params are incorrect
    if ( !_json["fromBlock"].empty() )
        filter.withEarliest( jsToFixed< 32 >( _json["fromBlock"].asString() ) );
    if ( !_json["toBlock"].empty() )
        filter.withLatest( jsToFixed< 32 >( _json["toBlock"].asString() ) );
    if ( !_json["address"].empty() ) {
        if ( _json["address"].isArray() )
            for ( auto i : _json["address"] )
                filter.address( jsToAddress( i.asString() ) );
        else
            filter.address( jsToAddress( _json["address"].asString() ) );
    }
    if ( !_json["topics"].empty() )
        for ( unsigned i = 0; i < _json["topics"].size(); i++ ) {
            if ( _json["topics"][i].isArray() ) {
                for ( auto t : _json["topics"][i] )
                    if ( !t.isNull() )
                        filter.topic( i, jsToFixed< 32 >( t.asString() ) );
            } else if ( !_json["topics"][i].isNull() )  // if it is anything else then string, it
                                                        // should and will fail
                filter.topic( i, jsToFixed< 32 >( _json["topics"][i].asString() ) );
        }
    return filter;
}
*/

// TODO: this should be removed once we decide to remove backward compatibility with old log filters
dev::eth::LogFilter toLogFilter( Json::Value const& _json )  // commented to avoid warning.
                                                             // Uncomment once in use @ PoC-7.
{
    dev::eth::LogFilter filter;
    if ( !_json.isObject() || _json.empty() )
        return filter;

    // check only !empty. it should throw exceptions if input params are incorrect
    if ( !_json["fromBlock"].empty() )
        filter.withEarliest( dev::eth::jsToBlockNumber( _json["fromBlock"].asString() ) );
    if ( !_json["toBlock"].empty() )
        filter.withLatest( dev::eth::jsToBlockNumber( _json["toBlock"].asString() ) );
    if ( !_json["address"].empty() ) {
        if ( _json["address"].isArray() )
            for ( auto i : _json["address"] )
                filter.address( jsToAddress( i.asString() ) );
        else
            filter.address( jsToAddress( _json["address"].asString() ) );
    }
    if ( !_json["topics"].empty() )
        for ( unsigned i = 0; i < _json["topics"].size(); i++ ) {
            if ( _json["topics"][i].isArray() ) {
                for ( auto t : _json["topics"][i] )
                    if ( !t.isNull() )
                        filter.topic( i, jsToFixed< 32 >( t.asString() ) );
            } else if ( !_json["topics"][i].isNull() )  // if it is anything else then string, it
                                                        // should and will fail
                filter.topic( i, jsToFixed< 32 >( _json["topics"][i].asString() ) );
        }
    return filter;
}

bool validateEIP1898Json( const rapidjson::Value& jo ) {
    if ( !jo.IsObject() )
        return false;

    if ( jo.HasMember( "blockHash" ) ) {
        if ( !jo["blockHash"].IsString() )
            return false;

        if ( jo.MemberCount() > 2 )
            return false;

        if ( jo.MemberCount() == 1 )
            return true;

        if ( !jo.HasMember( "requireCanonical" ) )
            return false;
        return jo["requireCanonical"].IsBool();
    } else if ( jo.HasMember( "blockNumber" ) ) {
        return jo["blockNumber"].IsString() && jo.MemberCount() == 1;
    }

    return false;
}

std::string getBlockFromEIP1898Json( const rapidjson::Value& jo ) {
    if ( jo.IsString() ) {
        return jo.GetString();
    }

    if ( !dev::eth::validateEIP1898Json( jo ) )
        throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );

    return "latest";
}

}  // namespace eth

// ////////////////////////////////////////////////////////////////////////////////////
// rpc
// ////////////////////////////////////////////////////////////////////////////////////

namespace rpc {
h256 h256fromHex( string const& _s ) {
    try {
        return h256( _s );
    } catch ( boost::exception const& ) {
        throw jsonrpc::JsonRpcException( "Invalid hex-encoded string: " + _s );
    }
}
}  // namespace rpc

}  // namespace dev
