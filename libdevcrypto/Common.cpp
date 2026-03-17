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
/** @file Common.cpp
 * @author Alex Leverington <nessence@gmail.com>
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Common.h"
#include "AES.h"
#include "CryptoPP.h"
#include "Exceptions.h"
#include "Hash.h"

#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>
#include <libdevcore/Guards.h>  // <boost/thread> conflicts with <thread>
#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libscrypt.h>
#include <secp256k1.h>
#include <secp256k1_ecdh.h>
#include <secp256k1_recovery.h>
#include <secp256k1_sha256.h>

#include <libdevcore/microprofile.h>

using namespace std;
using namespace dev;
using namespace dev::crypto;

namespace {

secp256k1_context const* getCtx() {
    static std::unique_ptr< secp256k1_context, decltype( &secp256k1_context_destroy ) > s_ctx{
        secp256k1_context_create( SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY ),
        &secp256k1_context_destroy
    };
    return s_ctx.get();
}

}  // namespace

bool dev::SignatureStruct::isValid() const noexcept {
    static const h256 s_max{ "0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141" };
    static const h256 s_zero;

    return ( v <= 1 && r > s_zero && s > s_zero && r < s_max && s < s_max );
}

Public dev::toPublic( Secret const& _secret ) {
    auto* ctx = getCtx();
    secp256k1_pubkey rawPubkey;
    // Creation will fail if the secret key is invalid.
    if ( !secp256k1_ec_pubkey_create( ctx, &rawPubkey, _secret.data() ) )
        return {};
    std::array< _byte_, 65 > serializedPubkey;
    size_t serializedPubkeySize = serializedPubkey.size();
    secp256k1_ec_pubkey_serialize( ctx, serializedPubkey.data(), &serializedPubkeySize, &rawPubkey,
        SECP256K1_EC_UNCOMPRESSED );
    assert( serializedPubkeySize == serializedPubkey.size() );
    // Expect single byte header of value 0x04 -- uncompressed public key.
    assert( serializedPubkey[0] == 0x04 );
    // Create the Public skipping the header.
    return Public{ &serializedPubkey[1], Public::ConstructFromPointer };
}

Address dev::toAddress( Public const& _public ) {
    return right160( sha3( _public.ref() ) );
}

Address dev::toAddress( Secret const& _secret ) {
    return toAddress( toPublic( _secret ) );
}

Address dev::toAddress( Address const& _from, u256 const& _nonce ) {
    return right160( sha3( rlpList( _from, _nonce ) ) );
}

void dev::encrypt( Public const& _k, bytesConstRef _plain, bytes& o_cipher ) {
    bytes io = _plain.toBytes();
    Secp256k1PP::get()->encrypt( _k, io );
    o_cipher = std::move( io );
}

bool dev::decrypt( Secret const& _k, bytesConstRef _cipher, bytes& o_plaintext ) {
    bytes io = _cipher.toBytes();
    Secp256k1PP::get()->decrypt( _k, io );
    if ( io.empty() )
        return false;
    o_plaintext = std::move( io );
    return true;
}

void dev::encryptECIES( Public const& _k, bytesConstRef _plain, bytes& o_cipher ) {
    encryptECIES( _k, bytesConstRef(), _plain, o_cipher );
}

void dev::encryptECIES(
    Public const& _k, bytesConstRef _sharedMacData, bytesConstRef _plain, bytes& o_cipher ) {
    bytes io = _plain.toBytes();
    Secp256k1PP::get()->encryptECIES( _k, _sharedMacData, io );
    o_cipher = std::move( io );
}

bool dev::decryptECIES( Secret const& _k, bytesConstRef _cipher, bytes& o_plaintext ) {
    return decryptECIES( _k, bytesConstRef(), _cipher, o_plaintext );
}

bool dev::decryptECIES(
    Secret const& _k, bytesConstRef _sharedMacData, bytesConstRef _cipher, bytes& o_plaintext ) {
    bytes io = _cipher.toBytes();
    if ( !Secp256k1PP::get()->decryptECIES( _k, _sharedMacData, io ) )
        return false;
    o_plaintext = std::move( io );
    return true;
}

void dev::encryptSym( Secret const& _k, bytesConstRef _plain, bytes& o_cipher ) {
    // TODO: @alex @subtly do this properly.
    encrypt( KeyPair( _k ).pub(), _plain, o_cipher );
}

bool dev::decryptSym( Secret const& _k, bytesConstRef _cipher, bytes& o_plain ) {
    // TODO: @alex @subtly do this properly.
    return decrypt( _k, _cipher, o_plain );
}

std::pair< bytes, h128 > dev::encryptSymNoAuth(
    SecureFixedHash< 16 > const& _k, bytesConstRef _plain ) {
    h128 iv( Nonce::get().makeInsecure() );
    return make_pair( encryptSymNoAuth( _k, iv, _plain ), iv );
}

bytes dev::encryptAES128CTR( bytesConstRef _k, h128 const& _iv, bytesConstRef _plain ) {
    if ( _k.size() != 16 && _k.size() != 24 && _k.size() != 32 )
        return bytes();
    CryptoPP::SecByteBlock key( _k.data(), _k.size() );
    try {
        CryptoPP::CTR_Mode< CryptoPP::AES >::Encryption e;
        e.SetKeyWithIV( key, key.size(), _iv.data() );
        bytes ret( _plain.size() );
        e.ProcessData( ret.data(), _plain.data(), _plain.size() );
        return ret;
    } catch ( CryptoPP::Exception& _e ) {
        cerror << "Error in encryptAES128CTR()" << _e.what();
        return bytes();
    }
}

bytesSec dev::decryptAES128CTR( bytesConstRef _k, h128 const& _iv, bytesConstRef _cipher ) {
    if ( _k.size() != 16 && _k.size() != 24 && _k.size() != 32 )
        return bytesSec();
    CryptoPP::SecByteBlock key( _k.data(), _k.size() );
    try {
        CryptoPP::CTR_Mode< CryptoPP::AES >::Decryption d;
        d.SetKeyWithIV( key, key.size(), _iv.data() );
        bytesSec ret( _cipher.size() );
        d.ProcessData( ret.writable().data(), _cipher.data(), _cipher.size() );
        return ret;
    } catch ( CryptoPP::Exception& _e ) {
        cerror << "Error in decryptAES128CTR()" << _e.what();
        return bytesSec();
    }
}

Public dev::recover( Signature const& _sig, h256 const& _message ) {
    MICROPROFILE_SCOPEI( "Common.cpp", "recover", MP_BROWN1 );

    int v = _sig[64];
    if ( v > 3 )
        return {};

    auto* ctx = getCtx();
    secp256k1_ecdsa_recoverable_signature rawSig;
    if ( !secp256k1_ecdsa_recoverable_signature_parse_compact( ctx, &rawSig, _sig.data(), v ) )
        return {};

    secp256k1_pubkey rawPubkey;
    if ( !secp256k1_ecdsa_recover( ctx, &rawPubkey, &rawSig, _message.data() ) )
        return {};

    std::array< _byte_, 65 > serializedPubkey;
    size_t serializedPubkeySize = serializedPubkey.size();
    secp256k1_ec_pubkey_serialize( ctx, serializedPubkey.data(), &serializedPubkeySize, &rawPubkey,
        SECP256K1_EC_UNCOMPRESSED );
    assert( serializedPubkeySize == serializedPubkey.size() );
    // Expect single byte header of value 0x04 -- uncompressed public key.
    assert( serializedPubkey[0] == 0x04 );
    // Create the Public skipping the header.
    return Public{ &serializedPubkey[1], Public::ConstructFromPointer };
}

static const u256 c_secp256k1n(
    "115792089237316195423570985008687907852837564279074904382605163141518161494337" );

Signature dev::sign( Secret const& _k, h256 const& _hash ) {
    auto* ctx = getCtx();
    secp256k1_ecdsa_recoverable_signature rawSig;
    if ( !secp256k1_ecdsa_sign_recoverable(
             ctx, &rawSig, _hash.data(), _k.data(), nullptr, nullptr ) )
        return {};

    Signature s;
    int v = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact( ctx, s.data(), &v, &rawSig );

    SignatureStruct& ss = *reinterpret_cast< SignatureStruct* >( &s );
    ss.v = static_cast< _byte_ >( v );
    if ( ss.s > c_secp256k1n / 2 ) {
        ss.v = static_cast< _byte_ >( ss.v ^ 1 );
        ss.s = h256( c_secp256k1n - u256( ss.s ) );
    }
    assert( ss.s <= c_secp256k1n / 2 );
    return s;
}

bool dev::verify( Public const& _p, Signature const& _s, h256 const& _hash ) {
    // TODO: Verify w/o recovery (if faster).
    if ( !_p )
        return false;
    return _p == recover( _s, _hash );
}

bytesSec dev::pbkdf2(
    string const& _pass, bytes const& _salt, unsigned _iterations, unsigned _dkLen ) {
    bytesSec ret( _dkLen );
    if ( CryptoPP::PKCS5_PBKDF2_HMAC< CryptoPP::SHA256 >().DeriveKey( ret.writable().data(), _dkLen,
             0, reinterpret_cast< _byte_ const* >( _pass.data() ), _pass.size(), _salt.data(),
             _salt.size(), _iterations ) != _iterations )
        BOOST_THROW_EXCEPTION( CryptoException() << errinfo_comment( "Key derivation failed." ) );
    return ret;
}

bytesSec dev::scrypt( std::string const& _pass, bytes const& _salt, uint64_t _n, uint32_t _r,
    uint32_t _p, unsigned _dkLen ) {
    bytesSec ret( _dkLen );
    if ( libscrypt_scrypt( reinterpret_cast< uint8_t const* >( _pass.data() ), _pass.size(),
             _salt.data(), _salt.size(), _n, _r, _p, ret.writable().data(), _dkLen ) != 0 )
        BOOST_THROW_EXCEPTION( CryptoException() << errinfo_comment( "Key derivation failed." ) );
    return ret;
}

KeyPair::KeyPair( Secret const& _sec ) : m_secret( _sec ), m_public( toPublic( _sec ) ) {
    // Assign address only if the secret key is valid.
    if ( m_public )
        m_address = toAddress( m_public );
}

KeyPair KeyPair::create() {
    while ( true ) {
        KeyPair keyPair( Secret::random() );
        if ( keyPair.address() )
            return keyPair;
    }
}

KeyPair KeyPair::fromEncryptedSeed( bytesConstRef _seed, std::string const& _password ) {
    return KeyPair( Secret( sha3( aesDecrypt( _seed, _password ) ) ) );
}

h256 crypto::kdf( Secret const& _priv, h256 const& _hash ) {
    // H(H(r||k)^h)
    h256 s;
    sha3mac( Secret::random().ref(), _priv.ref(), s.ref() );
    s ^= _hash;
    sha3( s.ref(), s.ref() );

    if ( !s || !_hash || !_priv )
        BOOST_THROW_EXCEPTION( InvalidState() );
    return s;
}

Secret Nonce::next() {
    Guard l( x_value );
    if ( !m_value ) {
        m_value = Secret::random();
        if ( !m_value )
            BOOST_THROW_EXCEPTION( InvalidState() );
    }
    m_value = sha3Secure( m_value.ref() );
    return sha3( ~m_value );
}

bool ecdh::agree( Secret const& _s, Public const& _r, Secret& o_s ) noexcept {
    auto* ctx = getCtx();
    static_assert( sizeof( Secret ) == 32, "Invalid Secret type size" );
    secp256k1_pubkey rawPubkey;
    std::array< _byte_, 65 > serializedPubKey{ { 0x04 } };
    std::copy( _r.asArray().begin(), _r.asArray().end(), serializedPubKey.begin() + 1 );
    if ( !secp256k1_ec_pubkey_parse(
             ctx, &rawPubkey, serializedPubKey.data(), serializedPubKey.size() ) )
        return false;  // Invalid public key.
    // FIXME: We should verify the public key when constructed, maybe even keep
    //        secp256k1_pubkey as the internal data of Public.
    std::array< _byte_, 33 > compressedPoint;
    if ( !secp256k1_ecdh_raw( ctx, compressedPoint.data(), &rawPubkey, _s.data() ) )
        return false;  // Invalid secret key.
    std::copy( compressedPoint.begin() + 1, compressedPoint.end(), o_s.writable().data() );
    return true;
}

bytes ecies::kdf( Secret const& _z, bytes const& _s1, unsigned kdByteLen ) {
    auto reps = ( ( kdByteLen + 7 ) * 8 ) / 512;
    // SEC/ISO/Shoup specify counter size SHOULD be equivalent
    // to size of hash output, however, it also notes that
    // the 4 bytes is okay. NIST specifies 4 bytes.
    std::array< _byte_, 4 > ctr{ { 0, 0, 0, 1 } };
    bytes k;
    secp256k1_sha256_t ctx;
    for ( unsigned i = 0; i <= reps; i++ ) {
        secp256k1_sha256_initialize( &ctx );
        secp256k1_sha256_write( &ctx, ctr.data(), ctr.size() );
        secp256k1_sha256_write( &ctx, _z.data(), Secret::size );
        secp256k1_sha256_write( &ctx, _s1.data(), _s1.size() );
        // append hash to k
        std::array< _byte_, 32 > digest;
        secp256k1_sha256_finalize( &ctx, digest.data() );

        k.reserve( k.size() + h256::size );
        move( digest.begin(), digest.end(), back_inserter( k ) );

        if ( ++ctr[3] || ++ctr[2] || ++ctr[1] || ++ctr[0] )
            continue;
    }

    k.resize( kdByteLen );
    return k;
}

#ifdef BITE
// Check if x is a valid x-coordinate on secp256k1 curve (y² = x³ + 7 mod p)
bool dev::isValidSecp256k1X( const u256& x ) {
    static const u256 secp256k1P{
        "0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f"
    };

    // Calculate x³ + 7 mod p
    u256 xCubed = boost::multiprecision::powm( x, 3, secp256k1P );
    u256 ySquared = ( xCubed + 7 ) % secp256k1P;

    // Check if rhs is a quadratic residue (has a square root) using Legendre symbol
    // If (ySquared^((p-1)/2) mod p) == 1, then rhs is a quadratic residue
    u256 exponent = ( secp256k1P - 1 ) / 2;
    u256 legendre = boost::multiprecision::powm( ySquared, exponent, secp256k1P );

    return legendre == 1;
}

// Return (r,s,v) fabricated from entropy bytes and transaction index.
SignatureStruct dev::makeSignature( const bytes& entropy, const dev::u256& txIndex ) {
    static const u256 kSecp256k1_N{
        "0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141"
    };

    // Mix transaction index into entropy
    bytes combinedEntropy = entropy;
    // Convert u256 to bytes
    bytes txIndexBytes = dev::toBigEndian( txIndex );
    combinedEntropy.insert( combinedEntropy.end(), txIndexBytes.begin(), txIndexBytes.end() );

    // Hash combined entropy to expand it
    h256 h1 = dev::sha3( combinedEntropy );
    h256 h2 = dev::sha3( h1 );
    h256 h3 = dev::sha3( h1.asBytes() + h2.asBytes() );

    // Construct r - ensure it's a valid x-coordinate on secp256k1 curve
    u256 r = ( u256 ) h1;
    if ( r == 0 || r >= kSecp256k1_N )
        r = ( r % ( kSecp256k1_N - 1 ) ) + 1;

    // Keep incrementing r until we find a valid curve point
    size_t attempts = 0;
    while ( !dev::isValidSecp256k1X( r ) && attempts < 1000 ) {
        r = ( r + 1 ) % kSecp256k1_N;
        if ( r == 0 )
            r = 1;
        ++attempts;
    }

    // Construct s from second hash
    u256 s = ( u256 ) h2;
    if ( s == 0 || s >= kSecp256k1_N )
        s = ( s % ( kSecp256k1_N - 1 ) ) + 1;

    // Enforce “low s” (canonical form): if s > n/2 set s = n - s
    u256 halfN = kSecp256k1_N / 2;
    if ( s > halfN )
        s = kSecp256k1_N - s;

    uint8_t parity = ( uint8_t )( ( uint64_t ) h3[0] & 0x01 );

    return SignatureStruct( h256( r ), h256( s ), parity );
}

bytes dev::compressPublicKey( Public const& _pub ) {
    auto* ctx = getCtx();

    // Build uncompressed format: 0x04 || x || y (65 bytes)
    std::array< uint8_t, 65 > uncompressedPubKey;
    uncompressedPubKey[0] = 0x04;
    memcpy( uncompressedPubKey.data() + 1, _pub.data(), 64 );

    // Parse into secp256k1 internal format
    secp256k1_pubkey parsedPubKey;
    if ( !secp256k1_ec_pubkey_parse(
             ctx, &parsedPubKey, uncompressedPubKey.data(), uncompressedPubKey.size() ) ) {
        return {};  // Invalid public key
    }

    // Serialize to compressed format (33 bytes)
    bytes result( 33 );
    size_t outputLen = 33;
    secp256k1_ec_pubkey_serialize(
        ctx, result.data(), &outputLen, &parsedPubKey, SECP256K1_EC_COMPRESSED );

    return result;
}

Public dev::decompressPublicKey( bytesConstRef _compressed ) {
    if ( _compressed.size() != 33 )
        return {};

    auto* ctx = getCtx();

    // Parse compressed public key
    secp256k1_pubkey parsedPubKey;
    if ( !secp256k1_ec_pubkey_parse( ctx, &parsedPubKey, _compressed.data(), _compressed.size() ) )
        return {};

    // Serialize to uncompressed format (65 bytes: 0x04 prefix + 64 bytes)
    std::array< uint8_t, 65 > uncompressedPubKey;
    size_t outputLen = 65;
    secp256k1_ec_pubkey_serialize(
        ctx, uncompressedPubKey.data(), &outputLen, &parsedPubKey, SECP256K1_EC_UNCOMPRESSED );

    // Return the 64 bytes (skip the 0x04 prefix)
    return Public( &uncompressedPubKey[1], Public::ConstructFromPointer );
}

bool dev::isValidPublicKey( Public const& _pub ) {
    if ( !_pub )
        return false;

    auto* ctx = getCtx();

    // Build uncompressed format: 0x04 || x || y (65 bytes)
    std::array< uint8_t, 65 > uncompressedPubKey;
    uncompressedPubKey[0] = 0x04;
    memcpy( uncompressedPubKey.data() + 1, _pub.data(), 64 );

    // Try to parse - this validates it's on the curve
    secp256k1_pubkey parsedPubKey;
    return secp256k1_ec_pubkey_parse(
        ctx, &parsedPubKey, uncompressedPubKey.data(), uncompressedPubKey.size() );
}

bytes dev::encryptECIES_CBC(
    Public const& _recipientPubKey, bytesConstRef _plain, h256 const* _seed ) {
    // Note: Empty plaintext is allowed - with PKCS7 padding it produces a 16-byte padding block
    // This prevents distinguishing between empty and non-empty encrypted payloads

    KeyPair ephemeralKeyPair{ Secret() };
    h128 iv;

    if ( _seed ) {
        // Deterministic mode: derive ephemeral key and IV from seed with context strings
        // Derive ephemeral private key: SHA256(seed || "ECIES_EPHEMERAL_KEY" || counter)
        // Retry with incrementing counter until valid key is found
        // attempt 255 times
        h256 ephemeralPrivateKey;
        for ( uint8_t attempt = 0; attempt < 255; ++attempt ) {
            bytes seedWithKeyContext = _seed->asBytes();
            const std::string keyContext = "ECIES_EPHEMERAL_KEY";
            seedWithKeyContext.insert(
                seedWithKeyContext.end(), keyContext.begin(), keyContext.end() );
            seedWithKeyContext.push_back( attempt );  // Append counter for retries
            ephemeralPrivateKey = dev::sha256( bytesConstRef( &seedWithKeyContext ) );

            ephemeralKeyPair = KeyPair( Secret( ephemeralPrivateKey ) );
            if ( ephemeralKeyPair.pub() )
                break;  // Valid key found
        }
        if ( !ephemeralKeyPair.pub() ) {
            return {};  // Failed after all attempts (extremely unlikely)
        }

        // Derive IV: SHA256(seed || "ECIES_IV"), take first 16 bytes
        bytes seedWithIvContext = _seed->asBytes();
        const std::string ivContext = "ECIES_IV";
        seedWithIvContext.insert( seedWithIvContext.end(), ivContext.begin(), ivContext.end() );
        h256 ivHash = dev::sha256( bytesConstRef( &seedWithIvContext ) );
        iv = h128( ivHash.data(), h128::ConstructFromPointer );
    } else {
        // Random mode: generate ephemeral key pair and IV randomly
        ephemeralKeyPair = KeyPair::create();
        iv = h128::random();
    }

    // ECDH: shared secret = ephemeral_private * recipient_public
    Secret sharedSecret;
    if ( !crypto::ecdh::agree( ephemeralKeyPair.secret(), _recipientPubKey, sharedSecret ) )
        return {};

    // KDF: encryption_key = SHA-256(shared_secret)
    h256 encryptionKey = dev::sha256( sharedSecret.ref() );

    // AES-256-CBC encryption with PKCS7 padding
    CryptoPP::CBC_Mode< CryptoPP::AES >::Encryption aesEncryptor;
    aesEncryptor.SetKeyWithIV( encryptionKey.data(), encryptionKey.size, iv.data() );

    std::string ciphertextStr;
    CryptoPP::StreamTransformationFilter stfEncryptor(
        aesEncryptor, new CryptoPP::StringSink( ciphertextStr ) );
    stfEncryptor.Put( _plain.data(), _plain.size() );
    stfEncryptor.MessageEnd();

    // Compress ephemeral public key to 33 bytes
    bytes compressedEphPubKey = compressPublicKey( ephemeralKeyPair.pub() );
    if ( compressedEphPubKey.empty() )
        return {};

    // Output format: [IV(16)] [CompressedEphPubKey(33)] [Ciphertext]
    bytes result;
    result.reserve( 16 + 33 + ciphertextStr.size() );
    result.insert( result.end(), iv.begin(), iv.end() );
    result.insert( result.end(), compressedEphPubKey.begin(), compressedEphPubKey.end() );
    result.insert( result.end(), ciphertextStr.begin(), ciphertextStr.end() );

    return result;
}

bytes dev::decryptECIES_CBC( Secret const& _recipientPrivKey, bytesConstRef _cipher ) {
    // Minimum size: IV(16) + CompressedPubKey(33) + at least 1 block(16)
    static constexpr size_t MIN_CIPHER_SIZE = 16 + 33 + 16;
    if ( _cipher.size() < MIN_CIPHER_SIZE )
        return {};

    // Parse input: [IV(16)] [CompressedEphPubKey(33)] [Ciphertext]
    h128 iv( _cipher.cropped( 0, 16 ) );
    bytesConstRef compressedEphPubKey = _cipher.cropped( 16, 33 );
    bytesConstRef ciphertext = _cipher.cropped( 49 );

    // Decompress ephemeral public key
    Public ephemeralPubKey = decompressPublicKey( compressedEphPubKey );
    if ( !ephemeralPubKey )
        return {};

    // ECDH: shared secret = recipient_private * ephemeral_public
    Secret sharedSecret;
    if ( !crypto::ecdh::agree( _recipientPrivKey, ephemeralPubKey, sharedSecret ) )
        return {};

    // KDF: encryption_key = SHA-256(shared_secret)
    h256 encryptionKey = dev::sha256( sharedSecret.ref() );

    // AES-256-CBC decryption with PKCS7 unpadding
    CryptoPP::CBC_Mode< CryptoPP::AES >::Decryption aesDecryptor;
    aesDecryptor.SetKeyWithIV( encryptionKey.data(), encryptionKey.size, iv.data() );

    std::string plaintextStr;
    try {
        CryptoPP::StreamTransformationFilter stfDecryptor( aesDecryptor,
            new CryptoPP::StringSink( plaintextStr ),
            CryptoPP::BlockPaddingSchemeDef::PKCS_PADDING );
        stfDecryptor.Put( ciphertext.data(), ciphertext.size() );
        stfDecryptor.MessageEnd();
    } catch ( ... ) {
        return {};  // Decryption or padding error
    }

    return bytes( plaintextStr.begin(), plaintextStr.end() );
}

#endif
