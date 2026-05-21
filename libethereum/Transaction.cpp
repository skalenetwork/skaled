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
/** @file Transaction.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Transaction.h"
#include "Interface.h"
#include <libdevcore/CommonIO.h>
#include <libdevcore/Log.h>
#include <libdevcore/vector_ref.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Exceptions.h>
#include <libethereum/SchainPatch.h>
#include <libevm/VMFace.h>
#include <algorithm>

using namespace std;
using namespace dev;
using namespace dev::eth;

#define ETH_ADDRESS_DEBUG 0

std::ostream& dev::eth::operator<<( std::ostream& _out, ExecutionResult const& _er ) {
    _out << "{" << _er.gasUsed << ", " << _er.newAddress << ", " << toHex( _er.output ) << "}";
    return _out;
}

u256 Transaction::getEffectiveGasPrice( bool _isLondon, u256 const& _baseFeePerGas ) const {
#ifndef FAIR
    // Checked external-gas transactions are exempt from gas fees in non-FAIR builds:
    // no upfront gas deduction, no refund credit, no author fee. The same zero must flow
    // through both execution and receipt reconstruction, including under London.
    if ( m_externalGasIsChecked && hasExternalGas() ) {
        return 0;
    }
#endif
    if ( !_isLondon ) {
        // Pre-London: legacy gasPrice() for every tx type. For type-2 txs this is m_maxFeePerGas
        // (TransactionBase aligns m_gasPrice to maxFeePerGas on parse).
        return gasPrice();
    }
    if ( txType() != TransactionType::Type2 ) {
        return gasPrice();
    }
    return std::min( maxFeePerGas(), _baseFeePerGas + maxPriorityFeePerGas() );
}

TransactionException dev::eth::toTransactionException( Exception const& _e ) {
    // Basic Transaction exceptions
    if ( !!dynamic_cast< RLPException const* >( &_e ) )
        return TransactionException::BadRLP;
    if ( !!dynamic_cast< OutOfGasIntrinsic const* >( &_e ) )
        return TransactionException::OutOfGasIntrinsic;
    if ( !!dynamic_cast< InvalidSignature const* >( &_e ) )
        return TransactionException::InvalidSignature;
    // Executive exceptions
    if ( !!dynamic_cast< OutOfGasBase const* >( &_e ) )
        return TransactionException::OutOfGasBase;
    if ( !!dynamic_cast< InvalidNonce const* >( &_e ) )
        return TransactionException::InvalidNonce;
    if ( !!dynamic_cast< NotEnoughCash const* >( &_e ) )
        return TransactionException::NotEnoughCash;
    if ( !!dynamic_cast< BlockGasLimitReached const* >( &_e ) )
        return TransactionException::BlockGasLimitReached;
    if ( !!dynamic_cast< AddressAlreadyUsed const* >( &_e ) )
        return TransactionException::AddressAlreadyUsed;
    // VM execution exceptions
    if ( !!dynamic_cast< CodeStartsWith0xEF const* >( &_e ) )
        return TransactionException::CodeStartsWith0xEF;
    if ( !!dynamic_cast< BadInstruction const* >( &_e ) )
        return TransactionException::BadInstruction;
    if ( !!dynamic_cast< BadJumpDestination const* >( &_e ) )
        return TransactionException::BadJumpDestination;
    if ( !!dynamic_cast< OutOfGas const* >( &_e ) )
        return TransactionException::OutOfGas;
    if ( !!dynamic_cast< OutOfStack const* >( &_e ) )
        return TransactionException::OutOfStack;
    if ( !!dynamic_cast< StackUnderflow const* >( &_e ) )
        return TransactionException::StackUnderflow;
    if ( !!dynamic_cast< InvalidContractDeployer const* >( &_e ) )
        return TransactionException::InvalidContractDeployer;
#ifdef FAIR
    if ( !!dynamic_cast< UnsupportedDencunOpcode const* >( &_e ) )
        return TransactionException::UnsupportedDencunOpcode;
#endif
    return TransactionException::Unknown;
}

std::ostream& dev::eth::operator<<( std::ostream& _out, TransactionException const& _er ) {
    switch ( _er ) {
    case TransactionException::None:
        _out << "None";
        break;
    case TransactionException::BadRLP:
        _out << "BadRLP";
        break;
    case TransactionException::InvalidFormat:
        _out << "InvalidFormat";
        break;
    case TransactionException::OutOfGasIntrinsic:
        _out << "OutOfGasIntrinsic";
        break;
    case TransactionException::InvalidSignature:
        _out << "InvalidSignature";
        break;
    case TransactionException::InvalidNonce:
        _out << "InvalidNonce";
        break;
    case TransactionException::NotEnoughCash:
        _out << "NotEnoughCash";
        break;
    case TransactionException::OutOfGasBase:
        _out << "OutOfGasBase";
        break;
    case TransactionException::BlockGasLimitReached:
        _out << "BlockGasLimitReached";
        break;
    case TransactionException::BadInstruction:
        _out << "BadInstruction";
        break;
    case TransactionException::BadJumpDestination:
        _out << "BadJumpDestination";
        break;
    case TransactionException::OutOfGas:
        _out << "OutOfGas";
        break;
    case TransactionException::OutOfStack:
        _out << "OutOfStack";
        break;
    case TransactionException::StackUnderflow:
        _out << "StackUnderflow";
        break;
    case TransactionException::InvalidContractDeployer:
        _out << "InvalidContractDeployer";
        break;
    case TransactionException::RevertInstruction:
        _out << "RevertInstruction";
        break;
    case TransactionException::InvalidZeroSignatureFormat:
        _out << "InvalidZeroSignatureFormat";
        break;
    case TransactionException::AddressAlreadyUsed:
        _out << "AddressAlreadyUsed";
        break;
    case TransactionException::CodeStartsWith0xEF:
        _out << "CodeStartsWith0xEF";
        break;
    case TransactionException::WouldNotBeInBlock:
        _out << "WouldNotBeInBlock";
        break;
#ifdef FAIR
    case TransactionException::UnsupportedDencunOpcode:
        _out << "UnsupportedDencunOpcode";
        break;
#endif
    default:
        _out << "Unknown";
        break;
    }
    return _out;
}

Transaction::Transaction() {}

Transaction::Transaction( const TransactionSkeleton& _ts, const Secret& _s )
    : TransactionBase( _ts, _s ) {}

#ifdef FAIR

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const Address& _dest, const bytes& _data, const u256& _nonce, const u256& _chainId,
    const Secret& _secret )
    : TransactionBase( _value, _gasPrice, _gas, _dest, _data, _nonce, _chainId, _secret ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const Address& _dest, const bytes& _data, const u256& _nonce, const u256& _chainId )
    : TransactionBase( _value, _gasPrice, _gas, _dest, _data, _nonce, _chainId ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const bytes& _data, const u256& _nonce, const u256& _chainId, const Secret& _secret )
    : TransactionBase( _value, _gasPrice, _gas, _data, _nonce, _chainId, _secret ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const bytes& _data, const u256& _nonce, const u256& _chainId )
    : TransactionBase( _value, _gasPrice, _gas, _data, _nonce, _chainId ) {}

#endif

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const Address& _dest, const bytes& _data, const u256& _nonce, const Secret& _secret )
    : TransactionBase( _value, _gasPrice, _gas, _dest, _data, _nonce, _secret ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const Address& _dest, const bytes& _data, const u256& _nonce )
    : TransactionBase( _value, _gasPrice, _gas, _dest, _data, _nonce ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const bytes& _data, const u256& _nonce, const Secret& _secret )
    : TransactionBase( _value, _gasPrice, _gas, _data, _nonce, _secret ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const bytes& _data, const u256& _nonce )
    : TransactionBase( _value, _gasPrice, _gas, _data, _nonce ) {}

Transaction::Transaction( bytesConstRef _rlpData, CheckTransaction _checkSig, bool _allowInvalid,
    bool _eip1559Enabled, bool _invalidTransactionFormatPatchEnabled
#ifdef BITE
    ,
    bool _bite2PatchEnabled
#endif
    )
    : TransactionBase(
          _rlpData, _checkSig, _allowInvalid, _eip1559Enabled, _invalidTransactionFormatPatchEnabled
#ifdef BITE
          ,
          _bite2PatchEnabled
#endif
      ) {
}

Transaction::Transaction( const bytes& _rlp, CheckTransaction _checkSig, bool _allowInvalid,
    bool _eip1559Enabled, bool _invalidTransactionFormatPatchEnabled
#ifdef BITE
    ,
    bool _bite2PatchEnabled
#endif
    )
    : Transaction(
          &_rlp, _checkSig, _allowInvalid, _eip1559Enabled, _invalidTransactionFormatPatchEnabled
#ifdef BITE
          ,
          _bite2PatchEnabled
#endif
      ) {
}

#ifndef FAIR
bool Transaction::hasExternalGas() const {
#ifdef BITE
    // POW is disabled for CTXs
    if ( isCTX() ) {
        return false;
    }
#endif
    if ( !m_externalGasIsChecked ) {
        throw ExternalGasException();
    }
    return m_externalGas.has_value();
}

u256 Transaction::getExternalGas() const {
    // Never throws: short-circuits to 0 when external gas hasn't been checked yet, and
    // hasExternalGas() returns false (no throw) for checked-but-none and BITE CTX cases.
    if ( m_externalGasIsChecked && hasExternalGas() ) {
        return *m_externalGas;
    } else {
        return u256( 0 );
    }
}
#endif

u256 Transaction::gasPrice() const {
#ifdef FAIR
    return TransactionBase::gasPrice();
#else
    if ( m_externalGasIsChecked && hasExternalGas() ) {
        return 0;
    } else {
        return TransactionBase::gasPrice();
    }
#endif
}

#ifndef FAIR
void Transaction::checkOutExternalGas(
    const ChainParams& _cp, time_t _committedBlockTimestamp, uint64_t _committedBlockNumber ) {
    u256 const& difficulty = _cp.getExternalGasDifficulty();
    assert( difficulty > 0 );
    if ( !isInvalid() ) {
#ifdef BITE
        // POW is disabled for CTXs
        if ( isCTX() ) {
            return;
        }
#endif
        h256 hash;
        if ( !ExternalGasPatch::isEnabledWhen( _committedBlockTimestamp ) ) {
            hash = dev::sha3( sender().ref() ) ^ dev::sha3( nonce() ) ^ dev::sha3( gasPrice() );
        } else {
            // reset externalGas value
            // we may face patch activation after txn was added to the queue but before it was
            // executed. therefore we need to recalculate externalGas
            m_externalGasIsChecked = false;
            m_externalGas.reset();
            hash = dev::sha3( sender().ref() ) ^ dev::sha3( nonce() ) ^
                   dev::sha3( TransactionBase::gasPrice() );
        }
        if ( !hash ) {
            hash = h256( 1 );
        }
        u256 externalGas = ~u256( 0 ) / u256( hash ) / difficulty;
        if ( externalGas > 0 )
            BOOST_LOG( m_loggerTrace ) << "Mined gas: " << externalGas;

        EVMSchedule scheduleForUse = ConstantinopleSchedule;
        if ( CorrectForkInPowPatch::isEnabledWhen( _committedBlockTimestamp ) )
            scheduleForUse = _cp.makeEvmSchedule(
                _committedBlockTimestamp, _committedBlockNumber );  // BUG should be +1

        if ( externalGas >= baseGasRequired( scheduleForUse ) )
            m_externalGas = externalGas;

        m_externalGasIsChecked = true;
    }
}
#endif

LocalisedTransaction::LocalisedTransaction( const Transaction& _t, const h256& _blockHash,
    unsigned _transactionIndex, BlockNumber _blockNumber )
    : Transaction( _t ),
      m_transactionIndex( _transactionIndex ),
      m_blockNumber( _blockNumber ),
      m_blockHash( _blockHash ) {}

const h256& LocalisedTransaction::blockHash() const {
    return m_blockHash;
}

unsigned LocalisedTransaction::transactionIndex() const {
    return m_transactionIndex;
}

BlockNumber LocalisedTransaction::blockNumber() const {
    return m_blockNumber;
}
