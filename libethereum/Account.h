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
/** @file Account.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include "libethcore/Counter.h"
#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcore/TrieDB.h>
#include <libethcore/Common.h>

namespace dev {
class OverlayDB;
namespace eth {

// introduce new classes StorageRoot and GlobalRoot to better separate storage and
// global roots through the program


class StorageRoot : public h256 {
public:
    StorageRoot( const u256& _value ) : h256( _value ) {}

    StorageRoot( const StorageRoot& _value ) : h256( _value ){};
};

class GlobalRoot : public h256 {
public:
    GlobalRoot( const u256& _value ) : h256( _value ) {}

    GlobalRoot( const GlobalRoot& _value ) : h256( _value ){};
};


/**
 * Models the state of a single Ethereum account.
 * Used to cache a portion of the full Ethereum state. State keeps a mapping of Address's to
 * Accounts.
 *
 * Aside from storing the nonce and balance, the account may also be "dead" (where isAlive() returns
 * false). This allows State to explicitly store the notion of a deleted account in it's cache.
 * kill() can be used for this.
 *
 * For the account's storage, the class operates a cache. originalStorageRoot() specifies the base
 * state of the storage given as the Trie root to be looked up in the state database. Alterations
 * beyond this base are specified in the overlay, stored in this class and retrieved with
 * storageOverlay(). setStorage allows the overlay to be altered.
 *
 * The code handling explicitly supports a two-stage commit model needed for contract-creation. When
 * creating a contract (running the initialisation code), the code of the account is considered
 * empty. The attribute of emptiness can be retrieved with codeBearing(). After initialisation one
 * must set the code accordingly; the code of the Account can be set with setCode(). To validate a
 * setCode() call, this class records the state of being in contract-creation (and thus in a state
 * where setCode may validly be called). It can be determined through isFreshCode().
 *
 * The code can be retrieved through code(), and its hash through codeHash(). codeHash() is only
 * valid when the account is not in the contract-creation phase (i.e. when isFreshCode() returns
 * false). This class supports populating code on-demand from the state database. To determine if
 * the code has been prepopulated call codeCacheValid(). To populate the code, look it up with
 * codeHash() and populate with noteCode().
 *
 * @todo: need to make a noteCodeCommitted().
 *
 * The constructor allows you to create an one of a number of "types" of accounts. The default
 * constructor makes a dead account (this is ignored by State when writing out the Trie). Another
 * three allow a basic or contract account to be specified along with an initial balance. The fina
 * two allow either a basic or a contract account to be created with arbitrary values.
 */
class Account {
public:
    /// Changedness of account to create.
    enum Changedness {
        /// Account starts as though it has been changed.
        Changed,
        /// Account starts as though it has not been changed.
        Unchanged
    };

    /// Construct a dead Account.
    Account() {}

    /// Construct an alive Account, with given endowment, for either a normal (non-contract) account
    /// or for a contract account in the conception phase, where the code is not yet known.
    Account( u256 _nonce, u256 _balance, Changedness _c = Changed )
        : m_isAlive( true ),
          m_isUnchanged( _c == Unchanged ),
          m_nonce( _nonce ),
          m_balance( _balance ) {}

    /// Explicit constructor for wierd cases of construction or a contract account.
    Account( u256 _nonce, u256 _balance, StorageRoot _contractRoot, h256 _codeHash,
        u256 const& _version, Changedness _c, s256 _storageUsed = 0 )
        : m_isAlive( true ),
          m_isUnchanged( _c == Unchanged ),
          m_nonce( _nonce ),
          m_balance( _balance ),
          m_codeHash( _codeHash ),
          m_version( _version ),
          m_storageUsed( _storageUsed ),
          m_storageRoot( _contractRoot ) {
        assert( _contractRoot );
    }


    /// Kill this account. Useful for the suicide opcode. Following this call, isAlive() returns
    /// false.
    void kill() {
        m_isAlive = false;
        m_storageOverlay.clear();
        m_codeHash = EmptySHA3;
        m_storageRoot = StorageRoot( EmptyTrie );
        m_balance = 0;
        m_nonce = 0;
        m_version = 0;
        changed();
    }

    /// @returns true iff this object represents an account in the state. Returns false if this
    /// object represents an account that should no longer exist in the trie (an account that never
    /// existed or was suicided).
    bool isAlive() const { return m_isAlive; }

    /// @returns true if the account is unchanged from creation.
    bool isDirty() const { return !m_isUnchanged; }

    void untouch() { m_isUnchanged = true; }

    /// @returns true if the nonce, balance and code is zero / empty. Code is considered empty
    /// during creation phase.
    bool isEmpty() const { return nonce() == 0 && balance() == 0 && codeHash() == EmptySHA3; }

    /// @returns the balance of this account.
    u256 const& balance() const { return m_balance; }

    /// Increments the balance of this account by the given amount.
    void addBalance( u256 _value ) {
        m_balance += _value;
        changed();
    }

    /// @returns the nonce of the account.
    u256 nonce() const { return m_nonce; }

    /// Increment the nonce of the account by one.
    void incNonce() {
        ++m_nonce;
        changed();
    }

    /// Set nonce to a new value. This is used when reverting changes made to
    /// the account.
    void setNonce( u256 const& _nonce ) {
        m_nonce = _nonce;
        changed();
    }


    /// @returns account's original storage
    /// not taking into account overlayed modifications
    std::unordered_map< u256, u256 > const& originalStorageCache() const;

    /// @returns the storage overlay as a simple hash map.
    std::unordered_map< u256, u256 > const& storageOverlay() const { return m_storageOverlay; }

    /// Set a key/value pair in the account's storage. This actually goes into the overlay, for
    /// committing to the trie later.
    void setStorage( u256 _p, u256 _v ) {
        m_storageOverlay[_p] = _v;
        changed();
    }

