/*
    Copyright (C) 2019-present, SKALE Labs

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
 * @file SkaleClient.cpp
 * @author Dima Litvinov
 * @date 2019
 */

#include <libethcore/CommonJS.h>

#include "SkaleClient.h"

SkaleClient::SkaleClient( jsonrpc::IClientConnector& conn, jsonrpc::clientVersion_t type )
    : jsonrpc::Client( conn, type ) {}

std::string SkaleClient::skale_shutdownInstance() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "skale_shutdownInstance", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string SkaleClient::skale_protocolVersion() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "skale_protocolVersion", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string SkaleClient::skale_receiveTransaction( const std::string& _rlp ) {
    Json::Value p;
    Json::Value result;
    p.append( _rlp );

    result = this->CallMethod( "skale_receiveTransaction", p );

    if ( result.isString() ) {
        return result.asString();
    } else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value SkaleClient::skale_getMessageSignature( unsigned blockNumber ) {
    Json::Value p;
    Json::Value result;
    p.append( blockNumber );

    result = this->CallMethod( "skale_getMessageSignature", p );

    if ( result.isObject() ) {
        return result;
    } else {
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
    }
}

Json::Value SkaleClient::skale_imaInfo() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result;

    result = this->CallMethod( "skale_imaInfo", p );

    if ( result.isObject() ) {
        return result;
    } else {
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
    }
}

unsigned SkaleClient::skale_getLatestSnapshotBlockNumber() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result;

    result = this->CallMethod( "skale_getLatestSnapshotBlockNumber", p );

    if ( result.isString() ) {
        return dev::eth::jsToBlockNumber( result.asString() );
    } else {
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
    }
}
