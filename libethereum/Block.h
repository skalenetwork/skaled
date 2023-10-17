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
/** @file Block.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <array>
#include <unordered_map>

#include <libdevcore/Common.h>
#include <libdevcore/OverlayDB.h>
#include <libdevcore/RLP.h>
#include <libdevcore/TrieDB.h>
#include <libethcore/BlockHeader.h>
#include <libethcore/ChainOperationParams.h>
#include <libethcore/Counter.h>
#include <libethcore/Exceptions.h>
#include <libskale/State.h>


#include "Account.h"
#include "GasPricer.h"
#include "Transaction.h"
#include "TransactionReceipt.h"


namespace skale {
class State;
}  // namespace skale

namespace dev {
namespace test {
class ImportTest;
class StateLoader;
}  // namespace test

namespace eth {
class SealEngineFace;
class BlockChain;
class TransactionQueue;
struct VerifiedBlockRef;
class LastBlockHashesFace;

struct PopulationStatistics {
    double verify;
    double enact;
};

DEV_SIMPLE_EXCEPTION( ChainOperationWithUnknownBlockChain );
DEV_SIMPLE_EXCEPTION( InvalidOperationOnSealedBlock );

class AlethStandardTrace;

/**
 * @brief Active model of a block within the block chain.
 * Keeps track of all transactions, receipts and state for a particular block. Can apply all
 * needed transforms of the state for rewards and contains logic for sealing the block.
 */
class Block {
    friend class ExtVM;
    friend class dev::test::ImportTest;
    friend class dev::test::StateLoader;
    friend class Executive;
    friend class AlethExecutive;
    friend class BlockChain;

public:
    // TODO: pass in ChainOperationParams rather than u256

    /// Default constructor; creates with a blank database prepopulated with the genesis block.
    Block( u256 const& _accountStartNonce )
        : m_state( _accountStartNonce ), m_precommit( _accountStartNonce ) {}

    /// Basic state object from database.
    /// Use the default when you already have a database and you just want to make a Block object
    /// which uses it. If you have no preexisting database then set BaseState to something other
    /// than BaseState::PreExisting in order to prepopulate the Trie.
    /// You can also set the author address.
    Block( BlockChain const& _bc, boost::filesystem::path const& _dbPath, dev::h256 const& _genesis,
        skale::BaseState _bs = skale::BaseState::PreExisting, Address const& _author = Address() );

    Block( BlockChain const& _bc, h256 const& _hash, skale::State const& _state,
        skale::BaseState _bs = skale::BaseState::PreExisting, Address const& _author = Address() );

    enum NullType { Null };
    Block( NullType ) : m_state( 0 ), m_precommit( 0 ) {}

    /// Construct from a given blockchain. Empty, but associated with @a _bc 's chain params.
    explicit Block( BlockChain const& _bc ) : Block( Null ) { noteChain( _bc ); }

    /// Copy state object.
    Block( Block const& _s );

    /// Copy state object.
    Block& operator=( Block const& _s );

    /// Get the author address for any transactions we do and rewards we get.
    Address author() const { return m_author; }

    /// Set the author address for any transactions we do and rewards we get.
    /// This causes a complete reset of current block.
    void setAuthor( Address const& _id ) {
        m_author = _id;
        resetCurrent();
    }

    /// Note the fact that this block is being used with a particular chain.
    /// Call this before using any non-const methods.
    void noteChain( BlockChain const& _bc );

    // Account-getters. All operate on the final state.

    /// Get an account's balance.
    /// @returns 0 if the address has never been used.
    u256 balance( Address const& _address ) const { return m_state.balance( _address ); }

    /// Get the number of transactions a particular address has sent (used for the transaction
    /// nonce).
    /// @returns 0 if the address has never been used.
    u256 transactionsFrom( Address const& _address ) const { return m_state.getNonce( _address ); }

    /// Check if the address is in use.
    bool addressInUse( Address const& _address ) const { return m_state.addressInUse( _address ); }

    /// Check if the address contains executable code.
    bool addressHasCode( Address const& _address ) const {
        return m_state.addressHasCode( _address );
    }

    /// Get the value of a storage position of an account.
    /// @returns 0 if no account exists at that address.
    u256 storage( Address const& _contract, u256 const& _memory ) const {
        return m_state.storage( _contract, _memory );
    }

