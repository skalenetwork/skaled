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

#pragma once

#include <array>
#include <unordered_map>

#include <libdevcore/Common.h>
#include <libdevcore/OverlayDB.h>
#include <libdevcore/RLP.h>
#include <libdevcore/TrieDB.h>
#include <libethcore/BlockHeader.h>
#include <libethcore/Exceptions.h>
#include <libethereum/CodeSizeCache.h>
#include <libevm/ExtVMFace.h>
#include <libskale/State.h>

#include "Account.h"
#include "GasPricer.h"
#include "Transaction.h"
#include "TransactionReceipt.h"


namespace dev {
namespace test {
class ImportTest;
class StateLoader;
}  // namespace test

namespace eth {
// Import-specific errinfos
using errinfo_uncleIndex = boost::error_info< struct tag_uncleIndex, unsigned >;
using errinfo_currentNumber = boost::error_info< struct tag_currentNumber, u256 >;
using errinfo_uncleNumber = boost::error_info< struct tag_uncleNumber, u256 >;
using errinfo_unclesExcluded = boost::error_info< struct tag_unclesExcluded, h256Hash >;
using errinfo_block = boost::error_info< struct tag_block, bytes >;
using errinfo_now = boost::error_info< struct tag_now, unsigned >;

using errinfo_transactionIndex = boost::error_info< struct tag_transactionIndex, unsigned >;

using errinfo_vmtrace = boost::error_info< struct tag_vmtrace, std::string >;
using errinfo_receipts = boost::error_info< struct tag_receipts, std::vector< bytes > >;
using errinfo_transaction = boost::error_info< struct tag_transaction, bytes >;
using errinfo_phase = boost::error_info< struct tag_phase, unsigned >;
using errinfo_required_LogBloom = boost::error_info< struct tag_required_LogBloom, LogBloom >;
using errinfo_got_LogBloom = boost::error_info< struct tag_get_LogBloom, LogBloom >;
using LogBloomRequirementError = boost::tuple< errinfo_required_LogBloom, errinfo_got_LogBloom >;

class BlockChain;
class State;
class TransactionQueue;
struct VerifiedBlockRef;

enum class BaseState { PreExisting, Empty };

enum class Permanence {
    Reverted,
    Committed,
    Uncommitted,  ///< Uncommitted state for change log readings in tests.
    CommittedWithoutState
};

#if ETH_FATDB
template < class KeyType, class DB >
using SecureTrieDB = SpecificTrieDB< FatGenericTrieDB< DB >, KeyType >;
#else
template < class KeyType, class DB >
using SecureTrieDB = SpecificTrieDB< HashedGenericTrieDB< DB >, KeyType >;
#endif

DEV_SIMPLE_EXCEPTION( InvalidAccountStartNonceInState );
DEV_SIMPLE_EXCEPTION( IncorrectAccountStartNonceInState );
DEV_SIMPLE_EXCEPTION( AttemptToWriteToStateInThePast );
DEV_SIMPLE_EXCEPTION( AttemptToReadFromStateInThePast );
DEV_SIMPLE_EXCEPTION( AttemptToWriteToNotLockedStateObject );

class SealEngineFace;
class Executive;

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
    bytes oldCode;    ///< Code overwritten by CREATE, empty except in case of address collision.

    /// Helper constructor to make change log update more readable.
    Change( Kind _kind, Address const& _addr, u256 const& _value = 0 )
        : kind( _kind ), address( _addr ), value( _value ) {
        assert( _kind != Code );  // For this the special constructor needs to be used.
    }

    /// Helper constructor especially for storage change log.
    Change( Address const& _addr, u256 const& _key, u256 const& _value )
        : kind( Storage ), address( _addr ), value( _value ), key( _key ) {}

    /// Helper constructor for nonce change log.
    Change( Address const& _addr, u256 const& _value )
        : kind( Nonce ), address( _addr ), value( _value ) {}

    /// Helper constructor especially for new code change log.
    Change( Address const& _addr, bytes const& _oldCode )
        : kind( Code ), address( _addr ), oldCode( _oldCode ) {}
};

using ChangeLog = std::vector< Change >;

State& createIntermediateState(
    State& o_s, Block const& _block, unsigned _txIndex, BlockChain const& _bc );

template < class DB >
AddressHash commit( AccountMap const& _cache, SecureTrieDB< Address, DB >& _state );

}  // namespace eth
}  // namespace dev
