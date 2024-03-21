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
    ChainParams( ChainParams const& /*_org*/ ) = default;
    ChainParams( std::string const& _s );
    ChainParams( bytes const& _genesisRLP, AccountMap const& _state ) {
        populateFromGenesis( _genesisRLP, _state );
    }
    ChainParams( std::string const& _json, bytes const& _genesisRLP, AccountMap const& _state )
        : ChainParams( _json ) {
        populateFromGenesis( _genesisRLP, _state );
    }

    SealEngineFace* createSealEngine();

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

    /// Genesis block info.
    bytes genesisBlock() const;

    /// load config
    ChainParams loadConfig(
        std::string const& _json, const boost::filesystem::path& _configPath = {} ) const;

    const std::string& getOriginalJson() const;
    void resetJson() { originalJSON = ""; }

    bool checkAdminOriginAllowed( const std::string& origin ) const;
    static void processSkaleConfigItems( ChainParams& _cp, json_spirit::mObject& _obj );

private:
    void populateFromGenesis( bytes const& _genesisRLP, AccountMap const& _state );

    /// load genesis
    ChainParams loadGenesis( std::string const& _json ) const;

    mutable std::string originalJSON;
};

}  // namespace dev::eth
