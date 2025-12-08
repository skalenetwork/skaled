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
/** @file TransactionBase.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "TransactionBase.h"

#include "EVMSchedule.h"

#include <libdevcore/Log.h>
#include <libdevcore/vector_ref.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Exceptions.h>

#include <libdevcore/microprofile.h>

#ifdef BITE
#include <libconsensus/libBLS/threshold_encryption/ThresholdEncryption.h>
#include <libconsensus/node/ConsensusInterface.h>
#endif
using namespace std;

using namespace dev;
using namespace dev::eth;

const size_t MAX_ACCESS_LIST_COUNT = 16;


#ifdef BITE
// Used for comparing the 'to' address against this constant to check if transaction is BITE
const dev::Address TransactionBase::BITE_ADDRESS =
    dev::Address( std::string( BITE_ADDRESS_AS_STRING ) );
#endif

std::vector< bytes > validateAccessListRLP( const RLP& _data ) {
    if ( !_data.isList() )
        BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                               << errinfo_comment( "transaction accessList RLP must be a list" ) );
    auto rlpList = _data.toList();
    if ( rlpList.empty() ) {
        // empty accessList, ignore it
        return {};
    }

    for ( const auto& d : rlpList ) {
        if ( !d.isList() )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat() << errinfo_comment(
                                       "transaction accessList RLP must be a list" ) );
        auto accessList = d.toList();
        if ( accessList.size() != 2 )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat() << errinfo_comment(
                                       "transaction accessList RLP must be a list of size 2" ) );
        if ( !accessList[0].isData() || !accessList[1].isList() )
            BOOST_THROW_EXCEPTION(
                InvalidTransactionFormat() << errinfo_comment(
                    "transaction accessList RLP must be a list of byte array and a list" ) );
        for ( const auto& k : accessList[1].toList() )
            if ( !k.isData() )
                BOOST_THROW_EXCEPTION(
                    InvalidTransactionFormat() << errinfo_comment(
                        "transaction storageKeys RLP must be a list of byte array" ) );
    }

    if ( rlpList.size() > MAX_ACCESS_LIST_COUNT )
        BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                               << errinfo_comment( "The number of access lists is too large." ) );

    std::vector< bytes > accessList( rlpList.size() );
    for ( size_t i = 0; i < rlpList.size(); ++i ) {
        accessList[i] = rlpList.at( i ).data().toBytes();
    }

    return accessList;
}

dev::RLPs accessListToRLPs( const std::vector< bytes >& _accessList ) {
    dev::RLPs accessList( _accessList.size() );
    for ( size_t i = 0; i < _accessList.size(); ++i ) {
        accessList[i] = RLP( _accessList[i] );
    }
    return accessList;
}

TransactionBase::TransactionBase( TransactionSkeleton const& _ts, Secret const& _s )
    : m_nonce( _ts.nonce ),
      m_value( _ts.value ),
      m_gasPrice( _ts.gasPrice ),
      m_gas( _ts.gas ),
      m_data( _ts.data ),
      m_type( _ts.creation ? ContractCreation : MessageCall ),
      m_sender( _ts.from ),
      m_receiveAddress( _ts.to ) {
    if ( _s )
        sign( _s );

#ifdef BITE
    checkIfBITETxnAndSet( _ts.to );
#endif
}

void TransactionBase::fillFromBytesLegacy(
    bytesConstRef _rlpData, CheckTransaction _checkSig, bool _allowInvalid ) {
    RLP const rlp( _rlpData );
    try {
        if ( !rlp.isList() )
            BOOST_THROW_EXCEPTION(
                InvalidTransactionFormat() << errinfo_comment( "transaction RLP must be a list" ) );

        m_nonce = rlp[0].toInt< u256 >();
        m_gasPrice = rlp[1].toInt< u256 >();
        m_gas = rlp[2].toInt< u256 >();
        if ( !rlp[3].isData() )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                   << errinfo_comment( "recipient RLP must be a byte array" ) );
        m_type = rlp[3].isEmpty() ? ContractCreation : MessageCall;
        m_receiveAddress =
            rlp[3].isEmpty() ? Address() : rlp[3].toHash< Address >( RLP::VeryStrict );
        m_value = rlp[4].toInt< u256 >();

        if ( !rlp[5].isData() )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                   << errinfo_comment( "transaction data RLP must be an array" ) );

        m_data = rlp[5].toBytes();

        u256 const v = rlp[6].toInt< u256 >();
        h256 const r = rlp[7].toInt< u256 >();
        h256 const s = rlp[8].toInt< u256 >();

        if ( isZeroSignature( r, s ) ) {
            m_chainId = static_cast< uint64_t >( v );
            m_vrs = SignatureStruct{ r, s, 0 };
        } else {
            if ( v > 36 ) {
                auto const chainId = ( v - 35 ) / 2;
                if ( chainId > std::numeric_limits< uint64_t >::max() )
                    BOOST_THROW_EXCEPTION( InvalidSignature() );
                m_chainId = static_cast< uint64_t >( chainId );
            } else if ( v != 27 && v != 28 ) {
                BOOST_THROW_EXCEPTION( InvalidSignature() );
            }

            // ifdef FAIR, then chainId is always set at this point - will fall into first branch
            auto const recoveryID = m_chainId.has_value() ?
                                        _byte_{ v - ( u256{ *m_chainId } * 2 + 35 ) } :
                                        _byte_{ v - 27 };

            m_vrs = SignatureStruct{ r, s, recoveryID };

            if ( _checkSig >= CheckTransaction::Cheap && !m_vrs->isValid() )
                BOOST_THROW_EXCEPTION( InvalidSignature() );
        }

        if ( _checkSig == CheckTransaction::Everything )
            m_sender = sender();

        m_txType = TransactionType::Legacy;

        if ( rlp.itemCount() > 9 )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                   << errinfo_comment( "too many fields in the transaction RLP" ) );
        // XXX Strange "catch"-s %)
    } catch ( Exception& _e ) {
        _e << errinfo_name(
            "invalid transaction format: " + toString( rlp ) + " RLP: " + toHex( rlp.data() ) );
        m_type = Type::Invalid;
        m_rawData = std::make_shared< bytes >( _rlpData.toBytes() );

        if ( !_allowInvalid )
            throw;
        else {
            cwarn << _e.what();
        }
    }
}

void TransactionBase::fillFromBytesType1( bytesConstRef _rlpData, CheckTransaction _checkSig,
    bool _allowInvalid, bool _invalidTransactionFormatPatchEnabled ) {
    bytes croppedRlp( _rlpData.begin() + 1, _rlpData.end() );
    RLP const rlp( croppedRlp );
    try {
        if ( !rlp.isList() )
            BOOST_THROW_EXCEPTION(
                InvalidTransactionFormat() << errinfo_comment( "transaction RLP must be a list" ) );

        m_chainId = rlp[0].toInt< u256 >().convert_to< uint64_t >();
        m_nonce = rlp[1].toInt< u256 >();
        m_gasPrice = rlp[2].toInt< u256 >();
        m_gas = rlp[3].toInt< u256 >();

        if ( !rlp[4].isData() )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                   << errinfo_comment( "recipient RLP must be a byte array" ) );
        m_type = rlp[4].isEmpty() ? ContractCreation : MessageCall;
        m_receiveAddress =
            rlp[4].isEmpty() ? Address() : rlp[4].toHash< Address >( RLP::VeryStrict );
        m_value = rlp[5].toInt< u256 >();

        if ( !rlp[6].isData() )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                   << errinfo_comment( "transaction data RLP must be an array" ) );

        m_data = rlp[6].toBytes();

        m_accessList = validateAccessListRLP( rlp[7] );

        bool const yParity = rlp[8].toInt< uint8_t >();
        h256 const r = rlp[9].toInt< u256 >();
        h256 const s = rlp[10].toInt< u256 >();

        m_vrs = SignatureStruct{ r, s, yParity };

        if ( _checkSig >= CheckTransaction::Cheap && !m_vrs->isValid() )
            BOOST_THROW_EXCEPTION( InvalidSignature() );

        m_txType = TransactionType::Type1;

        if ( _checkSig == CheckTransaction::Everything )
            m_sender = sender();

        if ( rlp.itemCount() > 11 )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                   << errinfo_comment( "too many fields in the transaction RLP" ) );
    } catch ( Exception& _e ) {
        _e << errinfo_name(
            "invalid transaction format: " + toString( rlp ) + " RLP: " + toHex( rlp.data() ) );
        m_type = Type::Invalid;
        if ( _invalidTransactionFormatPatchEnabled )
            // use croppedRlp here because the first byte in _rlpData contains transaction type
            // identifier that is, it makes _rlpData an invalid rlp object
            m_rawData = std::make_shared< bytes >( croppedRlp );
        else
            m_rawData = std::make_shared< bytes >( _rlpData.toBytes() );

        if ( !_allowInvalid )
            throw;
        else {
            cwarn << _e.what();
        }
    }
}

void TransactionBase::fillFromBytesType2( bytesConstRef _rlpData, CheckTransaction _checkSig,
    bool _allowInvalid, bool _invalidTransactionFormatPatchEnabled ) {
    bytes croppedRlp( _rlpData.begin() + 1, _rlpData.end() );
    RLP const rlp( croppedRlp );
    try {
        if ( !rlp.isList() )
            BOOST_THROW_EXCEPTION(
                InvalidTransactionFormat() << errinfo_comment( "transaction RLP must be a list" ) );

        m_chainId = rlp[0].toInt< u256 >().convert_to< uint64_t >();
        m_nonce = rlp[1].toInt< u256 >();
        m_maxPriorityFeePerGas = rlp[2].toInt< u256 >();
        m_maxFeePerGas = rlp[3].toInt< u256 >();

        auto toCompare =
            _invalidTransactionFormatPatchEnabled ? m_maxFeePerGas : m_maxPriorityFeePerGas;
        if ( m_maxPriorityFeePerGas > toCompare )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat() << errinfo_comment(
                                       "maxFeePerGas cannot be less than maxPriorityFeePerGas (The "
                                       "total must be the larger of the two)" ) );
        // set m_gasPrice as SKALE ignores priority fees
        m_gasPrice = m_maxFeePerGas;
        m_gas = rlp[4].toInt< u256 >();

        if ( !rlp[5].isData() )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                   << errinfo_comment( "recipient RLP must be a byte array" ) );
        m_type = rlp[5].isEmpty() ? ContractCreation : MessageCall;
        m_receiveAddress =
            rlp[5].isEmpty() ? Address() : rlp[5].toHash< Address >( RLP::VeryStrict );
        m_value = rlp[6].toInt< u256 >();

        if ( !rlp[7].isData() )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                   << errinfo_comment( "transaction data RLP must be an array" ) );

        m_data = rlp[7].toBytes();

        m_accessList = validateAccessListRLP( rlp[8] );

        bool const yParity = rlp[9].toInt< uint8_t >();
        h256 const r = rlp[10].toInt< u256 >();
        h256 const s = rlp[11].toInt< u256 >();

        m_vrs = SignatureStruct{ r, s, yParity };

        if ( _checkSig >= CheckTransaction::Cheap && !m_vrs->isValid() )
            BOOST_THROW_EXCEPTION( InvalidSignature() );

        m_txType = TransactionType::Type2;

        if ( _checkSig == CheckTransaction::Everything )
            m_sender = sender();

        if ( rlp.itemCount() > 12 )
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                   << errinfo_comment( "too many fields in the transaction RLP" ) );
    } catch ( Exception& _e ) {
        _e << errinfo_name(
            "invalid transaction format: " + toString( rlp ) + " RLP: " + toHex( rlp.data() ) );
        m_type = Type::Invalid;
        if ( _invalidTransactionFormatPatchEnabled )
            // use croppedRlp here because the first byte in _rlpData contains transaction type
            // identifier that is, it makes _rlpData an invalid rlp object
            m_rawData = std::make_shared< bytes >( croppedRlp );
        else
            m_rawData = std::make_shared< bytes >( _rlpData.toBytes() );

        if ( !_allowInvalid )
            throw;
        else {
            cwarn << _e.what();
        }
    }
}

void TransactionBase::fillFromBytesByType( bytesConstRef _rlpData, CheckTransaction _checkSig,
    bool _allowInvalid, TransactionType _type, bool _invalidTransactionFormatPatchEnabled ) {
    switch ( _type ) {
    case TransactionType::Legacy:
        fillFromBytesLegacy( _rlpData, _checkSig, _allowInvalid );
        break;
    case TransactionType::Type1:
        fillFromBytesType1(
            _rlpData, _checkSig, _allowInvalid, _invalidTransactionFormatPatchEnabled );
        break;
    case TransactionType::Type2:
        fillFromBytesType2(
            _rlpData, _checkSig, _allowInvalid, _invalidTransactionFormatPatchEnabled );
        break;
    default:
        BOOST_THROW_EXCEPTION(
            InvalidTransactionFormat() << errinfo_comment(
                "transaction format doesn't correspond to any of the supported formats" ) );
    }
}

TransactionType TransactionBase::getTransactionType( bytesConstRef _rlp ) {
    if ( _rlp.empty() )
        return TransactionType::Legacy;
    if ( _rlp[0] > 2 )
        return TransactionType::Legacy;
    return TransactionType( _rlp[0] );
}

TransactionBase::TransactionBase( bytesConstRef _rlpData, CheckTransaction _checkSig,
    bool _allowInvalid, bool _eip1559Enabled, bool _invalidTransactionFormatPatchEnabled ) {
    MICROPROFILE_SCOPEI( "TransactionBase", "ctor", MP_GOLD2 );
    try {
        if ( _eip1559Enabled ) {
            TransactionType txnType = getTransactionType( _rlpData );
            fillFromBytesByType( _rlpData, _checkSig, _allowInvalid, txnType,
                _invalidTransactionFormatPatchEnabled );
        } else {
            fillFromBytesLegacy( _rlpData, _checkSig, _allowInvalid );
        }
#ifdef BITE
        // check if a txn is a BITE txn here
        // bad formatted txns cannot make it to the block
        // therefore no need to check it anywhere else
        checkIfBITETxnAndSet( m_receiveAddress );
#endif
    } catch ( std::exception& e ) {
        m_type = Type::Invalid;
        RLPStream s;
        s.append( _rlpData.toBytes() );  // add "string" header
        m_rawData = std::make_shared< bytes >( s.out() );

        if ( !_allowInvalid ) {
            cerror << "Got invalid transaction." << e.what();
            throw;
        }
    } catch ( ... ) {
        m_type = Type::Invalid;
        RLPStream s;
        s.append( _rlpData.toBytes() );  // add "string" header
        m_rawData = std::make_shared< bytes >( s.out() );

        if ( !_allowInvalid ) {
            cerror << "Got invalid transaction.";
            throw;
        }
    }
}  // ctor

Address const& TransactionBase::safeSender() const noexcept {
    try {
        return sender();
    } catch ( ... ) {
        return ZeroAddress;
    }
}

Address const& TransactionBase::sender() const {
    if ( !m_sender.has_value() ) {
        if ( isInvalid() || hasZeroSignature() )
            m_sender = MaxAddress;
        else {
            if ( !m_vrs )
                BOOST_THROW_EXCEPTION( TransactionIsUnsigned() );

            auto p = recover( *m_vrs, sha3( WithoutSignature ) );
            if ( !p )
                BOOST_THROW_EXCEPTION( InvalidSignature() );
            m_sender = right160( dev::sha3( bytesConstRef( p.data(), sizeof( p ) ) ) );
        }
    }
    return *m_sender;
}

SignatureStruct const& TransactionBase::signature() const {
    if ( isInvalid() || !m_vrs )
        BOOST_THROW_EXCEPTION( TransactionIsUnsigned() );

    return *m_vrs;
}

void TransactionBase::sign( Secret const& _priv ) {
    CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot sign transaction." );

    auto sig = dev::sign( _priv, sha3( WithoutSignature ) );
    SignatureStruct sigStruct = *( SignatureStruct const* ) &sig;
    if ( sigStruct.isValid() )
        m_vrs = sigStruct;
}

void TransactionBase::streamLegacyTransaction(
    RLPStream& _s, IncludeSignature _sig, bool _forEip155hash ) const {
    _s.appendList( ( _sig || _forEip155hash ? 3 : 0 ) + 6 );
    _s << m_nonce << m_gasPrice << m_gas;
    if ( m_type == MessageCall )
        _s << m_receiveAddress;
    else
        _s << "";
    _s << m_value << m_data;

    if ( _sig ) {
        if ( !m_vrs )
            BOOST_THROW_EXCEPTION( TransactionIsUnsigned() );

        if ( hasZeroSignature() )
            _s << ( m_chainId.has_value() ? *m_chainId : 0 );
        else {
            uint64_t const vOffset = m_chainId.has_value() ? *m_chainId * 2 + 35 : 27;
            _s << ( m_vrs->v + vOffset );
        }
        _s << ( u256 ) m_vrs->r << ( u256 ) m_vrs->s;
    } else if ( _forEip155hash )
        _s << *m_chainId << 0 << 0;
}

void TransactionBase::streamType1Transaction( RLPStream& _s, IncludeSignature _sig ) const {
    _s.appendList( ( _sig ? 3 : 0 ) + 8 );
    if ( !m_chainId.has_value() )
        BOOST_THROW_EXCEPTION( InvalidTransactionFormat() );
    _s << *m_chainId << m_nonce << m_gasPrice << m_gas;
    if ( m_type == MessageCall )
        _s << m_receiveAddress;
    else
        _s << "";
    _s << m_value << m_data;

    _s << accessListToRLPs( m_accessList );

    if ( _sig ) {
        _s << static_cast< u256 >( m_vrs->v );
        _s << static_cast< u256 >( m_vrs->r );
        _s << static_cast< u256 >( m_vrs->s );
    }
}

void TransactionBase::streamType2Transaction( RLPStream& _s, IncludeSignature _sig ) const {
    _s.appendList( ( _sig ? 3 : 0 ) + 9 );
    if ( !m_chainId.has_value() )
        BOOST_THROW_EXCEPTION( InvalidTransactionFormat() );
    _s << *m_chainId << m_nonce << m_maxPriorityFeePerGas << m_maxFeePerGas << m_gas;
    if ( m_type == MessageCall )
        _s << m_receiveAddress;
    else
        _s << "";
    _s << m_value << m_data;

    _s << accessListToRLPs( m_accessList );

    if ( _sig ) {
        _s << static_cast< u256 >( m_vrs->v );
        _s << static_cast< u256 >( m_vrs->r );
        _s << static_cast< u256 >( m_vrs->s );
    }
}

void TransactionBase::streamRLP( RLPStream& _s, IncludeSignature _sig, bool _forEip155hash ) const {
    if ( isInvalid() ) {
        _s.appendRaw( *m_rawData );
        return;
    }

    if ( m_type == NullTransaction )
        return;

    switch ( m_txType ) {
    case TransactionType::Legacy:
        streamLegacyTransaction( _s, _sig, _forEip155hash );
        break;
    case TransactionType::Type1:
        streamType1Transaction( _s, _sig );
        break;
    case TransactionType::Type2:
        streamType2Transaction( _s, _sig );
        break;
    default:
        break;
    }
}

static const u256 c_secp256k1n(
    "115792089237316195423570985008687907852837564279074904382605163141518161494337" );

void TransactionBase::checkLowS() const {
    if ( !m_vrs )
        BOOST_THROW_EXCEPTION( TransactionIsUnsigned() );

    if ( m_vrs->s > c_secp256k1n / 2 )
        BOOST_THROW_EXCEPTION( InvalidSignature() );
}

void TransactionBase::checkChainId( uint64_t chainId ) const {
    if ( !m_chainId.has_value() ) {
        BOOST_THROW_EXCEPTION( InvalidTransactionFormat() );
    }

    if ( m_chainId != chainId )
        BOOST_THROW_EXCEPTION( InvalidSignature() << errinfo_txHash( sha3() ) );
}

int64_t TransactionBase::baseGasRequired(
    bool _contractCreation, bytesConstRef _data, EVMSchedule const& _es
#ifdef BITE
    ,
    bool _isBITETxn
#endif
#ifdef BITE2
    ,
    std::optional< size_t > _bite2EncryptedArgsSize
#endif
) {
    int64_t g = _contractCreation ? _es.txCreateGas : _es.txGas;

    // charge the cost of BITE transaction
#ifdef BITE
    if ( _isBITETxn ) {
        if ( g > std::numeric_limits< int64_t >::max() - _es.BITETxnCost )
            BOOST_THROW_EXCEPTION(
                InvalidTransactionFormat() << errinfo_comment( "Gas calculation overflow" ) );
        g += _es.BITETxnCost;
    }
#endif

#ifdef BITE2
    // BITE2 transaction - charge for every encrypted payload
    if ( _bite2EncryptedArgsSize.has_value() ) {
        // Check for multiplication overflow
        if ( _bite2EncryptedArgsSize.value() > 0 &&
             _es.BITETxnCost >
                 std::numeric_limits< int64_t >::max() / _bite2EncryptedArgsSize.value() )
            BOOST_THROW_EXCEPTION(
                InvalidTransactionFormat() << errinfo_comment( "Gas calculation overflow" ) );
        int64_t bite2Cost = _bite2EncryptedArgsSize.value() * _es.BITETxnCost;
        if ( g > std::numeric_limits< int64_t >::max() - bite2Cost )
            BOOST_THROW_EXCEPTION(
                InvalidTransactionFormat() << errinfo_comment( "Gas calculation overflow" ) );
        g += bite2Cost;
    }
#endif

    // Calculate the cost of input data.
    // No risk of overflow by using int64 until txDataNonZeroGas is quite small
    // (the value not in billions).
    for ( auto i : _data ) {
        int64_t dataCost = i ? _es.txDataNonZeroGas : _es.txDataZeroGas;
        if ( g > std::numeric_limits< int64_t >::max() - dataCost )
            BOOST_THROW_EXCEPTION(
                InvalidTransactionFormat() << errinfo_comment( "Gas calculation overflow" ) );
        g += dataCost;
    }
    return g;
}

h256 TransactionBase::sha3( IncludeSignature _sig ) const {
    if ( _sig == WithSignature && m_hashWith )
        return m_hashWith;

    MICROPROFILE_SCOPEI( "TransactionBase", "sha3", MP_KHAKI2 );

    dev::bytes input;
    if ( !isInvalid() ) {
        RLPStream s;

        streamRLP( s, _sig, isReplayProtected() && _sig == WithoutSignature );

        input = s.out();
        if ( m_txType != TransactionType::Legacy )
            input.insert( input.begin(), m_txType );
    } else {
        RLP data( *m_rawData );
        input = dev::bytes( data.payload().begin(), data.payload().end() );
    }

    auto ret = dev::sha3( input );
    if ( _sig == WithSignature )
        m_hashWith = ret;
    return ret;
}

u256 TransactionBase::gasPrice() const {
    CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot get gas price." );
    return m_gasPrice;
}

u256 TransactionBase::gas() const {
    /* Note that gas() function has been removed from Transaction.
     * instead the logic has been moved to the gas() function of TransactionBase
     * this has been done in order to address the problem of switching "virtual" on/off
     */
    CHECK_STATE2( !isInvalid(), "Transaction is invalid. Cannot get gas." );
