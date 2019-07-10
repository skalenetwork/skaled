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
/** @file ClientBase.h
 * @author Gav Wood <i@gavwood.com>
 * @author Marek Kotewicz <marek@ethdev.com>
 * @date 2015
 */

#pragma once

#include "Block.h"
#include "CommonNet.h"
#include "Interface.h"
#include "LogFilter.h"
#include "TransactionQueue.h"
#include <chrono>

namespace dev {
namespace eth {
struct InstalledFilter {
    InstalledFilter( LogFilter const& _f ) : filter( _f ) {}

    LogFilter filter;
    unsigned refCount = 1;
    LocalisedLogEntries changes_;
};

static const h256 PendingChangedFilter = u256( 0 );
static const h256 ChainChangedFilter = u256( 1 );

static const LogEntry SpecialLogEntry = LogEntry( Address(), h256s(), bytes() );
static const LocalisedLogEntry InitialChange( SpecialLogEntry );

struct ClientWatch;

struct ClientWatch {
    ClientWatch();
    explicit ClientWatch(
        h256 _id, Reaping _r, fnClientWatchHandlerMulti_t fnOnNewChanges, unsigned iw = 0 );

    h256 id;
    unsigned iw_ = 0;

private:
    void init( h256 _id, Reaping _r, fnClientWatchHandlerMulti_t fnOnNewChanges );
    fnClientWatchHandlerMulti_t fnOnNewChanges_;
#if INITIAL_STATE_AS_CHANGES
    LocalisedLogEntries changes_ = LocalisedLogEntries{InitialChange};
#else
    LocalisedLogEntries changes_;
#endif
public:
    LocalisedLogEntries get_changes() const;
    void swap_changes( LocalisedLogEntries& otherChanges );
    void append_changes( const LocalisedLogEntries& otherChanges );
    void append_changes( const LocalisedLogEntry& entry );

    mutable std::chrono::system_clock::time_point lastPoll = std::chrono::system_clock::now();
};

class ClientBase : public Interface {
public:
    class CreationException : public std::exception {
        virtual const char* what() const noexcept { return "Error creating Client"; }
    };

    ClientBase() {}
    virtual ~ClientBase() {}

    /// Estimate gas usage for call/create.
    /// @param _maxGas An upper bound value for estimation, if not provided default value of
    /// c_maxGasEstimate will be used.
    /// @param _callback Optional callback function for progress reporting
    std::pair< u256, ExecutionResult > estimateGas( Address const& _from, u256 _value,
        Address _dest, bytes const& _data, int64_t _maxGas, u256 _gasPrice,
        GasEstimationCallback const& _callback ) override;

    u256 balanceAt( Address _a ) const override;
    u256 countAt( Address _a ) const override;
    u256 stateAt( Address _a, u256 _l ) const override;
    bytes codeAt( Address _a ) const override;
    h256 codeHashAt( Address _a ) const override;
    std::map< h256, std::pair< u256, u256 > > storageAt( Address _a ) const override;

    LocalisedLogEntries logs( unsigned _watchId ) const override;
    LocalisedLogEntries logs( LogFilter const& _filter ) const override;
    virtual void prependLogsFromBlock( LogFilter const& _filter, h256 const& _blockHash,
        BlockPolarity _polarity, LocalisedLogEntries& io_logs ) const;

    /// Install, uninstall and query watches.
    unsigned installWatch( LogFilter const& _filter, Reaping _r = Reaping::Automatic,
        fnClientWatchHandlerMulti_t fnOnNewChanges = fnClientWatchHandlerMulti_t() ) override;
    unsigned installWatch( h256 _filterId, Reaping _r = Reaping::Automatic,
        fnClientWatchHandlerMulti_t fnOnNewChanges = fnClientWatchHandlerMulti_t() ) override;
    bool uninstallWatch( unsigned _watchId ) override;
    LocalisedLogEntries peekWatch( unsigned _watchId ) const override;
    LocalisedLogEntries checkWatch( unsigned _watchId ) override;

