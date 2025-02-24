//
// Created by stan on 22-10-2022.
//

#ifndef SKALED_BASESTATE_H
#define SKALED_BASESTATE_H

#include <libdevcore/Address.h>

namespace skale {
enum class BaseState { PreExisting, Empty };
}

namespace dev {

namespace eth {

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

    Kind kind;        ///< The kind of the change.
    Address address;  ///< Changed account address.
    u256 value;       ///< Change value, e.g. balance, storage and nonce.
    u256 key;         ///< Storage key. Last because used only in one case.

    /// Helper constructor to make change log update more readable.
    Change( Kind _kind, Address const& _addr, u256 const& _value = 0 )
        : kind( _kind ), address( _addr ), value( _value ) {}

    /// Helper constructor especially for storage change log.
    Change( Address const& _addr, u256 const& _key, u256 const& _value )
        : kind( Storage ), address( _addr ), value( _value ), key( _key ) {}

    /// Helper constructor for nonce change log.
    Change( Address const& _addr, u256 const& _value )
        : kind( Nonce ), address( _addr ), value( _value ) {}
};

using ChangeLog = std::vector< Change >;

/**
 * Model of an Ethereum state, essentially a facade for the trie.
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

enum class CommitBehaviour { KeepEmptyAccounts, RemoveEmptyAccounts };

} // namespace eth

} // namespace dev

#endif  // SKALED_BASESTATE_H
