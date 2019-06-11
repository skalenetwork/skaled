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
#include <libevm/VMFace.h>
using namespace std;
using namespace dev;
using namespace dev::eth;

#define ETH_ADDRESS_DEBUG 0

std::ostream& dev::eth::operator<<( std::ostream& _out, ExecutionResult const& _er ) {
    _out << "{" << _er.gasUsed << ", " << _er.newAddress << ", " << toHex( _er.output ) << "}";
    return _out;
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
    default:
        _out << "Unknown";
        break;
    }
    return _out;
}

Transaction::Transaction() {}

Transaction::Transaction( const TransactionSkeleton& _ts, const Secret& _s )
    : TransactionBase( _ts, _s ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const Address& _dest, const bytes& _data, const u256& _nonce, const Secret& _secret )
    : TransactionBase( _value, _gasPrice, _gas, _dest, _data, _nonce, _secret ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const bytes& _data, const u256& _nonce, const Secret& _secret )
    : TransactionBase( _value, _gasPrice, _gas, _data, _nonce, _secret ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const Address& _dest, const bytes& _data, const u256& _nonce )
    : TransactionBase( _value, _gasPrice, _gas, _dest, _data, _nonce ) {}

Transaction::Transaction( const u256& _value, const u256& _gasPrice, const u256& _gas,
    const bytes& _data, const u256& _nonce )
    : TransactionBase( _value, _gasPrice, _gas, _data, _nonce ) {}

Transaction::Transaction( bytesConstRef _rlpData, CheckTransaction _checkSig, bool _allowInvalid )
    : TransactionBase( _rlpData, _checkSig, _allowInvalid ) {}

Transaction::Transaction( const bytes& _rlp, CheckTransaction _checkSig, bool _allowInvalid )
    : Transaction( &_rlp, _checkSig, _allowInvalid ) {}

bool Transaction::hasExternalGas() const {
    if ( !m_externalGasIsChecked ) {
        throw ExternalGasException();
    }
    return m_externalGas.has_value();
}

u256 Transaction::getExternalGas() const {
    if ( hasExternalGas() ) {
        return *m_externalGas;
    } else {
        return u256( 0 );
    }
}

u256 Transaction::gas() const {
    if ( m_externalGasIsChecked && hasExternalGas() ) {
        return *m_externalGas;
    } else {
        return TransactionBase::gas();
    }
}

u256 Transaction::gasPrice() const {
    if ( m_externalGasIsChecked && hasExternalGas() ) {
        return 0;
    } else {
        return TransactionBase::gasPrice();
    }
}

void Transaction::checkOutExternalGas( u256 const& _difficulty ) {
    assert( _difficulty > 0 );
    if ( !m_externalGasIsChecked && !isInvalid() ) {
        h256 hash = dev::sha3( sender().ref() ) ^ dev::sha3( nonce() ) ^ dev::sha3( gasPrice() );
        if ( !hash ) {
            hash = h256( 1 );
        }
        u256 externalGas = ~u256( 0 ) / u256( hash ) / _difficulty;
        cdebug << "Mined gas: " << externalGas << endl;
        if ( externalGas >= baseGasRequired( ConstantinopleSchedule ) ) {
            m_externalGas = externalGas;
        }
        m_externalGasIsChecked = true;
    }
}

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