    h256 hashFromNumber( BlockNumber _number ) const override;
    BlockNumber numberFromHash( h256 _blockHash ) const override;
    int compareBlockHashes( h256 _h1, h256 _h2 ) const override;
    BlockHeader blockInfo( h256 _hash ) const override;
    BlockDetails blockDetails( h256 _hash ) const override;
    Transaction transaction( h256 _transactionHash ) const override;
    LocalisedTransaction localisedTransaction( h256 const& _transactionHash ) const override;
    Transaction transaction( h256 _blockHash, unsigned _i ) const override;
    LocalisedTransaction localisedTransaction( h256 const& _blockHash, unsigned _i ) const override;
    TransactionReceipt transactionReceipt( h256 const& _transactionHash ) const override;
    LocalisedTransactionReceipt localisedTransactionReceipt(
        h256 const& _transactionHash ) const override;
    std::pair< h256, unsigned > transactionLocation( h256 const& _transactionHash ) const override;
    Transactions transactions( h256 _blockHash ) const override;
    Transactions transactions( BlockNumber _block ) const override {
        if ( _block == PendingBlock )
            return postSeal().pending();
        return transactions( hashFromNumber( _block ) );
    }
    TransactionHashes transactionHashes( h256 _blockHash ) const override;
    BlockHeader uncle( h256 _blockHash, unsigned _i ) const override;
    UncleHashes uncleHashes( h256 _blockHash ) const override;
    unsigned transactionCount( h256 _blockHash ) const override;
    unsigned transactionCount( BlockNumber _block ) const override {
        if ( _block == PendingBlock ) {
            auto p = postSeal().pending();
            return p.size();
        }
        return transactionCount( hashFromNumber( _block ) );
    }
    unsigned uncleCount( h256 _blockHash ) const override;
    unsigned number() const override;
    h256s pendingHashes() const override;
    BlockHeader pendingInfo() const override;
    BlockDetails pendingDetails() const override;

    EVMSchedule evmSchedule() const override {
        return sealEngine()->evmSchedule( pendingInfo().number() );
    }

    ImportResult injectBlock( bytes const& _block ) override;

    u256 gasLimitRemaining() const override;
    u256 gasBidPrice() const override { return DefaultGasPrice; }

    /// Get the block author
    Address author() const override;

    bool isKnown( h256 const& _hash ) const override;
    bool isKnown( BlockNumber _block ) const override;
    bool isKnownTransaction( h256 const& _transactionHash ) const override;
    bool isKnownTransaction( h256 const& _blockHash, unsigned _i ) const override;

    void startSealing() override {
        BOOST_THROW_EXCEPTION(
            InterfaceNotSupported() << errinfo_interface( "ClientBase::startSealing" ) );
    }
    void stopSealing() override {
        BOOST_THROW_EXCEPTION(
            InterfaceNotSupported() << errinfo_interface( "ClientBase::stopSealing" ) );
    }
    bool wouldSeal() const override {
        BOOST_THROW_EXCEPTION(
            InterfaceNotSupported() << errinfo_interface( "ClientBase::wouldSeal" ) );
    }

    SyncStatus syncStatus() const override {
        BOOST_THROW_EXCEPTION(
            InterfaceNotSupported() << errinfo_interface( "ClientBase::syncStatus" ) );
    }

    Block latestBlock() const;

    int chainId() const override;

protected:
    /// The interface that must be implemented in any class deriving this.
    /// {
    virtual BlockChain& bc() = 0;
    virtual BlockChain const& bc() const = 0;
    virtual Block preSeal() const = 0;
    virtual Block postSeal() const = 0;
    virtual void prepareForTransaction() = 0;
    /// }

    // filters
    mutable Mutex x_filtersWatches;                         ///< Our lock.
    std::unordered_map< h256, InstalledFilter > m_filters;  ///< The dictionary of filters that are
                                                            ///< active.
    std::unordered_map< h256, h256s > m_specialFilters =
        std::unordered_map< h256, std::vector< h256 > >{
            {PendingChangedFilter, {}}, {ChainChangedFilter, {}}};
    ///< The dictionary of special filters and their additional data
    std::map< unsigned, ClientWatch > m_watches;  ///< Each and every watch - these reference a
                                                  ///< filter.

    Logger m_loggerWatch{createLogger( VerbosityDebug, "watch" )};
};

}  // namespace eth
}  // namespace dev
