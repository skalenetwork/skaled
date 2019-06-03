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

using namespace std;
using namespace dev;
using namespace dev::eth;

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
}

TransactionBase::TransactionBase(
    bytesConstRef _rlpData, CheckTransaction _checkSig, bool _allowInvalid ) {
    MICROPROFILE_SCOPEI( "TransactionBase", "ctor", MP_GOLD2 );
    try {
        RLP const rlp( _rlpData );
        try {
            if ( !rlp.isList() )
                BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                       << errinfo_comment( "transaction RLP must be a list" ) );

            m_nonce = rlp[0].toInt< u256 >();
            m_gasPrice = rlp[1].toInt< u256 >();
            m_gas = rlp[2].toInt< u256 >();
            m_type = rlp[3].isEmpty() ? ContractCreation : MessageCall;
            m_receiveAddress =
                rlp[3].isEmpty() ? Address() : rlp[3].toHash< Address >( RLP::VeryStrict );
            m_value = rlp[4].toInt< u256 >();

            if ( !rlp[5].isData() )
                BOOST_THROW_EXCEPTION( InvalidTransactionFormat() << errinfo_comment(
                                           "transaction data RLP must be an array" ) );

            m_data = rlp[5].toBytes();

            int const v = rlp[6].toInt< int >();
            h256 const r = rlp[7].toInt< u256 >();
            h256 const s = rlp[8].toInt< u256 >();

            if ( isZeroSignature( r, s ) ) {
                m_chainId = v;
                m_vrs = SignatureStruct{r, s, 0};
            } else {
                if ( v > 36 )
                    m_chainId = ( v - 35 ) / 2;
                else if ( v == 27 || v == 28 )
                    m_chainId = -4;
                else
                    BOOST_THROW_EXCEPTION( InvalidSignature() );

                m_vrs = SignatureStruct{r, s, static_cast< _byte_ >( v - ( m_chainId * 2 + 35 ) )};

                if ( _checkSig >= CheckTransaction::Cheap && !m_vrs->isValid() )
                    BOOST_THROW_EXCEPTION( InvalidSignature() );
            }

            if ( _checkSig == CheckTransaction::Everything )
                m_sender = sender();

            if ( rlp.itemCount() > 9 )
                BOOST_THROW_EXCEPTION( InvalidTransactionFormat() << errinfo_comment(
                                           "too many fields in the transaction RLP" ) );
            // XXX Strange "catch"-s %)
        } catch ( Exception& _e ) {
            _e << errinfo_name(
                "invalid transaction format: " + toString( rlp ) + " RLP: " + toHex( rlp.data() ) );
            m_type = Type::Invalid;
            m_rawData = _rlpData.toBytes();

            if ( !_allowInvalid )
                throw;
            else
                cwarn << _e.what();
        }
    } catch ( ... ) {
        m_type = Type::Invalid;
        RLPStream s;
        s.append( _rlpData.toBytes() );  // add "string" header
        m_rawData = s.out();

        if ( !_allowInvalid )
            throw;
    }
}  // ctor

Address const& TransactionBase::safeSender() const noexcept {
    assert( !isInvalid() );

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
    assert( !isInvalid() );

    auto sig = dev::sign( _priv, sha3( WithoutSignature ) );
    SignatureStruct sigStruct = *( SignatureStruct const* ) &sig;
    if ( sigStruct.isValid() )
        m_vrs = sigStruct;
}

void TransactionBase::streamRLP( RLPStream& _s, IncludeSignature _sig, bool _forEip155hash ) const {
    if ( isInvalid() ) {
        _s.appendRaw( m_rawData );
        return;
    }

    if ( m_type == NullTransaction )
        return;

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
            _s << m_chainId;
        else {
            int const vOffset = m_chainId * 2 + 35;
            _s << ( m_vrs->v + vOffset );
        }
        _s << ( u256 ) m_vrs->r << ( u256 ) m_vrs->s;
    } else if ( _forEip155hash )
        _s << m_chainId << 0 << 0;
}

static const u256 c_secp256k1n(
    "115792089237316195423570985008687907852837564279074904382605163141518161494337" );

void TransactionBase::checkLowS() const {
    if ( !m_vrs )
        BOOST_THROW_EXCEPTION( TransactionIsUnsigned() );

    if ( m_vrs->s > c_secp256k1n / 2 )
        BOOST_THROW_EXCEPTION( InvalidSignature() );
}

void TransactionBase::checkChainId( int chainId ) const {
    if ( m_chainId != chainId && m_chainId != -4 )
        BOOST_THROW_EXCEPTION( InvalidSignature() );
}

int64_t TransactionBase::baseGasRequired(
    bool _contractCreation, bytesConstRef _data, EVMSchedule const& _es ) {
    int64_t g = _contractCreation ? _es.txCreateGas : _es.txGas;

    // Calculate the cost of input data.
    // No risk of overflow by using int64 until txDataNonZeroGas is quite small
    // (the value not in billions).
    for ( auto i : _data )
        g += i ? _es.txDataNonZeroGas : _es.txDataZeroGas;
    return g;
}

h256 TransactionBase::sha3( IncludeSignature _sig ) const {
    if ( _sig == WithSignature && m_hashWith )
        return m_hashWith;

    MICROPROFILE_SCOPEI( "TransactionBase", "sha3", MP_KHAKI2 );

    RLPStream s;
    streamRLP( s, _sig, m_chainId > 0 && _sig == WithoutSignature );

    auto ret = dev::sha3( s.out() );
    if ( _sig == WithSignature )
        m_hashWith = ret;
    return ret;
}

u256 TransactionBase::gasPrice() const {
    assert( !isInvalid() );
    return m_gasPrice;
}

u256 TransactionBase::gas() const {
    assert( !isInvalid() );
    return m_gas;
}
