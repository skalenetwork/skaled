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
 * @file OverlayDB.h
 * @author Dmytro Stebaiev
 * @date 2018
 */

#pragma once

#include <functional>
#include <memory>

#include <libbatched-io/batched_db.h>
#include <libdevcore/Address.h>
#include <libdevcore/Common.h>
#include <libdevcore/Log.h>
//#include <libethereum/Account.h>

namespace dev {
namespace eth {
class Account;
class TransactionReceipt;
}  // namespace eth
}  // namespace dev

namespace skale {

namespace slicing {

dev::db::Slice toSlice( dev::h256 const& _h );
dev::db::Slice toSlice( dev::bytes const& _b );
dev::db::Slice toSlice( dev::h160 const& _h );
dev::db::Slice toSlice( std::string const& _s );

};  // namespace slicing

class OverlayDB {
public:
    explicit OverlayDB( std::unique_ptr< batched_io::db_face > _db_face = nullptr );

    virtual ~OverlayDB() = default;

    // Copyable
    OverlayDB( OverlayDB const& ) = default;
    OverlayDB& operator=( OverlayDB const& ) = default;
    // Movable
    OverlayDB( OverlayDB&& ) = default;
    OverlayDB& operator=( OverlayDB&& ) = default;

    dev::h256 getLastExecutedTransactionHash() const;
    dev::bytes getPartialTransactionReceipts() const;
    void setLastExecutedTransactionHash( const dev::h256& );
    void setPartialTransactionReceipts( const dev::bytes& );

    void addReceiptToPartials( const dev::eth::TransactionReceipt& );
    void clearPartialTransactionReceipts();

    // commit key-value pairs in storage
    void commitStorageValues();
    void commit( const std::string& _debugCommitId, bool _isHistoricState = false );
    void rollback();
    void clearDB();
    bool connected() const;
    bool empty() const;

    std::string lookup( dev::h160 const& _address ) const;
    bool exists( dev::h160 const& _address ) const;
    void kill( dev::h160 const& _address );
    void insert( dev::h160 const& _address, dev::bytesConstRef _value );

    dev::h256 lookup( dev::h160 const& _address, dev::h256 const& _storageAddress ) const;
    bool exists( dev::h160 const& _address, dev::h256 const& _storageAddress ) const;
    void kill( dev::h160 const& _address, dev::h256 const& _storageAddress );
    void insert(
        dev::h160 const& _address, dev::h256 const& _storageAddress, dev::h256 const& _value );

    std::string lookupAuxiliary( dev::h160 const& _address, _byte_ space = 0xFF ) const;
    bool existsAuxiliary( dev::h160 const& _address, _byte_ space = 0xFF ) const;
    void killAuxiliary( dev::h160 const& _address, _byte_ space = 0xFF );
    void insertAuxiliary(
        dev::h160 const& _address, dev::bytesConstRef _value, _byte_ space = 0xFF );

    dev::s256 storageUsed() const;
    void updateStorageUsage( dev::s256 const& _storageUsed );

    /// @returns the set containing all accounts currently in use in Ethereum.
    /// @warning This is slowslowslow. Don't use it unless you want to lock the object for seconds
    /// or minutes at a time.
    std::unordered_map< dev::h160, std::string > accounts() const;

    std::unordered_map< dev::u256, dev::u256 > storage( dev::h160 const& address ) const;

    // block for HistoricState
    void insert( dev::h256 const& _h, dev::bytesConstRef _v );

    std::string lookup( dev::h256 const& _h ) const { return lookup( _h, UINT64_MAX ); }
    std::string lookup( dev::h256 const& _h, uint64_t _rootBlockTimestamp ) const;

    bool exists( dev::h256 const& _h ) const;

    void kill( dev::h256 const& _h );

    dev::bytes lookupAux( dev::h256 const& _h ) const;
    void removeAux( dev::h256 const& _h );
    void insertAux( dev::h256 const& _h, dev::bytesConstRef _v );

    void setCommitOnEveryInsert( bool _value, uint64_t _timestamp ) {
        commit( std::to_string( _timestamp ) );
        m_commitOnEveryInsert = _value;
    }

private:
    std::unordered_map< dev::h160, dev::bytes > m_cache;
    std::unordered_map< dev::h160, std::unordered_map< _byte_, dev::bytes > > m_auxiliaryCache;
    std::unordered_map< dev::h160, std::unordered_map< dev::h256, dev::h256 > > m_storageCache;
    dev::s256 storageUsed_ = 0;

    // for HistoricState
    std::unordered_map< dev::h256, std::pair< std::string, unsigned > > m_historicMain;
    std::unordered_map< dev::h256, std::pair< dev::bytes, bool > > m_historicAux;

    std::shared_ptr< batched_io::db_face > m_db_face;

    dev::bytes getAuxiliaryKey( dev::h160 const& _address, _byte_ space ) const;
    dev::bytes getStorageKey( dev::h160 const& _address, dev::h256 const& _storageAddress ) const;

    // a flag to commit to disk on every insert to save memory
    // this is currently only used for historic state conversion
    bool m_commitOnEveryInsert = false;

    mutable std::optional< dev::h256 > lastExecutedTransactionHash;
    mutable std::optional< dev::bytes > lastExecutedTransactionReceipts;

public:
    std::shared_ptr< batched_io::db_face > db() { return m_db_face; }
    void copyStorageIntoAccountMap(
        std::unordered_map< dev::Address, dev::eth::Account >& _map ) const;
};

}  // namespace skale