    std::map< h256, std::pair< u256, u256 > > storage( Address const& _contract ) const {
        return m_state.storage( _contract );
    }

    /// Get the code of an account.
    /// @returns bytes() if no account exists at that address.
    bytes const& code( Address const& _contract ) const { return m_state.code( _contract ); }

    /// Get the code hash of an account.
    /// @returns EmptySHA3 if no account exists at that address or if there is no code associated
    /// with the address.
    h256 codeHash( Address const& _contract ) const { return m_state.codeHash( _contract ); }

    // General information from state

    /// Get the backing state object.
    skale::State const& state() const { return m_state; }

    // For altering accounts behind-the-scenes

    /// Get a mutable State object which is backing this block.
    /// @warning Only use this is you know what you're doing. If you use it while constructing a
    /// normal sealable block, don't expect things to work right.
    skale::State& mutableState() { return m_state; }

    // Information concerning ongoing transactions

    /// Get the remaining gas limit in this block.
    u256 gasLimitRemaining() const { return m_currentBlock.gasLimit() - gasUsed(); }

    /// Get the list of pending transactions.
    Transactions const& pending() const { return m_transactions; }

    /// Get the list of hashes of pending transactions.
    h256Hash const& pendingHashes() const { return m_transactionSet; }

    /// Get the transaction receipt for the transaction of the given index.
    TransactionReceipt const& receipt( unsigned _i ) const { return m_receipts.at( _i ); }

    /// Get the list of pending transactions.
    LogEntries const& log( unsigned _i ) const { return receipt( _i ).log(); }

    /// Get the bloom filter of all logs that happened in the block.
    LogBloom logBloom() const;

    /// Get the bloom filter of a particular transaction that happened in the block.
    LogBloom const& logBloom( unsigned _i ) const { return receipt( _i ).bloom(); }

    /// Get the State root hash immediately after all previous transactions before transaction @a _i
    /// have been applied. If (_i == 0) returns the initial state of the block. If (_i ==
    /// pending().size()) returns the final state of the block, prior to rewards. Returns zero hash
    /// if intermediate state root is not available in the receipt (the case after EIP98)
    h256 stateRootBeforeTx( unsigned _i ) const;

    // State-change operations

    /// Construct state object from arbitrary point in blockchain.
    PopulationStatistics populateFromChain( BlockChain const& _bc, h256 const& _hash,
        ImportRequirements::value _ir = ImportRequirements::None );

    /// Execute a given transaction.
    /// This will append @a _t to the transaction list and change the state accordingly.
    ExecutionResult execute( LastBlockHashesFace const& _lh, Transaction const& _t,
        skale::Permanence _p = skale::Permanence::Committed, OnOpFunc const& _onOp = OnOpFunc() );

#ifndef HISTORIC_STATE
#define HISTORIC_STATE
#endif

#ifdef HISTORIC_STATE
    ExecutionResult executeHistoricCall( LastBlockHashesFace const& _lh, Transaction const& _t,
        std::shared_ptr< AlethStandardTrace > _tracer );
#endif


    /// Sync our transactions, killing those from the queue that we have and assimilating those that
    /// we don't.
    /// @returns a list of receipts one for each transaction placed from the queue into the state
    /// and bool, true iff there are more transactions to be processed.
    std::pair< TransactionReceipts, bool > sync( BlockChain const& _bc, TransactionQueue& _tq,
        GasPricer const& _gp, unsigned _msTimeout = 100 );

    /// Sync our state with the block chain.
    /// This basically involves wiping ourselves if we've been superceded and rebuilding from the
    /// transaction queue.
    bool sync( BlockChain const& _bc );

    bool sync( BlockChain const& _bc, skale::State const& _state );

    /// Sync with the block chain, but rather than synching to the latest block, instead sync to the
    /// given block.
    bool sync(
        BlockChain const& _bc, h256 const& _blockHash, BlockHeader const& _bi = BlockHeader() );

    /// Sync all transactions unconditionally
    std::tuple< TransactionReceipts, unsigned > syncEveryone( BlockChain const& _bc,
        const Transactions& _transactions, uint64_t _timestamp, u256 _gasPrice,
        Transactions* vecMissing = nullptr  // it's non-null only for PARTIAL CATCHUP
    );

