/*
    Copyright (C) 2026 SKALE Labs

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
 * @file P256.cpp
 * EIP-7951 implementation on top of OpenSSL low-level EC arithmetic.
 *
 * The verification steps are performed explicitly (instead of calling ECDSA_do_verify) so that
 * every check required by EIP-7951 is visible here and does not depend on OpenSSL's ECDSA
 * semantics (digest truncation, DER encoding, key validation policy).
 */

#include <libdevcrypto/P256.h>

#include <libdevcrypto/Exceptions.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/obj_mac.h>

#include <memory>

using namespace std;
using namespace dev;
using namespace dev::crypto;

namespace {

constexpr size_t FIELD_SIZE = 32;
constexpr size_t INPUT_SIZE = 5 * FIELD_SIZE;

using BnCtxPtr = unique_ptr< BN_CTX, decltype( &BN_CTX_free ) >;
using EcPointPtr = unique_ptr< EC_POINT, decltype( &EC_POINT_free ) >;

/// OpenSSL calls on valid data fail only on internal errors (e.g. out of memory).
void ensure( bool _ok ) {
    if ( !_ok )
        BOOST_THROW_EXCEPTION( CryptoException() << errinfo_comment( "P256 internal error" ) );
}

struct P256Group {
    P256Group() {
        ensure( group && p );
        ensure( EC_GROUP_get_curve( group.get(), p.get(), nullptr, nullptr, nullptr ) == 1 );
        n = EC_GROUP_get0_order( group.get() );
    }

    unique_ptr< EC_GROUP, decltype( &EC_GROUP_free ) > group{
        EC_GROUP_new_by_curve_name( NID_X9_62_prime256v1 ), &EC_GROUP_free
    };
    unique_ptr< BIGNUM, decltype( &BN_free ) > p{ BN_new(), &BN_free };
    BIGNUM const* n = nullptr;  // owned by group
};

// EC_GROUP is never modified after creation, so it can be shared between threads
P256Group const& p256() {
    static P256Group const s_p256;
    return s_p256;
}

BIGNUM* bnFromInput( bytesConstRef _in, size_t _index, BN_CTX* _ctx ) {
    BIGNUM* ret = BN_CTX_get( _ctx );
    ensure( ret && BN_bin2bn( _in.data() + _index * FIELD_SIZE, FIELD_SIZE, ret ) );
    return ret;
}

bool isInRange( BIGNUM const* _x, BIGNUM const* _upper ) {
    return !BN_is_negative( _x ) && BN_cmp( _x, _upper ) < 0;
}

bool verify( bytesConstRef _in ) {
    if ( _in.size() != INPUT_SIZE )
        return false;

    P256Group const& curve = p256();
    EC_GROUP const* group = curve.group.get();

    BnCtxPtr ctx( BN_CTX_new(), &BN_CTX_free );
    ensure( ctx != nullptr );
    BN_CTX_start( ctx.get() );
    // BN_CTX_end must be called before ctx is freed
    unique_ptr< BN_CTX, decltype( &BN_CTX_end ) > frame( ctx.get(), &BN_CTX_end );

    BIGNUM* h = bnFromInput( _in, 0, ctx.get() );
    BIGNUM* r = bnFromInput( _in, 1, ctx.get() );
    BIGNUM* s = bnFromInput( _in, 2, ctx.get() );
    BIGNUM* qx = bnFromInput( _in, 3, ctx.get() );
    BIGNUM* qy = bnFromInput( _in, 4, ctx.get() );

    // 0 < r < n, 0 < s < n
    if ( BN_is_zero( r ) || !isInRange( r, curve.n ) || BN_is_zero( s ) ||
         !isInRange( s, curve.n ) )
        return false;

    // 0 <= qx < p, 0 <= qy < p
    if ( !isInRange( qx, curve.p.get() ) || !isInRange( qy, curve.p.get() ) )
        return false;

    // Q must be on the curve. This also rejects the point at infinity encoded as (0, 0),
    // because b != 0 for P-256.
    EcPointPtr q( EC_POINT_new( group ), &EC_POINT_free );
    ensure( q != nullptr );
    if ( !EC_POINT_set_affine_coordinates( group, q.get(), qx, qy, ctx.get() ) ||
         EC_POINT_is_on_curve( group, q.get(), ctx.get() ) != 1 )
        return false;

    // s1 = s^-1 mod n; u1 = h * s1 mod n; u2 = r * s1 mod n
    BIGNUM* s1 = BN_CTX_get( ctx.get() );
    BIGNUM* u1 = BN_CTX_get( ctx.get() );
    BIGNUM* u2 = BN_CTX_get( ctx.get() );
    // BN_CTX_get returns NULL for all subsequent calls once one fails, so checking u2 is enough
    ensure( u2 && BN_mod_inverse( s1, s, curve.n, ctx.get() ) &&
            BN_mod_mul( u1, h, s1, curve.n, ctx.get() ) &&
            BN_mod_mul( u2, r, s1, curve.n, ctx.get() ) );

    // R' = u1 * G + u2 * Q
    EcPointPtr rPoint( EC_POINT_new( group ), &EC_POINT_free );
    ensure( rPoint && EC_POINT_mul( group, rPoint.get(), u1, q.get(), u2, ctx.get() ) );
    if ( EC_POINT_is_at_infinity( group, rPoint.get() ) )
        return false;

    // R'.x mod n == r
    BIGNUM* x = BN_CTX_get( ctx.get() );
    ensure( x && EC_POINT_get_affine_coordinates( group, rPoint.get(), x, nullptr, ctx.get() ) &&
            BN_nnmod( x, x, curve.n, ctx.get() ) );
    return BN_cmp( x, r ) == 0;
}

}  // namespace

pair< bool, bytes > dev::crypto::p256Verify( bytesConstRef _in ) {
    if ( verify( _in ) )
        return { true, h256( 1 ).asBytes() };
    // Rejected inputs leave entries in the thread-local OpenSSL error queue; don't leak them
    // into unrelated OpenSSL users on the same thread.
    ERR_clear_error();
    return { true, {} };
}
