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
/** @file Transaction.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libethcore/ChainOperationParams.h>
#include <libethcore/Common.h>
#include <libethcore/TransactionBase.h>

#include "ChainParams.h"

namespace dev {
namespace eth {

enum class TransactionException {
    None = 0,
    Unknown,
    BadRLP,
    InvalidFormat,
    OutOfGasIntrinsic,  ///< Too little gas to pay for the base transaction cost.
    InvalidSignature,
    InvalidNonce,
    NotEnoughCash,
    OutOfGasBase,  ///< Too little gas to pay for the base transaction cost.
    BlockGasLimitReached,
    BadInstruction,
    BadJumpDestination,
    OutOfGas,    ///< Ran out of gas executing code of the transaction.
    OutOfStack,  ///< Ran out of stack executing code of the transaction.
    StackUnderflow,
    RevertInstruction,
    InvalidZeroSignatureFormat,
    AddressAlreadyUsed,
    InvalidContractDeployer,
    WouldNotBeInBlock  ///< In original Ethereum this tx should not be included in block
};

enum class CodeDeposit { None = 0, Failed, Success };

struct VMException;

DEV_SIMPLE_EXCEPTION( ExternalGasException );

TransactionException toTransactionException( Exception const& _e );
std::ostream& operator<<( std::ostream& _out, TransactionException const& _er );

/// Description of the result of executing a transaction.
struct ExecutionResult {
    u256 gasUsed = 0;
    TransactionException excepted = TransactionException::Unknown;
    Address newAddress;
    bytes output;
    CodeDeposit codeDeposit =
        CodeDeposit::None;  ///< Failed if an attempted deposit failed due to lack of gas.
    u256 gasRefunded = 0;
    unsigned depositSize = 0;  ///< Amount of code of the creation's attempted deposit.
    u256 gasForDeposit;        ///< Amount of gas remaining for the code deposit phase.
};

std::ostream& operator<<( std::ostream& _out, ExecutionResult const& _er );

/// Encodes a transaction, ready to be exported to or freshly imported from RLP.
class Transaction : public TransactionBase {
public:
    /// Constructs a null transaction.
    Transaction();

    /// Constructs from a transaction skeleton & optional secret.
    Transaction( TransactionSkeleton const& _ts, Secret const& _s = Secret() );

    /// Constructs a signed message-call transaction.
    Transaction( u256 const& _value, u256 const& _gasPrice, u256 const& _gas, Address const& _dest,
        bytes const& _data, u256 const& _nonce, Secret const& _secret );

    /// Constructs a signed contract-creation transaction.
    Transaction( u256 const& _value, u256 const& _gasPrice, u256 const& _gas, bytes const& _data,
        u256 const& _nonce, Secret const& _secret );

    /// Constructs an unsigned message-call transaction.
    Transaction( u256 const& _value, u256 const& _gasPrice, u256 const& _gas, Address const& _dest,
        bytes const& _data, u256 const& _nonce = Invalid256 );

    /// Constructs an unsigned contract-creation transaction.
    Transaction( u256 const& _value, u256 const& _gasPrice, u256 const& _gas, bytes const& _data,
        u256 const& _nonce = Invalid256 );

    /// Constructs a transaction from the given RLP.
    explicit Transaction( bytesConstRef _rlp, CheckTransaction _checkSig,
        bool _allowInvalid = false, bool _eip1559Enabled = false );

    /// Constructs a transaction from the given RLP.
    explicit Transaction( bytes const& _rlp, CheckTransaction _checkSig, bool _allowInvalid = false,
        bool _eip1559Enabled = false );

    Transaction( Transaction const& ) = default;

    bool hasExternalGas() const;

    u256 getExternalGas() const;

    u256 gasPrice() const;

    void checkOutExternalGas(
        const ChainParams& _cp, time_t _committedBlockTimestamp, uint64_t _committedBlockNumber );

    void ignoreExternalGas() {
        m_externalGasIsChecked = true;
        m_externalGas.reset();
    }

private:
    bool m_externalGasIsChecked = false;
    std::optional< u256 > m_externalGas;
};

/// Nice name for vector of Transaction.
using Transactions = std::vector< Transaction >;

class LocalisedTransaction : public Transaction {
public:
    LocalisedTransaction( Transaction const& _t, h256 const& _blockHash, unsigned _transactionIndex,
        BlockNumber _blockNumber = 0 );

    h256 const& blockHash() const;
    unsigned transactionIndex() const;
    BlockNumber blockNumber() const;

private:
    unsigned m_transactionIndex;
    BlockNumber m_blockNumber;
    h256 m_blockHash;
};

}  // namespace eth
}  // namespace dev
