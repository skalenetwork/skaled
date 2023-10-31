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
            if ( !rlp[3].isData() )
                BOOST_THROW_EXCEPTION( InvalidTransactionFormat()
                                       << errinfo_comment( "recepient RLP must be a byte array" ) );
            m_type = rlp[3].isEmpty() ? ContractCreation : MessageCall;
            m_receiveAddress =
                rlp[3].isEmpty() ? Address() : rlp[3].toHash< Address >( RLP::VeryStrict );
            m_value = rlp[4].toInt< u256 >();

            if ( !rlp[5].isData() )
                BOOST_THROW_EXCEPTION( InvalidTransactionFormat() << errinfo_comment(
                                           "transaction data RLP must be an array" ) );

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
                } else if ( v != 27 && v != 28 )
                    BOOST_THROW_EXCEPTION( InvalidSignature() );
                // else leave m_chainId as is (unitialized)

                auto const recoveryID = m_chainId.has_value() ?
                                            _byte_{ v - ( u256{ *m_chainId } * 2 + 35 ) } :
                                            _byte_{ v - 27 };
                m_vrs = SignatureStruct{ r, s, recoveryID };

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
            else {
                cwarn << _e.what();
            }
        }
    } catch ( ... ) {
        m_type = Type::Invalid;
        RLPStream s;
        s.append( _rlpData.toBytes() );  // add "string" header
        m_rawData = s.out();

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
            _s << ( m_chainId.has_value() ? *m_chainId : 0 );
        else {
            uint64_t const vOffset = m_chainId.has_value() ? *m_chainId * 2 + 35 : 27;
            _s << ( m_vrs->v + vOffset );
        }
        _s << ( u256 ) m_vrs->r << ( u256 ) m_vrs->s;
    } else if ( _forEip155hash )
        _s << *m_chainId << 0 << 0;
}

static const u256 c_secp256k1n(
    "115792089237316195423570985008687907852837564279074904382605163141518161494337" );

void TransactionBase::checkLowS() const {
    if ( !m_vrs )
        BOOST_THROW_EXCEPTION( TransactionIsUnsigned() );

    if ( m_vrs->s > c_secp256k1n / 2 )
        BOOST_THROW_EXCEPTION( InvalidSignature() );
}

void TransactionBase::checkChainId( uint64_t chainId, bool disableChainIdCheck ) const {
    if ( !disableChainIdCheck ) {
        if ( !m_chainId.has_value() ) {
            BOOST_THROW_EXCEPTION( InvalidTransactionFormat() );
        }
    }
    if ( m_chainId.has_value() && m_chainId != chainId )
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
    streamRLP( s, _sig, !isInvalid() && isReplayProtected() && _sig == WithoutSignature );

    auto ret = dev::sha3( s.out() );
    if ( _sig == WithSignature )
        m_hashWith = ret;
    return ret;
}

u256 TransactionBase::gasPrice() const {
    assert( !isInvalid() );
    return m_gasPrice;
}

// we start from 2.1.1 stable branch and add changes to address the "virtual" problem
// since "virtual" can not be dynamically disabled/enabled we remove
// the gas() function from Transaction and move the logic to TransactionBase
// this pull request does not include reading isPatchEnabled from the config file
bool isPatchEnabled = false;

u256 TransactionBase::gas() const {
    /* Note that gas() function has been removed from Transaction.
     * instead the logic has been moved to the gas() function of TransactionBase
     * this has been done in order to address the problem of switching "virtual" on/off

     *
     * the code has been also simplified since there were redundant checks.
     * One can check that

     m_externalGasIsChecked && hasExternalGas()

     * is mathematically identical to

     getExternalGas() != 0

     *
     *
     * it is easy to prove that when isPatchEnabled is false the below code
     * is mathematically identical to the code that existed in the 2.1.1 implementation.
     * if the patch is enabled the code corresponds to the "virtual" patch
     * For block gas limit, this function will return the mined gas
     * Note that we still have a problem that "accidentally mined transactions" can fail,
     * but it can be fixed for 2.3
     * If we want to solve the problem of accidentally mined transaction we can use something like

     if ( getExternalGas() != 0 && isPatchEnabled && getExternalGas > m_gas) {
        return getExternalGas();
     } else {
        return m_gas;
     }

     */
    assert( !isInvalid() );
    if ( getExternalGas() != 0 && isPatchEnabled ) {
        return getExternalGas();
    } else {
        return m_gas;
    }
}