    /// Execute all transactions within a given block.
    /// @returns the additional total difficulty.
    u256 enactOn( VerifiedBlockRef const& _block, BlockChain const& _bc );

    /// Returns back to a pristine state after having done a playback.
    void cleanup();

    /// Sets m_currentBlock to a clean state, (i.e. no change from m_previousBlock) and
    /// optionally modifies the timestamp.
    void resetCurrent( int64_t _timestamp = utcTime() );

    // Sealing

    /// Prepares the current state for mining.
    /// Commits all transactions into the trie, compiles uncles and transactions list, applies all
    /// rewards and populates the current block header with the appropriate hashes.
    /// The only thing left to do after this is to actually mine().
    ///
    /// This may be called multiple times and without issue.
    void commitToSeal( BlockChain const& _bc, bytes const& _extraData = {},
        dev::h256 const& _stateRootHash = dev::h256( "" ) );

    /// Pass in a properly sealed header matching this block.
    /// @returns true iff we were previously committed to sealing, the header is valid and it
    /// corresponds to this block.
    /// TODO: verify it prior to calling this.
    /** Commit to DB and build the final block if the previous call to mine()'s result is
     * completion. Typically looks like:
     * @code
     * while (!isSealed)
     * {
     * // lock
     * commitToSeal(_blockChain);  // will call uncommitToSeal if a repeat.
     * sealBlock(sealedHeader);
     * // unlock
     * @endcode
     */
    bool sealBlock( bytes const& _header ) { return sealBlock( &_header ); }
    bool sealBlock( bytesConstRef _header );

    /// @returns true if sealed - in this case you can no longer append transactions.
    bool isSealed() const { return !m_currentBytes.empty(); }

    /// Get the complete current block, including valid nonce.
    /// Only valid when isSealed() is true.
    bytes const& blockData() const { return m_currentBytes; }

    /// Get the header information on the present block.
    BlockHeader const& info() const { return m_currentBlock; }

    void startReadState();

private:
    SealEngineFace* sealEngine() const;

    /// Undo the changes to the state for committing to mine.
    void uncommitToSeal();

    /// Execute the given block, assuming it corresponds to m_currentBlock.
    /// Throws on failure.
    u256 enact( VerifiedBlockRef const& _block, BlockChain const& _bc );

    /// Finalise the block, applying the earned rewards.
    void applyRewards(
        std::vector< BlockHeader > const& _uncleBlockHeaders, u256 const& _blockReward );

    /// @returns gas used by transactions thus far executed.
    u256 gasUsed() const { return m_receipts.size() ? m_receipts.back().cumulativeGasUsed() : 0; }

    /// Performs irregular modifications right after initialization, e.g. to implement a hard fork.
    void performIrregularModifications();

    /// Creates and updates the special contract for storing block hashes according to EIP96
    void updateBlockhashContract();

    State m_state;                ///< Our state.
    Transactions m_transactions;  ///< The current list of transactions that we've included in the
                                  ///< state.
    TransactionReceipts m_receipts;  ///< The corresponding list of transaction receipts.
    h256Hash m_transactionSet;  ///< The set of transaction hashes that we've included in the state.
    skale::State m_precommit;   ///< State at the point immediately prior to rewards.

    BlockHeader m_previousBlock;     ///< The previous block's information.
    BlockHeader m_currentBlock;      ///< The current block's information.
    bytes m_currentBytes;            ///< The current block's bytes.
    bool m_committedToSeal = false;  ///< Have we committed to mine on the present m_currentBlock?

    bytes m_currentTxs;     ///< The RLP-encoded block of transactions.
    bytes m_currentUncles;  ///< The RLP-encoded block of uncles.

    Address m_author;  ///< Our address (i.e. the address to which fees go).


    SealEngineFace* m_sealEngine = nullptr;  ///< The chain's seal engine.

    Logger m_logger{ createLogger( VerbosityDebug, "block" ) };
    Logger m_loggerDetailed{ createLogger( VerbosityTrace, "block" ) };

    Counter< Block > c;
    ;

public:
    static uint64_t howMany() { return Counter< Block >::howMany(); }
};


}  // namespace eth

}  // namespace dev
