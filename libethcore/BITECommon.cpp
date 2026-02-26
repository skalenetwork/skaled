#ifdef BITE

#include <libdevcore/RLP.h>
#include <libethcore/BITECommon.h>
#include <libethcore/Exceptions.h>

using namespace dev;
using namespace dev::eth;

namespace dev {

namespace bite {

void validateBITECiphertext(
    const dev::bytes& _ciphertext, const BITEVerificationData& _verificationData ) {
    RLP rlpEncodedBITETxn;
    try {
        try {
            rlpEncodedBITETxn = RLP( _ciphertext );
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
        if ( _verificationData.epochId != epochIdCandidate &&
             _verificationData.epochId != epochIdCandidate + 1 )
            BOOST_THROW_EXCEPTION( InvalidBITETransaction() << errinfo_comment(
                                       std::string( "BITE transaction's data is invalid: no "
                                                    "payload found with matching epochId " ) +
                                       std::to_string( _verificationData.epochId ) ) );

        try {
            // check that ciphertext is valid
            libBLS::Ciphertext ciphertext = libBLS::Ciphertext::fromBytes( encryptedBITEData );
            // if currentEpochId = epochIdCandidate + 1, then ciphertext must have
            // 2 encrypted AES keys associated with it
            if ( epochIdCandidate != _verificationData.epochId && ciphertext.getKeys().size() != 2 )
                BOOST_THROW_EXCEPTION( InvalidBITETransaction() << errinfo_comment(
                                           std::string( "BITE transaction's data is invalid: no "
                                                        "payload found with matching epochId " ) +
                                           std::to_string( _verificationData.epochId ) ) );
            // validate encrypted AES keys
            for ( const auto& cipheredKey : ciphertext.getKeys() )
                libBLS::ThresholdEncryption::validateEncryption(
                    cipheredKey, &_verificationData.aadTE );
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
        throw;
    }
}

#ifdef BITE2
dev::bytes constructDecryptedCTXData(
    const dev::bytes& _txData, const DecryptedCTXArgs& _decryptedCTXArgs ) {
    // Transform _txData from: selector(4 bytes) + RLP(RLP(encrypted_args), RLP(plaintext_args))
    // to: selector(4 bytes) + abi.encode(bytes[] decrypted_args, bytes[] plaintext_args)
    // Store result in m_decryptedData

    if ( _txData.size() < 4 )
        BOOST_THROW_EXCEPTION( InvalidBITETransaction() << errinfo_comment(
                                   "CTX transaction data too short - missing function selector" ) );

    // Extract function selector (first 4 bytes)
    dev::bytes functionSelector( _txData.begin(), _txData.begin() + 4 );

    // Parse RLP structure from remaining data
    dev::bytes rlpData( _txData.begin() + 4, _txData.end() );
    RLP rlp( rlpData );

    if ( !rlp.isList() || rlp.itemCount() != 2 )
        BOOST_THROW_EXCEPTION( InvalidBITETransaction() << errinfo_comment(
                                   "CTX transaction data must contain RLP list with 2 elements" ) );

    // Parse encrypted args array (first RLP list)
    RLP encryptedArgsRlp = rlp[0];
    if ( !encryptedArgsRlp.isList() )
        BOOST_THROW_EXCEPTION( InvalidBITETransaction()
                               << errinfo_comment( "CTX encrypted args must be an RLP list" ) );

    if ( encryptedArgsRlp.itemCount() != _decryptedCTXArgs.args.size() )
        BOOST_THROW_EXCEPTION( InvalidBITETransaction() << errinfo_comment(
                                   "CTX decrypted args count mismatch: expected " +
                                   std::to_string( encryptedArgsRlp.itemCount() ) + ", got " +
                                   std::to_string( _decryptedCTXArgs.args.size() ) ) );

    // Parse plaintext args array (second RLP list)
    RLP plaintextArgsRlp = rlp[1];
    if ( !plaintextArgsRlp.isList() )
        BOOST_THROW_EXCEPTION( InvalidBITETransaction()
                               << errinfo_comment( "CTX plaintext args must be an RLP list" ) );

    // Build new RLP structure with decrypted args
    RLPStream decryptedArgsStream;
    decryptedArgsStream.appendList( _decryptedCTXArgs.args.size() );
    for ( const auto& decryptedArg : _decryptedCTXArgs.args ) {
        decryptedArgsStream << decryptedArg;
    }

    // Reuse plaintext args as-is
    RLPStream plaintextArgsStream;
    plaintextArgsStream.appendList( plaintextArgsRlp.itemCount() );
    for ( size_t i = 0; i < plaintextArgsRlp.itemCount(); ++i ) {
        plaintextArgsStream << plaintextArgsRlp[i].toBytes();
    }

    // Create final RLP: RLP(RLP(decrypted_args), RLP(plaintext_args))
    RLPStream finalRlp;
    finalRlp.appendList( 2 );
    finalRlp.appendRaw( decryptedArgsStream.out() );
    finalRlp.appendRaw( plaintextArgsStream.out() );

    // Convert RLP to ABI-encoded format using helper function
    dev::bytes abiEncodedArrays = rlpToAbiEncodedArrays( finalRlp.out() );

    return functionSelector + abiEncodedArrays;
}

std::pair< RLPStream, size_t > parseAbiEncodedBytesArray( bytesConstRef dataRef,
    bigint const& arrayOffset, const std::string& arrayName,
    std::optional< const BITEVerificationData* > _verificationData ) {
    if ( dataRef.size() < arrayOffset.convert_to< size_t >() + dev::h256::size )
        throw std::runtime_error(
            "parseAbiEncodedBytesArray: input too short for " + arrayName + " array" );

    bigint const arrayLength( parseBigEndianRightPadded( dataRef, arrayOffset, dev::h256::size ) );
    if ( arrayLength < 0 )
        throw std::runtime_error( "parseAbiEncodedBytesArray: invalid " + arrayName + " length" );

    size_t arrayCount = arrayLength.convert_to< size_t >();
    size_t arrayBase = arrayOffset.convert_to< size_t >() + dev::h256::size;

    RLPStream arrayStream;
    arrayStream.appendList( arrayCount );

    for ( size_t i = 0; i < arrayCount; ++i ) {
        if ( dataRef.size() < arrayBase + i * dev::h256::size + dev::h256::size )
            throw std::runtime_error(
                "parseAbiEncodedBytesArray: input too short for " + arrayName + " element offset" );

        bigint elemOffset( parseBigEndianRightPadded(
            dataRef, arrayBase + i * dev::h256::size, dev::h256::size ) );
        size_t elemPos = arrayOffset.convert_to< size_t >() + dev::h256::size +
                         elemOffset.convert_to< size_t >();

        if ( dataRef.size() < elemPos + dev::h256::size )
            throw std::runtime_error(
                "parseAbiEncodedBytesArray: input too short for " + arrayName + " element length" );

        bigint elemLength( parseBigEndianRightPadded( dataRef, elemPos, dev::h256::size ) );
        if ( dataRef.size() < elemPos + dev::h256::size + elemLength.convert_to< size_t >() )
            throw std::runtime_error(
                "parseAbiEncodedBytesArray: input too short for " + arrayName + " element data" );

        // Validate encrypted element length if required
        if ( _verificationData.has_value() && isCiphertextValidationEnabled &&
             elemLength.convert_to< size_t >() < BITE_CIPHERTEXT_MIN_LEN )
            throw std::runtime_error(
                "parseAbiEncodedBytesArray: encrypted argument too short, must be at least " +
                std::to_string( BITE_CIPHERTEXT_MIN_LEN ) + " bytes" );

        dev::bytes elemData =
            dataRef.cropped( elemPos + dev::h256::size, elemLength.convert_to< size_t >() )
                .toBytes();
        if ( _verificationData.has_value() && isCiphertextValidationEnabled )
            dev::bite::validateBITECiphertext( elemData, *_verificationData.value() );
        arrayStream << elemData;
    }

    return { arrayStream, arrayCount };
}

std::pair< dev::bytes, size_t > abiEncodedArraysToRlp(
    const dev::bytes& _abiEncodedArrays, const BITEVerificationData& _verificationData ) {
    // Parse ABI-encoded data: abi.encode(bytes[] encryptedArgs, bytes[] plaintextArgs)
    // ABI format: offset_to_encryptedArgs(32) + offset_to_plaintextArgs(32) + encryptedArgs_data +
    // plaintextArgs_data where encryptedArgs_data = length(32) + offset_to_elem0(32) + ... +
    // elem0_length(32) + elem0_data + ...

    bytesConstRef dataRef( _abiEncodedArrays.data(), _abiEncodedArrays.size() );

    if ( dataRef.size() < 2 * dev::h256::size )
        throw std::runtime_error( "abiEncodedArraysToRlp: input too short for two array offsets" );

    // Read offsets to the two arrays
    bigint const encryptedArgsOffset( parseBigEndianRightPadded( dataRef, 0, dev::h256::size ) );
    bigint const plaintextArgsOffset(
        parseBigEndianRightPadded( dataRef, dev::h256::size, dev::h256::size ) );

    // Parse both arrays
    auto [encryptedArgsStream, encryptedArgsCount] = parseAbiEncodedBytesArray(
        dataRef, encryptedArgsOffset, "encryptedArgs", &_verificationData );
    auto [plaintextArgsStream, plaintextArgsCount] =
        parseAbiEncodedBytesArray( dataRef, plaintextArgsOffset, "plaintextArgs" );

    // Create final RLP: RLP(RLP(encryptedArgs[0], ...), RLP(plaintextArgs[0], ...))
    RLPStream finalStream;
    finalStream.appendList( 2 );
    finalStream.appendRaw( encryptedArgsStream.out() );
    finalStream.appendRaw( plaintextArgsStream.out() );

    return { finalStream.out(), encryptedArgsCount };
}
#endif  // BITE2

}  // namespace bite
}  // namespace dev

#endif  // BITE
