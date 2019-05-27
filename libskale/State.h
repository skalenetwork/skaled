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
 * @file State.h
 * @author Dmytro Stebaiev
 * @date 2018
 */

#pragma once

#include <array>
#include <unordered_map>

#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>

#include <libethcore/Exceptions.h>
#include <libethereum/Account.h>
#include <libethereum/Executive.h>
#include <libethereum/Transaction.h>
#include <libethereum/TransactionReceipt.h>

#include "OverlayDB.h"


namespace std {
template <>
struct hash< boost::filesystem::path > {
    size_t operator()( const boost::filesystem::path& p ) const {
        return boost::filesystem::hash_value( p );
    }
};
}  // namespace std

namespace dev {
namespace eth {
class SealEngineFace;
class Executive;
}  // namespace eth
}  // namespace dev

namespace skale {

namespace error {
// Import-specific errinfos
using errinfo_uncleIndex = boost::error_info< struct tag_uncleIndex, unsigned >;
using errinfo_currentNumber = boost::error_info< struct tag_currentNumber, dev::u256 >;
using errinfo_uncleNumber = boost::error_info< struct tag_uncleNumber, dev::u256 >;
using errinfo_unclesExcluded = boost::error_info< struct tag_unclesExcluded, dev::h256Hash >;
using errinfo_block = boost::error_info< struct tag_block, dev::bytes >;
using errinfo_now = boost::error_info< struct tag_now, unsigned >;

using errinfo_transactionIndex = boost::error_info< struct tag_transactionIndex, unsigned >;

using errinfo_vmtrace = boost::error_info< struct tag_vmtrace, std::string >;
using errinfo_receipts = boost::error_info< struct tag_receipts, std::vector< dev::bytes > >;
using errinfo_transaction = boost::error_info< struct tag_transaction, dev::bytes >;
using errinfo_phase = boost::error_info< struct tag_phase, unsigned >;
using errinfo_required_LogBloom =
    boost::error_info< struct tag_required_LogBloom, dev::eth::LogBloom >;
using errinfo_got_LogBloom = boost::error_info< struct tag_get_LogBloom, dev::eth::LogBloom >;
using LogBloomRequirementError = boost::tuple< errinfo_required_LogBloom, errinfo_got_LogBloom >;

DEV_SIMPLE_EXCEPTION( InvalidAccountStartNonceInState );
DEV_SIMPLE_EXCEPTION( IncorrectAccountStartNonceInState );
DEV_SIMPLE_EXCEPTION( AttemptToWriteToStateInThePast );
DEV_SIMPLE_EXCEPTION( AttemptToReadFromStateInThePast );
DEV_SIMPLE_EXCEPTION( AttemptToWriteToNotLockedStateObject );
}  // namespace error

enum class BaseState { PreExisting, Empty };

enum class Permanence {
    Reverted,
    Committed,
    Uncommitted,  ///< Uncommitted state for change log readings in tests.
    CommittedWithoutState
};

/// An atomic state changelog entry.
struct Change {
    enum Kind : int {
        /// Account balance changed. Change::value contains the amount the
        /// balance was increased by.
        Balance,

        /// Account storage was modified. Change::key contains the storage key,
        /// Change::value the storage value.
        Storage,

        /// Account storage root was modified.  Change::value contains the old
        /// account storage root.
        StorageRoot,

        /// Account nonce was changed.
        Nonce,

        /// Account was created (it was not existing before).
        Create,

        /// New code was added to an account (by "create" message execution).
        Code,

        /// Account was touched for the first time.
        Touch
    };

    Kind kind;             ///< The kind of the change.
    dev::Address address;  ///< Changed account address.
    dev::u256 value;       ///< Change value, e.g. balance, storage and nonce.
    dev::u256 key;         ///< Storage key. Last because used only in one case.
    dev::bytes oldCode;  ///< Code overwritten by CREATE, empty except in case of address collision.

