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

#ifndef SKALE_HISTORICACCOUNT_H

#include <libethereum/Account.h>

namespace dev {

namespace eth {

// Historic account has a state trie and storage root


class HistoricAccount : public Account {
public:
    /// @returns the root of the trie (whose nodes are stored in the state db externally to this
    /// class) which encodes the base-state of the account's storage (upon which the storage is
    /// overlaid).
    StorageRoot originalStorageRoot() const {
        assert( m_storageRoot );
        return m_storageRoot;
    }

    /// Construct a dead Account.
    HistoricAccount() : Account(){};

    HistoricAccount( const HistoricAccount& _value ) : Account( _value ){};

    HistoricAccount( u256 _nonce, u256 _balance, Changedness _c = Changed )
        : Account( _nonce, _balance, _c ){};

    HistoricAccount& operator=( const HistoricAccount& other ) = default;

    /// Explicit constructor for wierd cases of construction or a contract account.
    HistoricAccount( u256 _nonce, u256 _balance, StorageRoot _storageRoot, h256 _codeHash,
        u256 const& _version, Changedness _c, s256 _storageUsed = 0 )
        : Account( _nonce, _balance, _storageRoot, _codeHash, _version, _c, _storageUsed ){};
};
}  // namespace eth
}  // namespace dev
#endif
