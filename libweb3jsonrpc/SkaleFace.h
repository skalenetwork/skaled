/*
    Copyright (C) 2018-present, SKALE Labs

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
 * @file SkaleFace.h
 * @author Bogdan Bliznyuk
 * @date 2018
 */

#ifndef CPP_ETHEREUM_SKALEFACE_H
#define CPP_ETHEREUM_SKALEFACE_H

#include "ModularServer.h"
#include <iostream>

namespace dev {
namespace rpc {
class SkaleFace : public ServerInterface< SkaleFace > {
    inline virtual void skale_protocolVersionI(
        const Json::Value& request, Json::Value& response ) {
        ( void ) request;
        response = this->skale_protocolVersion();
    }

    inline virtual void skale_receiveTransactionI(
        const Json::Value& request, Json::Value& response ) {
        std::string str = request[0u].asString();
        response = this->skale_receiveTransaction( str );
    }

    inline virtual void skale_shutdownInstanceI(
        const Json::Value& request, Json::Value& response ) {
        ( void ) request;
        response = this->skale_shutdownInstance();
    }

    inline virtual void skale_getSnapshotI( const Json::Value& request, Json::Value& response ) {
        response = this->skale_getSnapshot( request );
    }
    inline virtual void skale_downloadSnapshotFragmentI(
        const Json::Value& request, Json::Value& response ) {
        response = this->skale_downloadSnapshotFragment( request );
    }

    inline virtual void skale_getSnapshotSignatureI(
        const Json::Value& request, Json::Value& response ) {
        unsigned blockNumber = request[0u].asInt();
        response = this->skale_getSnapshotSignature( blockNumber );
    }

    inline virtual void skale_getLatestSnapshotBlockNumberI(
        const Json::Value& request, Json::Value& response ) {
        ( void ) request;
        response = this->skale_getLatestSnapshotBlockNumber();
    }

    inline virtual void skale_getLatestBlockNumberI(
        const Json::Value& request, Json::Value& response ) {
        ( void ) request;
        response = this->skale_getLatestBlockNumber();
    }

    inline virtual void skale_getGenesisStateI(
        const Json::Value& request, Json::Value& response ) {
        ( void ) request;
        response = this->skale_getGenesisState();
    }

    inline virtual void skale_getDBUsageI( const Json::Value& request, Json::Value& response ) {
        ( void ) request;
        response = this->skale_getDBUsage();
    }

    inline virtual void oracle_submitRequestI( const Json::Value& request, Json::Value& response ) {
        std::string strRequest = request[0u].asString();
        response = this->oracle_submitRequest( strRequest );
    }

    inline virtual void oracle_checkResultI( const Json::Value& request, Json::Value& response ) {
        std::string receipt = request[0u].asString();
        response = this->oracle_checkResult( receipt );
    }

    virtual std::string skale_protocolVersion() = 0;
    virtual std::string skale_receiveTransaction( std::string const& _rlp ) = 0;
    virtual std::string skale_shutdownInstance() = 0;
    virtual Json::Value skale_getSnapshot( const Json::Value& request ) = 0;
    virtual Json::Value skale_downloadSnapshotFragment( const Json::Value& request ) = 0;
    virtual Json::Value skale_getSnapshotSignature( unsigned blockNumber ) = 0;
    virtual std::string skale_getLatestSnapshotBlockNumber() = 0;
    virtual std::string skale_getLatestBlockNumber() = 0;
    virtual Json::Value skale_getDBUsage() = 0;
    virtual Json::Value skale_getGenesisState() = 0;
    virtual std::string oracle_submitRequest( std::string& request ) = 0;
    virtual std::string oracle_checkResult( std::string& receipt ) = 0;

public:
    SkaleFace() {
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_protocolVersion",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleFace::skale_protocolVersionI );

        this->bindAndAddMethod(
            jsonrpc::Procedure( "skale_receiveTransaction", jsonrpc::PARAMS_BY_POSITION,
                jsonrpc::JSON_STRING, "param1", jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleFace::skale_receiveTransactionI );

        this->bindAndAddMethod( jsonrpc::Procedure( "skale_shutdownInstance",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleFace::skale_shutdownInstanceI );

        this->bindAndAddMethod( jsonrpc::Procedure( "skale_getSnapshot",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleFace::skale_getSnapshotI );
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_downloadSnapshotFragment",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleFace::skale_downloadSnapshotFragmentI );
        this->bindAndAddMethod(
            jsonrpc::Procedure( "skale_getSnapshotSignature", jsonrpc::PARAMS_BY_POSITION,
                jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_INTEGER, NULL ),
            &dev::rpc::SkaleFace::skale_getSnapshotSignatureI );
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_getLatestSnapshotBlockNumber",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_INTEGER, NULL ),
            &dev::rpc::SkaleFace::skale_getLatestSnapshotBlockNumberI );
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_getLatestBlockNumber",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_INTEGER, NULL ),
            &dev::rpc::SkaleFace::skale_getLatestBlockNumberI );
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_getDBUsage", jsonrpc::PARAMS_BY_POSITION,
                                    jsonrpc::JSON_INTEGER, NULL ),
            &dev::rpc::SkaleFace::skale_getDBUsageI );
        this->bindAndAddMethod( jsonrpc::Procedure( "skale_getGenesisState",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_INTEGER, NULL ),
            &dev::rpc::SkaleFace::skale_getGenesisStateI );
        this->bindAndAddMethod( jsonrpc::Procedure( "oracle_submitRequest",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleFace::oracle_submitRequestI );
        this->bindAndAddMethod( jsonrpc::Procedure( "oracle_checkResult",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL ),
            &dev::rpc::SkaleFace::oracle_checkResultI );
    }
};

}  // namespace rpc
}  // namespace dev

#endif  // CPP_ETHEREUM_SKALEFACE_H