    /// Helper constructor to make change log update more readable.
    Change( Kind _kind, dev::Address const& _addr, dev::u256 const& _value = 0 )
        : kind( _kind ), address( _addr ), value( _value ) {
        assert( _kind != Code );  // For this the special constructor needs to be used.
    }

    /// Helper constructor especially for storage change log.
    Change( dev::Address const& _addr, dev::u256 const& _key, dev::u256 const& _value )
        : kind( Storage ), address( _addr ), value( _value ), key( _key ) {}

    /// Helper constructor for nonce change log.
    Change( dev::Address const& _addr, dev::u256 const& _value )
        : kind( Nonce ), address( _addr ), value( _value ) {}

    /// Helper constructor especially for new code change log.
    Change( dev::Address const& _addr, dev::bytes const& _oldCode )
        : kind( Code ), address( _addr ), oldCode( _oldCode ) {}
};

using ChangeLog = std::vector< Change >;

/**
 * Model of an Skale state.
 *
 * Allows you to query the state of accounts as well as creating and modifying
 * accounts. It has built-in caching for various aspects of the state.
 *
 * # State Changelog
 *
 * Any atomic change to any account is registered and appended in the changelog.
 * In case some changes must be reverted, the changes are popped from the
 * changelog and undone. For possible atomic changes list @see Change::Kind.
 * The changelog is managed by savepoint(), rollback() and commit() methods.
 */
class State {
public:
    enum class CommitBehaviour { KeepEmptyAccounts, RemoveEmptyAccounts };

    using AddressMap = std::map< dev::h256, dev::Address >;

    /// Default constructor; creates with a blank database prepopulated with the genesis block.
    explicit State( dev::u256 const& _accountStartNonce )
        : State( _accountStartNonce, OverlayDB(), BaseState::Empty ) {}

    /// Basic state object from database.
    /// Use the default when you already have a database and you just want to make a State object
    /// which uses it. If you have no preexisting database then set BaseState to something other
    /// than BaseState::PreExisting in order to prepopulate the state.
    explicit State( dev::u256 const& _accountStartNonce, boost::filesystem::path const& _dbPath,
        dev::h256 const& _genesis, BaseState _bs = BaseState::PreExisting,
        dev::u256 _initialFunds = 0 )
        : State( _accountStartNonce,
              openDB( _dbPath, _genesis,
                  _bs == BaseState::PreExisting ? dev::WithExisting::Trust :
                                                  dev::WithExisting::Kill ),
              _bs, _initialFunds ) {}

    State() : State( dev::Invalid256, OverlayDB(), BaseState::Empty ) {}

    /// Copy state object.
    State( State const& _s );

    /// Copy state object.
    State& operator=( State const& _s );

    State( State&& ) = default;

    State& operator=( State&& ) = default;

    /// Populate the state from the given AccountMap. Just uses dev::eth::commit().
    void populateFrom( dev::eth::AccountMap const& _map );

    /// @returns the set containing all addresses currently in use in Ethereum.
    /// @warning This is slowslowslow. Don't use it unless you want to lock the object for seconds
    /// or minutes at a time.
    std::unordered_map< dev::Address, dev::u256 > addresses() const;

    /// @returns the map with maximum _maxResults elements containing hash->addresses and the next
    /// address hash.
    std::pair< AddressMap, dev::h256 > addresses(
        dev::h256 const& _begin, size_t _maxResults ) const;

    /// Check if the address is in use.
    bool addressInUse( dev::Address const& _address ) const;

    /// Check if the account exists in the state and is non empty (nonce > 0 || balance > 0 || code
    /// nonempty). These two notions are equivalent after EIP158.
    bool accountNonemptyAndExisting( dev::Address const& _address ) const;

    /// Check if the address contains executable code.
    bool addressHasCode( dev::Address const& _address ) const;

    /// Get an account's balance.
    /// @returns 0 if the address has never been used.
    dev::u256 balance( dev::Address const& _id ) const;

    /// Add some amount to balance.
    /// Will initialise the address if it has never been used.
    void addBalance( dev::Address const& _id, dev::u256 const& _amount );