#ifdef FAIR
    return m_gas;
#else
    if ( getExternalGas() != 0 ) {
        return getExternalGas();
    } else {
        return m_gas;
    }
#endif
}

#ifndef FAIR
u256 TransactionBase::nonPowGas() const {
    return m_gas;
}
#endif

bytesConstRef dev::eth::bytesRefFromTransactionRlp( const RLP& _rlp ) {
    if ( _rlp.isList() )
        return _rlp.data();
    else
        return _rlp.payload();
}

#ifdef BITE
bytes const& TransactionBase::decryptedData() const {
    if ( !m_decryptedData )
        return data();
    return *m_decryptedData;
}

void TransactionBase::checkIfBITETxnAndSet( const Address& _to ) {
    m_isBITETxn = _to == BITE_ADDRESS;
}

Address TransactionBase::decryptedTo() const {
    if ( !m_decryptedTo )
        return to();
    return *m_decryptedTo;
}

void TransactionBase::checkAndValidateBITETransaction( uint64_t _currentEpochId ) const {
    if ( !isBite() )
        return;

    RLP rlpEncodedBITETxn;
    try {
        try {
            rlpEncodedBITETxn = RLP( m_data );
        } catch ( ... ) {
            BOOST_THROW_EXCEPTION( InvalidBITETransaction() << errinfo_comment( std::string(
                                       "BITE transaction's data must be RLP encoded" ) ) );
        }

        // RLP structure: [epochId1, encryptedBITEData]
        // encryptedBITEData may optionally have 1 or 2 encrypted AES keys assosiated with it

        if ( !rlpEncodedBITETxn.isList() )
            BOOST_THROW_EXCEPTION(
                InvalidBITETransaction() << errinfo_comment(
                    std::string( "BITE transaction's data is invalid: RLP must be a list" ) ) );

        if ( rlpEncodedBITETxn.itemCount() != 2 )
            BOOST_THROW_EXCEPTION( InvalidBITETransaction() << errinfo_comment(
                                       std::string( "BITE transaction's data is invalid: RLP list "
                                                    "should have exactly 2 elements, got: " ) +
                                       std::to_string( rlpEncodedBITETxn.itemCount() ) ) );

        // read encrypted data
        dev::bytes encryptedBITEData = rlpEncodedBITETxn[1].toBytes();
        if ( encryptedBITEData.size() < BITE_CIPHERTEXT_MIN_LEN )
            BOOST_THROW_EXCEPTION(
                BITETransactionTooShort() << errinfo_comment(
                    std::string( "BITE transaction's data size must be at least " ) +
                    std::to_string( BITE_CIPHERTEXT_MIN_LEN ) + std::string( ", got " ) +
                    std::to_string( encryptedBITEData.size() ) ) );

        // read epochId
        if ( !rlpEncodedBITETxn[0].isInt() )
            BOOST_THROW_EXCEPTION(
                InvalidBITETransaction() << errinfo_comment(
                    std::string( "BITE transaction's data is invalid: epochId must be an int" ) ) );
        uint64_t epochIdCandidate = rlpEncodedBITETxn[0].toInt< uint64_t >();
        // if a txn was sent before rotation it may have previous epochId: currentEpochId - 1
        if ( _currentEpochId != epochIdCandidate && _currentEpochId != epochIdCandidate + 1 )
            BOOST_THROW_EXCEPTION( InvalidBITETransaction() << errinfo_comment(
                                       std::string( "BITE transaction's data is invalid: no "
                                                    "payload found with matching epochId " ) +
                                       std::to_string( _currentEpochId ) ) );

        try {
            // check that ciphertext is valid
            libBLS::Ciphertext ciphertext = libBLS::Ciphertext::fromBytes( encryptedBITEData );
            // if currentEpochId = epochIdCandidate + 1, then ciphertext must have
            // 2 encrypted AES keys associated with it
            if ( epochIdCandidate != _currentEpochId && ciphertext.getKeys().size() != 2 )
                BOOST_THROW_EXCEPTION( InvalidBITETransaction() << errinfo_comment(
                                           std::string( "BITE transaction's data is invalid: no "
                                                        "payload found with matching epochId " ) +
                                           std::to_string( _currentEpochId ) ) );
            // validate encrypted AES keys
            for ( const auto& cipheredKey : ciphertext.getKeys() )
                libBLS::ThresholdEncryption::validateEncryption( cipheredKey );
        } catch ( libBLS::ThresholdUtils::IncorrectInput& ex ) {
            BOOST_THROW_EXCEPTION(
                InvalidBITETransaction() << errinfo_comment(
                    std::string( "BITE transaction's data is invalid: " ) + ex.what() ) );
        } catch ( libBLS::ThresholdUtils::IsNotWellFormed& ex ) {
            BOOST_THROW_EXCEPTION(
                InvalidBITETransaction() << errinfo_comment(
                    std::string( "BITE transaction's data is invalid: " ) + ex.what() ) );
        }
    } catch ( const Exception& _e ) {
        BOOST_LOG( m_loggerDebug )
            << std::string( "invalid BITE data format: " ) << std::string( _e.what() );
        throw;
    }
}
#endif
