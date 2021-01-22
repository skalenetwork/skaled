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
 * @file SkaleClient.h
 * @author Dima Litvinov
 * @date 2019
 */

#ifndef CPP_ETHEREUM_SKALECLIENT_H
#define CPP_ETHEREUM_SKALECLIENT_H

#include <jsonrpccpp/client.h>
#include <iostream>

class SkaleClient : public jsonrpc::Client {
public:
    SkaleClient( jsonrpc::IClientConnector& conn,
        jsonrpc::clientVersion_t type = jsonrpc::JSONRPC_CLIENT_V2 );

    std::string skale_protocolVersion() noexcept( false );

    std::string skale_receiveTransaction( std::string const& _rlp ) noexcept( false );

    std::string skale_shutdownInstance() noexcept( false );

    Json::Value skale_getSnapshotSignature( unsigned blockNumber ) noexcept( false );

    Json::Value skale_imaInfo() noexcept( false );

    unsigned skale_getLatestSnapshotBlockNumber() noexcept( false );
};

#endif  // CPP_ETHEREUM_SKALECLIENT_H