    /// Subtract the @p _value amount from the balance of @p _addr account.
    /// @throws NotEnoughCash if the balance of the account is less than the
    /// amount to be subtrackted (also in case the account does not exist).
    void subBalance( dev::Address const& _addr, dev::u256 const& _value );

    /// Set the balance of @p _addr to @p _value.
    /// Will instantiate the address if it has never been used.
    void setBalance( dev::Address const& _addr, dev::u256 const& _value );

    /**
     * @brief Transfers "the balance @a _value between two accounts.
     * @param _from Account from which @a _value will be deducted.
     * @param _to Account to which @a _value will be added.
     * @param _value Amount to be transferred.
     */
    void transferBalance(
        dev::Address const& _from, dev::Address const& _to, dev::u256 const& _value ) {
        subBalance( _from, _value );
        addBalance( _to, _value );
    }

    /// Get the value of a storage position of an account.
    /// @returns 0 if no account exists at that address.
    dev::u256 storage( dev::Address const& _contract, dev::u256 const& _memory ) const;

    /// Set the value of a storage position of an account.
    void setStorage(
        dev::Address const& _contract, dev::u256 const& _location, dev::u256 const& _value );

    /// Get the original value of a storage position of an account (before modifications saved in
    /// account cache).
    /// @returns 0 if no account exists at that address.
    dev::u256 originalStorageValue( dev::Address const& _contract, dev::u256 const& _key ) const;

    /// Clear the storage root hash of an account to the hash of the empty trie.
    void clearStorage( dev::Address const& _contract );

    /// Create a contract at the given address (with unset code and unchanged balance).
    void createContract( dev::Address const& _address );

    /// Sets the code of the account. Must only be called during / after contract creation.
    void setCode( dev::Address const& _address, dev::bytes&& _code );

    /// Sets the code of the account.
    void setCode( dev::Address const& _address, const dev::bytes& _code );

    /// Delete an account (used for processing suicides).
    void kill( dev::Address _a );

    /// Get the storage of an account.
    /// @note This is expensive. Don't use it unless you need to.
    /// @returns map of hashed keys to key-value pairs or empty map if no account exists at that
    /// address.
    std::map< dev::h256, std::pair< dev::u256, dev::u256 > > storage(
        dev::Address const& _contract ) const;

    /// Get the code of an account.
    /// @returns bytes() if no account exists at that address.
    /// @warning The reference to the code is only valid until the access to
    ///          other account. Do not keep it.
    dev::bytes const& code( dev::Address const& _addr ) const;

    /// Get the code hash of an account.
    /// @returns EmptySHA3 if no account exists at that address or if there is no code associated
    /// with the address.
    dev::h256 codeHash( dev::Address const& _contract ) const;

    /// Get the byte-size of the code of an account.
    /// @returns code(_contract).size(), but utilizes CodeSizeHash.
    size_t codeSize( dev::Address const& _contract ) const;

    /// Increament the account nonce.
    void incNonce( dev::Address const& _id );

    /// Set the account nonce.
    void setNonce( dev::Address const& _addr, dev::u256 const& _newNonce );

    /// Get the account nonce -- the number of transactions it has sent.
    /// @returns 0 if the address has never been used.
    dev::u256 getNonce( dev::Address const& _addr ) const;

    /// Commit all changes waiting in the address cache to the DB.
    /// @param _commitBehaviour whether or not to remove empty accounts during commit.
    void commit( CommitBehaviour _commitBehaviour = CommitBehaviour::RemoveEmptyAccounts );

    /// Execute a given transaction.
    /// This will change the state accordingly.
    std::pair< dev::eth::ExecutionResult, dev::eth::TransactionReceipt > execute(
        dev::eth::EnvInfo const& _envInfo, dev::eth::SealEngineFace const& _sealEngine,
        dev::eth::Transaction const& _t, Permanence _p = Permanence::Committed,
        dev::eth::OnOpFunc const& _onOp = dev::eth::OnOpFunc() );

