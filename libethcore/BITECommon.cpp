#ifdef BITE

#include <libdevcore/RLP.h>
#include <libethcore/BITECommon.h>
#include <libethcore/Exceptions.h>

using namespace dev;
using namespace dev::eth;

namespace dev {

namespace bite {

void validateBITECiphertext( const dev::bytes& _ciphertext, uint64_t _currentEpochId ) {
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
        throw;
    }
}

#ifdef BITE2
dev::bytes constructDecryptedCTXData(
    const dev::bytes& _txData, const DecryptedCATArgs& _decryptedCTXArgs ) {
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
#endif   // BITE2

}  // namespace bite
}  // namespace dev

#endif   // BITE