    /// Empty the storage.  Used when a contract is overwritten.
    void clearStorage() {
        m_storageOverlay.clear();
        m_storageRoot = StorageRoot( EmptyTrie );
        changed();
    }


    /// Set a key/value pair in the account's storage to a value that is already present inside the
    /// database.
    void setStorageCache( u256 _p, u256 _v ) const {
        const_cast< decltype( m_storageOriginal )& >( m_storageOriginal )[_p] = _v;
    }

    /// Set the storage root.  Used when clearStorage() is reverted.
    void setStorageRoot( StorageRoot const& _root ) {
        m_storageOverlay.clear();
        m_storageRoot = _root;
        changed();
    }

    /// @returns the hash of the account's code.
    h256 codeHash() const { return m_codeHash; }

    bool hasNewCode() const { return m_hasNewCode; }

    /// Sets the code of the account. Used by "create" messages.
    void setCode( bytes&& _code, dev::u256 const& version );

    /// Reset the code set by previous setCode
    void resetCode();

    /// Specify to the object what the actual code is for the account. @a _code must have a SHA3
    /// equal to codeHash() and must only be called when isFreshCode() returns false.
    void noteCode( bytesConstRef _code ) {
        assert( sha3( _code ) == m_codeHash );
        m_codeCache = _code.toBytes();
    }

    /// @returns the account's code.
    bytes const& code() const { return m_codeCache; }

    u256 version() const { return m_version; }

    s256 const& storageUsed() const { return m_storageUsed; }

    void updateStorageUsage( const s256& _value ) {
        m_storageUsed += _value;
        changed();
    }

    /// @returns account's original storage value corresponding to the @_key
    /// not taking into account overlayed modifications
    u256 originalStorageValue( u256 const& _key, OverlayDB const& _db ) const;


    /// @returns account's storage value corresponding to the @_key
    /// taking into account overlayed modifications
    u256 storageValue( u256 const& _key, OverlayDB const& _db ) const {
        auto mit = m_storageOverlay.find( _key );
        if ( mit != m_storageOverlay.end() )
            return mit->second;

        return originalStorageValue( _key, _db );
    }


    /// Note that we've altered the account.
    void changed() { m_isUnchanged = false; }

private:
    /// Is this account existant? If not, it represents a deleted account.
    bool m_isAlive = false;

    /// True if we've not made any alteration to the account having been given it's properties
    /// directly.
    bool m_isUnchanged = false;

    /// True if new code was deployed to the account
    bool m_hasNewCode = false;

    /// Account's nonce.
    u256 m_nonce;

    /// Account's balance.
    u256 m_balance = 0;

    /** If c_contractConceptionCodeHash then we're in the limbo where we're running the
     * initialisation code. We expect a setCode() at some point later. If EmptySHA3, then m_code,
     * which should be empty, is valid. If anything else, then m_code is valid iff it's not empty,
     * otherwise, State::ensureCached() needs to be called with the correct args.
     */
    h256 m_codeHash = EmptySHA3;

    /// Account's version
    u256 m_version = 0;

    /// The map with is overlaid onto whatever storage is implied by the m_storageRoot in the trie.
    std::unordered_map< u256, u256 > m_storageOverlay;

    /// The cache of unmodifed storage items
    mutable std::unordered_map< u256, u256 > m_storageOriginal;

    /// The associated code for this account. The SHA3 of this should be equal to m_codeHash unless
    /// m_codeHash equals c_contractConceptionCodeHash.
    bytes m_codeCache;

    /// Value for m_codeHash when this account is having its code determined.
    static const h256 c_contractConceptionCodeHash;

    // is only valuable if account has code
    s256 m_storageUsed = 0;

    Counter< Account > c;

protected:
    /// The base storage root. Used with the state DB to give a base to the storage.
    /// m_storageOverlay is overlaid on this and takes precedence for all values set.
    StorageRoot m_storageRoot = StorageRoot( EmptyTrie );


public:
    static uint64_t howMany() { return Counter< Account >::howMany(); }
};

class AccountMask {
public:
    AccountMask( bool _all = false )
        : m_hasBalance( _all ), m_hasNonce( _all ), m_hasCode( _all ), m_hasStorage( _all ) {}

    AccountMask( bool _hasBalance, bool _hasNonce, bool _hasCode, bool _hasStorage,
        bool _shouldNotExist = false )
        : m_hasBalance( _hasBalance ),
          m_hasNonce( _hasNonce ),
          m_hasCode( _hasCode ),
          m_hasStorage( _hasStorage ),
          m_shouldNotExist( _shouldNotExist ) {}

    bool allSet() const { return m_hasBalance && m_hasNonce && m_hasCode && m_hasStorage; }
    bool hasBalance() const { return m_hasBalance; }
    bool hasNonce() const { return m_hasNonce; }
    bool hasCode() const { return m_hasCode; }
    bool hasStorage() const { return m_hasStorage; }
    bool shouldExist() const { return !m_shouldNotExist; }

private:
    bool m_hasBalance;
    bool m_hasNonce;
    bool m_hasCode;
    bool m_hasStorage;
    bool m_shouldNotExist = false;
};

using AccountMap = std::unordered_map< Address, Account >;
using AccountMaskMap = std::unordered_map< Address, AccountMask >;

class PrecompiledContract;
using PrecompiledContractMap = std::unordered_map< Address, PrecompiledContract >;

AccountMap jsonToAccountMap( std::string const& _json, u256 const& _defaultNonce = 0,
    AccountMaskMap* o_mask = nullptr, PrecompiledContractMap* o_precompiled = nullptr,
    const boost::filesystem::path& _configPath = {} );
}  // namespace eth
}  // namespace dev