    /// Get the account start nonce. May be required.
    dev::u256 const& accountStartNonce() const { return m_accountStartNonce; }
    dev::u256 const& requireAccountStartNonce() const;
    void noteAccountStartNonce( dev::u256 const& _actual );

    /// Create a savepoint in the state changelog.	///
    /// @return The savepoint index that can be used in rollback() function.
    size_t savepoint() const;

    /// Revert all recent changes up to the given @p _savepoint savepoint.
    void rollback( size_t _savepoint );

    ChangeLog const& changeLog() const { return m_changeLog; }

    void updateToLatestVersion();

    /// Create State copy to get access to data.
    /// Different copies can be safely used in different threads
    /// but single object is not thread safe.
    /// No one can change state while returned object exists.
    State startRead() const;

    /// Create State copy to modify data.
    State startWrite() const;

    /// Create State copy to modify data and pass writing lock to it
    State delegateWrite();

    void stopWrite();

    /**
     * @brief clearAll removes all data from database
     */
    void clearAll();

    /**
     * @brief connected returns true if state is connected to database
     */
    bool connected() const;

    /// Check if state is empty
    bool empty() const;

private:
    explicit State( dev::u256 const& _accountStartNonce, OverlayDB const& _db,
        BaseState _bs = BaseState::PreExisting, dev::u256 _initialFunds = 0 );

    /// Open a DB - useful for passing into the constructor & keeping for other states that are
    /// necessary.
    static OverlayDB openDB( boost::filesystem::path const& _path, dev::h256 const& _genesisHash,
        dev::WithExisting _we = dev::WithExisting::Trust );

    /// Turns all "touched" empty accounts into non-alive accounts.
    void removeEmptyAccounts();

    /// @returns the account at the given address or a null pointer if it does not exist.
    /// The pointer is valid until the next access to the state or account.
    dev::eth::Account const* account( dev::Address const& _addr ) const;

    /// @returns the account at the given address or a null pointer if it does not exist.
    /// The pointer is valid until the next access to the state or account.
    dev::eth::Account* account( dev::Address const& _addr );

    /// Purges non-modified entries in m_cache if it grows too large.
    void clearCacheIfTooLarge() const;

    void createAccount( dev::Address const& _address, dev::eth::Account const&& _account );

    /// @returns true when normally halted; false when exceptionally halted; throws when internal VM
    /// exception occurred.
    bool executeTransaction(
        dev::eth::Executive& _e, dev::eth::Transaction const& _t, dev::eth::OnOpFunc const& _onOp );

    bool checkVersion() const;

    enum Auxiliary { CODE = 1 };

    boost::optional< boost::shared_lock< boost::shared_mutex > > m_db_read_lock;
    boost::optional< boost::upgrade_lock< boost::shared_mutex > > m_db_write_lock;

    std::shared_ptr< boost::shared_mutex > x_db_ptr;
    std::shared_ptr< OverlayDB > m_db_ptr;  ///< Our overlay for the state.
    std::shared_ptr< size_t > m_storedVersion;
    size_t m_currentVersion;
    mutable std::unordered_map< dev::Address, dev::eth::Account > m_cache;  ///< Our address cache.
                                                                            ///< This stores the
                                                                            ///< states of each
                                                                            ///< address that has
                                                                            ///< (or at least might
                                                                            ///< have) been changed.
    mutable std::vector< dev::Address > m_unchangedCacheEntries;  ///< Tracks entries in m_cache
                                                                  ///< that can potentially be
                                                                  ///< purged if it grows too large.
    mutable std::set< dev::Address > m_nonExistingAccountsCache;  ///< Tracks addresses that are
                                                                  ///< known to not exist.
    dev::u256 m_accountStartNonce;

    friend std::ostream& operator<<( std::ostream& _out, State const& _s );
    ChangeLog m_changeLog;

    dev::u256 m_initial_funds = 0;
};

std::ostream& operator<<( std::ostream& _out, State const& _s );

}  // namespace skale
