const fs = require( "fs" );
const path = require( "path" );
const child_process = require( "child_process" );
const cc = require( "./cc.js" );
const log = require( "./log.js" );
cc.enable( true );
log.addStdout();
//log.add( strFilePath, nMaxSizeBeforeRotation, nMaxFilesCount );

let strHorizontalSeparator = "=================================================================================================";

function replaceAll( str, strFind, strReplace ) {
    var lenFind = strFind.length;
    if( lenFind == 0 )
        return str;
    strReplace = strReplace || "";
    while( true ) {
        var pos = str.indexOf( strFind );
        if( pos < 0 )
            break;
        str = str.substr( 0, pos ) + strReplace + str.substr( pos + lenFind );
    }
    return str;
}

function checkBoolOpt( opts, strName, bDefaultValue ) {
    bDefaultValue = bDefaultValue || false;
    try {
        if( opts[strName] )
            return true;
    } catch( e ) { }
    return bDefaultValue;
}

function ensure_endl( s ) {
    s = s || "";
    s = s.replace( /^\s+|\s+$/g, "" );
    var n = s.length;
    if( n > 0 ) {
        if( s[n-1] != "\n" )
            s += "\n";
    } else {
        //s = "\n";
    }
    return s;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

let mapTestExpectations = {
    "BlockchainTests":                                                 { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcStateTests":                                    { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcBlockGasLimitTest":                             { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcGasPricerTest":                                 { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcInvalidHeaderTest":                             { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcUncleHeaderValidity":                           { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcUncleTest":                                     { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcValidBlockTest":                                { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcWalletTest":                                    { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcTotalDifficultyTest":                           { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcMultiChainTest":                                { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcForkStressTest":                                { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcForgedTest":                                    { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcRandomBlockhashTest":                           { "isSkip": false, "expect": "pass" },
    "BlockchainTests/bcExploitTest":                                   { "isSkip": false, "expect": "pass" },
    "TransitionTests":                                                 { "isSkip": false, "expect": "pass" },
    "TransitionTests/bcFrontierToHomestead":                           { "isSkip": false, "expect": "pass" },
    "TransitionTests/bcHomesteadToDao":                                { "isSkip": false, "expect": "pass" },
    "TransitionTests/bcHomesteadToEIP150":                             { "isSkip": false, "expect": "pass" },
    "TransitionTests/bcEIP158ToByzantium":                             { "isSkip": false, "expect": "pass" },
    "TransitionTests/bcByzantiumToConstantinople":                     { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stCallCodes":                                 { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests":                                             { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stCallCreateCallCodeTest":                    { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stExample":                                   { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stInitCodeTest":                              { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stLogTests":                                  { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stMemoryTest":                                { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stPreCompiledContracts":                      { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stPreCompiledContracts2":                     { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stRandom":                                    { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stRandom2":                                   { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stRecursiveCreate":                           { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stRefundTest":                                { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stSolidityTest":                              { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stSpecialTest":                               { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stSystemOperationsTest":                      { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stTransactionTest":                           { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stTransitionTest":                            { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stWalletTest":                                { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stCallDelegateCodesCallCodeHomestead":        { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stCallDelegateCodesHomestead":                { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stHomesteadSpecific":                         { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stDelegatecallTestHomestead":                 { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stChangedEIP150":                             { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stEIP150singleCodeGasPrices":                 { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stMemExpandingEIP150Calls":                   { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stEIP150Specific":                            { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stEIP158Specific":                            { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stNonZeroCallsTest":                          { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stZeroCallsTest":                             { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stZeroCallsRevert":                           { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stCodeSizeLimit":                             { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stCreateTest":                                { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stRevertTest":                                { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stStackTests":                                { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stStaticCall":                                { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stReturnDataTest":                            { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stZeroKnowledge":                             { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stZeroKnowledge2":                            { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stBugs":                                      { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stShift":                                     { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stAttackTest":                                { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stMemoryStressTest":                          { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stQuadraticComplexityTest":                   { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stBadOpcode":                                 { "isSkip": false, "expect": "pass" },
    "BCGeneralStateTests/stArgsZeroOneBalance":                        { "isSkip": false, "expect": "pass" },
    "GeneralStateTests":                                               { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stCallCodes":                                   { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stCallCreateCallCodeTest":                      { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stExample":                                     { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stInitCodeTest":                                { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stLogTests":                                    { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stMemoryTest":                                  { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stPreCompiledContracts":                        { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stPreCompiledContracts2":                       { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stRandom":                                      { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stRandom2":                                     { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stRecursiveCreate":                             { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stRefundTest":                                  { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stSolidityTest":                                { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stSpecialTest":                                 { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stSystemOperationsTest":                        { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stTransactionTest":                             { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stTransitionTest":                              { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stWalletTest":                                  { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stCallDelegateCodesCallCodeHomestead":          { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stCallDelegateCodesHomestead":                  { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stHomesteadSpecific":                           { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stDelegatecallTestHomestead":                   { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stChangedEIP150":                               { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stEIP150singleCodeGasPrices":                   { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stMemExpandingEIP150Calls":                     { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stEIP150Specific":                              { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stEIP158Specific":                              { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stNonZeroCallsTest":                            { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stZeroCallsTest":                               { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stZeroCallsRevert":                             { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stCodeSizeLimit":                               { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stCreateTest":                                  { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stRevertTest":                                  { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stStackTests":                                  { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stStaticCall":                                  { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stReturnDataTest":                              { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stZeroKnowledge":                               { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stZeroKnowledge2":                              { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stCodeCopyTest":                                { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stBugs":                                        { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stShift":                                       { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stAttackTest":                                  { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stMemoryStressTest":                            { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stQuadraticComplexityTest":                     { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stBadOpcode":                                   { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stArgsZeroOneBalance":                          { "isSkip": false, "expect": "pass" },
    "GeneralStateTests/stEWASMTests":                                  { "isSkip": false, "expect": "pass" },
    "TransactionTests":                                                { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttAddress":                                      { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttData":                                         { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttGasLimit":                                     { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttGasPrice":                                     { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttNonce":                                        { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttRSValue":                                      { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttValue":                                        { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttVValue":                                       { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttSignature":                                    { "isSkip": false, "expect": "pass" },
    "TransactionTests/ttWrongRLP":                                     { "isSkip": false, "expect": "pass" },
    "VMTests":                                                         { "isSkip": false, "expect": "pass" },
    "VMTests/vmArithmeticTest":                                        { "isSkip": false, "expect": "pass" },
    "VMTests/vmBitwiseLogicOperation":                                 { "isSkip": false, "expect": "pass" },
    "VMTests/vmBlockInfoTest":                                         { "isSkip": false, "expect": "pass" },
    "VMTests/vmEnvironmentalInfo":                                     { "isSkip": false, "expect": "pass" },
    "VMTests/vmIOandFlowOperations":                                   { "isSkip": false, "expect": "pass" },
    "VMTests/vmLogTest":                                               { "isSkip": false, "expect": "pass" },
    "VMTests/vmPerformance":                                           { "isSkip": false, "expect": "pass" },
    "VMTests/vmPushDupSwapTest":                                       { "isSkip": false, "expect": "pass" },
    "VMTests/vmRandomTest":                                            { "isSkip": false, "expect": "pass" },
    "VMTests/vmSha3Test":                                              { "isSkip": false, "expect": "pass" },
    "VMTests/vmSystemOperations":                                      { "isSkip": false, "expect": "pass" },
    "VMTests/vmTests":                                                 { "isSkip": false, "expect": "pass" },
    "boostTests":                                                      { "isSkip": false, "expect": "pass" },
    "boostTests/u256_overflow_test":                                   { "isSkip": false, "expect": "pass" },
    "boostTests/u256_shift_left":                                      { "isSkip": false, "expect": "pass" },
    "boostTests/u256_shift_left_bug":                                  { "isSkip": false, "expect": "pass" },
    "boostTests/u256_logical_shift_right":                             { "isSkip": false, "expect": "pass" },
    "boostTests/u256_arithmetic_shift_right":                          { "isSkip": false, "expect": "pass" },
    "CommonJSTests":                                                   { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_toJS":                                         { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_jsToBytes":                                    { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_padded":                                       { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_paddedRight":                                  { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_unpadded":                                     { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_unpaddedLeft":                                 { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_fromRaw":                                      { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_jsToFixed":                                    { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_jsToInt":                                      { "isSkip": false, "expect": "pass" },
    "CommonJSTests/test_jsToU256":                                     { "isSkip": false, "expect": "pass" },
    "FixedHashTests":                                                  { "isSkip": false, "expect": "pass" },
    "FixedHashTests/FixedHashComparisons":                             { "isSkip": false, "expect": "pass" },
    "FixedHashTests/FixedHashXOR":                                     { "isSkip": false, "expect": "pass" },
    "FixedHashTests/FixedHashOR":                                      { "isSkip": false, "expect": "pass" },
    "FixedHashTests/FixedHashAND":                                     { "isSkip": false, "expect": "pass" },
    "FixedHashTests/FixedHashInvert":                                  { "isSkip": false, "expect": "pass" },
    "FixedHashTests/FixedHashContains":                                { "isSkip": false, "expect": "pass" },
    "FixedHashTests/FixedHashIncrement":                               { "isSkip": false, "expect": "pass" },
    "RangeMaskTest":                                                   { "isSkip": false, "expect": "pass" },
    "RangeMaskTest/constructor":                                       { "isSkip": false, "expect": "pass" },
    "RangeMaskTest/simple_unions":                                     { "isSkip": false, "expect": "pass" },
    "RangeMaskTest/empty_union":                                       { "isSkip": false, "expect": "pass" },
    "RangeMaskTest/overlapping_unions":                                { "isSkip": false, "expect": "pass" },
    "RangeMaskTest/complement":                                        { "isSkip": false, "expect": "pass" },
    "RangeMaskTest/iterator":                                          { "isSkip": false, "expect": "pass" },
    "CoreLibTests":                                                    { "isSkip": false, "expect": "pass" },
    "CoreLibTests/toHex":                                              { "isSkip": false, "expect": "pass" },
    "CoreLibTests/toCompactHex":                                       { "isSkip": false, "expect": "pass" },
    "CoreLibTests/byteRef":                                            { "isSkip": false, "expect": "pass" },
    "CoreLibTests/isHex":                                              { "isSkip": false, "expect": "pass" },
    "RlpTests":                                                        { "isSkip": false, "expect": "pass" },
    "RlpTests/EmptyArrayList":                                         { "isSkip": false, "expect": "pass" },
    "RlpTests/invalidRLPtest":                                         { "isSkip": false, "expect": "pass" },
    "RlpTests/rlptest":                                                { "isSkip": false, "expect": "pass" },
    "RlpTests/rlpRandom":                                              { "isSkip": false, "expect": "pass" },
    "Crypto":                                                          { "isSkip": false, "expect": "pass" },
    "Crypto/AES":                                                      { "isSkip": false, "expect": "pass" },
    "Crypto/AES/AesDecrypt":                                           { "isSkip": false, "expect": "pass" },
    "Crypto/AES/AesDecryptWrongSeed":                                  { "isSkip": false, "expect": "pass" },
    "Crypto/AES/AesDecryptWrongPassword":                              { "isSkip": false, "expect": "pass" },
    "Crypto/AES/AesDecryptFailInvalidSeed":                            { "isSkip": false, "expect": "pass" },
    "Crypto/AES/AesDecryptFailInvalidSeedSize":                        { "isSkip": false, "expect": "pass" },
    "Crypto/AES/AesDecryptFailInvalidSeed2":                           { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore":                                                 { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore/basic_tests":                                     { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore/import_key_from_file":                            { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore/import_secret":                                   { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore/import_secret_bytesConstRef":                     { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore/wrong_password":                                  { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore/recode":                                          { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore/keyImport_PBKDF2SHA256":                          { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore/keyImport_Scrypt":                                { "isSkip": false, "expect": "pass" },
    "Crypto/KeyStore/keyImport__ScryptV2":                             { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto":                                                { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/sha3general":                                    { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/emptySHA3Types":                                 { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/pubkeyOfZero":                                   { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/KeyPairMix":                                     { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/keypairs":                                       { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/KeyPairVerifySecret":                            { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/SignAndRecover":                                 { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/SignAndRecoverLoop":                             { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/cryptopp_patch":                                 { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/verify_secert":                                  { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/common_encrypt_decrypt":                         { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/sha3_norestart":                                 { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecies_kdf":                                      { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecdh_agree_invalid_pubkey":                      { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecdh_agree_invalid_seckey":                      { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecies_standard":                                 { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecies_sharedMacData":                            { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecies_eckeypair":                                { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecdhCryptopp":                                   { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecdhe":                                          { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecdhAgree":                                      { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/handshakeNew":                                   { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecies_aes128_ctr_unaligned":                     { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/ecies_aes128_ctr":                               { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/cryptopp_aes128_ctr":                            { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/cryptopp_aes128_cbc":                            { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/recoverVgt3":                                    { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/PerfSHA256_32":                                  { "isSkip": false, "expect": "pass" },
    "Crypto/devcrypto/PerfSHA256_4000":                                { "isSkip": false, "expect": "pass" },
    "Crypto/Basic":                                                    { "isSkip": false, "expect": "pass" },
    "Crypto/Basic/hexPrefix_test":                                     { "isSkip": false, "expect": "pass" },
    "Crypto/Basic/base64":                                             { "isSkip": false, "expect": "pass" },
    "Crypto/Trie":                                                     { "isSkip": false, "expect": "pass" },
    "Crypto/Trie/fat_trie":                                            { "isSkip": false, "expect": "pass" },
    "Crypto/Trie/hex_encoded_securetrie_test":                         { "isSkip": false, "expect": "pass" },
    "Crypto/Trie/trie_test_anyorder":                                  { "isSkip": false, "expect": "pass" },
    "Crypto/Trie/trie_tests_ordered":                                  { "isSkip": false, "expect": "pass" },
    "Crypto/Trie/moreTrieTests":                                       { "isSkip": false, "expect": "pass" },
    "Crypto/Trie/trieLowerBound":                                      { "isSkip": false, "expect": "pass" },
    "Crypto/Trie/hashedLowerBound":                                    { "isSkip": false, "expect": "pass" },
    "Crypto/Trie/trieStess":                                           { "isSkip": false, "expect": "pass" },
    "Crypto/Trie/triePerf":                                            { "isSkip": false, "expect": "pass" },
    "LibSnark":                                                        { "isSkip": false, "expect": "pass" },
    "LibSnark/ecadd":                                                  { "isSkip": false, "expect": "pass" },
    "LibSnark/fieldPointInvalid":                                      { "isSkip": false, "expect": "pass" },
    "LibSnark/invalid":                                                { "isSkip": false, "expect": "pass" },
    "LibSnark/ecmul_add":                                              { "isSkip": false, "expect": "pass" },
    "LibSnark/pairing":                                                { "isSkip": false, "expect": "pass" },
    "LibSnark/pairingNullInput":                                       { "isSkip": false, "expect": "pass" },
    "LibSnark/generateRandomPoints":                                   { "isSkip": false, "expect": "pass" },
    "LibSnark/benchECADD":                                             { "isSkip": false, "expect": "pass" },
    "LibSnark/benchECMULRand":                                         { "isSkip": false, "expect": "pass" },
    "LibSnark/benchECMULWorstCase1":                                   { "isSkip": false, "expect": "pass" },
    "LibSnark/benchECMULWorstCase2":                                   { "isSkip": false, "expect": "pass" },
    "LibSnark/benchECMULIdentity":                                     { "isSkip": false, "expect": "pass" },
    "LibSnark/ECMULuseCaseFromRopsten":                                { "isSkip": false, "expect": "pass" },
    "EthashTests":                                                     { "isSkip": false, "expect": "pass" },
    "EthashTests/calculateDifficultyByzantiumWithoutUncles":           { "isSkip": false, "expect": "pass" },
    "EthashTests/calculateDifficultyByzantiumWithUncles":              { "isSkip": false, "expect": "pass" },
    "EthashTests/calculateDifficultyByzantiumMaxAdjustment":           { "isSkip": false, "expect": "pass" },
    "EthashTests/epochSeed":                                           { "isSkip": false, "expect": "pass" },
    "EthashTests/etashQuickVerify":                                    { "isSkip": false, "expect": "fail" },
    "EthashTests/etashVerify":                                         { "isSkip": false, "expect": "fail" },
    "EthashTests/ethashEvalHeader":                                    { "isSkip": false, "expect": "pass" },
    "PrecompiledTests":                                                { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpFermatTheorem":                            { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpZeroBase":                                 { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpExtraByteIgnored":                         { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpRightPadding":                             { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpMissingValues":                            { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpEmptyValue":                               { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpZeroPowerZero":                            { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpZeroPowerZeroModZero":                     { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpModLengthZero":                            { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpCostFermatTheorem":                        { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpCostTooLarge":                             { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpCostEmptyExponent":                        { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpCostZeroExponent":                         { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpCostApproximated":                         { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpCostApproximatedPartialByte":              { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpCostApproximatedGhost":                    { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpCostMidRange":                             { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/modexpCostHighRange":                            { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/bench_ecrecover":                                { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/bench_modexp":                                   { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/bench_bn256Add":                                 { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/bench_bn256ScalarMul":                           { "isSkip": false, "expect": "pass" },
    "PrecompiledTests/bench_bn256Pairing":                             { "isSkip": false, "expect": "pass" },
    "SealEngineTests":                                                 { "isSkip": false, "expect": "pass" },
    "SealEngineTests/UnsignedTransactionTests":                        { "isSkip": false, "expect": "pass" },
    "SealEngineTests/UnsignedTransactionTests/UnsignedTransactionIsValidBeforeExperimental": { "isSkip": false, "expect": "pass" },
    "SealEngineTests/UnsignedTransactionTests/UnsignedTransactionIsValidInExperimental":     { "isSkip": false, "expect": "pass" },
    "commonjs":                                                        { "isSkip": false, "expect": "pass" },
    "commonjs/jsToPublic":                                             { "isSkip": false, "expect": "pass" },
    "commonjs/jsToAddress":                                            { "isSkip": false, "expect": "pass" },
    "commonjs/jsToSecret":                                             { "isSkip": false, "expect": "pass" },
    "DifficultyTests":                                                 { "isSkip": false, "expect": "pass" },
    "DifficultyTests/difficultyTestsFrontier":                         { "isSkip": false, "expect": "pass" },
    "DifficultyTests/difficultyTestsRopsten":                          { "isSkip": false, "expect": "pass" }, // became broken
    "DifficultyTests/difficultyTestsHomestead":                        { "isSkip": false, "expect": "pass" },
    "DifficultyTests/difficultyByzantium":                             { "isSkip": false, "expect": "pass" },
    "DifficultyTests/difficultyTestsMainNetwork":                      { "isSkip": false, "expect": "pass" },
    "DifficultyTests/difficultyTestsCustomMainNetwork":                { "isSkip": false, "expect": "pass" },
    "DifficultyTests/basicDifficultyTest":                             { "isSkip": false, "expect": "pass" },
    "KeyManagerTests":                                                 { "isSkip": false, "expect": "pass" },
    "KeyManagerTests/KeyInfoDefaultConstructor":                       { "isSkip": false, "expect": "pass" },
    "KeyManagerTests/KeyInfoConstructor":                              { "isSkip": false, "expect": "pass" },
    "KeyManagerTests/KeyManagerConstructor":                           { "isSkip": false, "expect": "pass" },
    "KeyManagerTests/KeyManagerKeysFile":                              { "isSkip": false, "expect": "pass" },
    "KeyManagerTests/KeyManagerHints":                                 { "isSkip": false, "expect": "pass" },
    "KeyManagerTests/KeyManagerAccounts":                              { "isSkip": false, "expect": "pass" },
    "KeyManagerTests/KeyManagerKill":                                  { "isSkip": false, "expect": "pass" },
    "BlockSuite":                                                      { "isSkip": false, "expect": "pass" },
    "BlockSuite/FrontierBlockSuite":                                   { "isSkip": false, "expect": "fail" },
    "BlockSuite/FrontierBlockSuite/bStates":                           { "isSkip": false, "expect": "pass" },
    "BlockSuite/FrontierBlockSuite/bCopyOperator":                     { "isSkip": false, "expect": "pass" },
    "BlockSuite/bGasPricer":                                           { "isSkip": false, "expect": "fail" },
    "BlockSuite/bGetReceiptOverflow":                                  { "isSkip": false, "expect": "fail" },
    "BlockSuite/ConstantinopleBlockSuite":                             { "isSkip": false, "expect": "fail" },
    "BlockSuite/ConstantinopleBlockSuite/bBlockhashContractIsCreated": { "isSkip": false, "expect": "pass" },
    "BlockChainFrontierSuite":                                         { "isSkip": false, "expect": "pass" },
    "BlockChainFrontierSuite/output":                                  { "isSkip": false, "expect": "fail" },
    "BlockChainFrontierSuite/opendb":                                  { "isSkip": false, "expect": "pass" },
    "BlockChainFrontierSuite/Mining_1_mineBlockWithTransaction":       { "isSkip": false, "expect": "fail" },
    "BlockChainFrontierSuite/Mining_2_mineUncles":                     { "isSkip": false, "expect": "fail" },
    "BlockChainFrontierSuite/insertWithoutParent":                     { "isSkip": false, "expect": "pass" },
    "BlockChainMainNetworkSuite":                                      { "isSkip": false, "expect": "pass" },
    "BlockChainMainNetworkSuite/Mining_5_BlockFutureTime":             { "isSkip": false, "expect": "pass" },
    "BlockChainMainNetworkSuite/attemptImport":                        { "isSkip": false, "expect": "fail" },
    "BlockChainMainNetworkSuite/insert":                               { "isSkip": false, "expect": "fail" },
    "BlockChainMainNetworkSuite/insertException":                      { "isSkip": false, "expect": "fail" },
    "BlockChainMainNetworkSuite/rescue":                               { "isSkip": false, "expect": "fail" },
    "BlockChainMainNetworkSuite/updateStats":                          { "isSkip": false, "expect": "fail" },
    "BlockChainMainNetworkSuite/invalidJsonThrows":                    { "isSkip": false, "expect": "pass" },
    "BlockChainMainNetworkSuite/unknownFieldThrows":                   { "isSkip": false, "expect": "pass" },
    "BlockChainInsertTests":                                           { "isSkip": false, "expect": "pass" },
    "BlockChainInsertTests/bcBasicInsert":                             { "isSkip": false, "expect": "fail" },
    "BlockQueueSuite":                                                 { "isSkip": false, "expect": "pass" },
    "BlockQueueSuite/BlockQueueImport":                                { "isSkip": false, "expect": "fail" },
    "ClientBase":                                                      { "isSkip": false, "expect": "pass" },
    "ClientBase/blocks":                                               { "isSkip": false, "expect": "pass" },
    "ClientTestSuite":                                                 { "isSkip": false, "expect": "pass" },
    "ClientTestSuite/ClientTest_setChainParamsAuthor":                 { "isSkip": false, "expect": "pass" },
    "ExtVmSuite":                                                      { "isSkip": false, "expect": "pass" },
    "ExtVmSuite/BlockhashOutOfBoundsRetunsZero":                       { "isSkip": false, "expect": "pass" },
    "ExtVmSuite/BlockhashBeforeConstantinopleReliesOnLastHashes":      { "isSkip": false, "expect": "pass" },
    "ExtVmSuite/BlockhashDoesntNeedLastHashesInConstantinople":        { "isSkip": false, "expect": "fail" },
    "GasPricer":                                                       { "isSkip": false, "expect": "pass" },
    "GasPricer/trivialGasPricer":                                      { "isSkip": false, "expect": "pass" },
    "GasPricer/basicGasPricerNoUpdate":                                { "isSkip": false, "expect": "pass" },
    "BasicTests":                                                      { "isSkip": false, "expect": "pass" },
    "BasicTests/emptySHA3Types":                                       { "isSkip": false, "expect": "pass" },
    "BasicTests/genesis_tests":                                        { "isSkip": false, "expect": "pass" },
    "SnapshotImporterSuite":                                           { "isSkip": false, "expect": "pass" },
    "SnapshotImporterSuite/SnapshotImporterSuite_importChecksManifestVersion":                                       { "isSkip": false, "expect": "pass" },
    "SnapshotImporterSuite/SnapshotImporterSuite_importNonsplittedAccount":                                          { "isSkip": false, "expect": "pass" },
    "SnapshotImporterSuite/SnapshotImporterSuite_importSplittedAccount":                                             { "isSkip": false, "expect": "pass" },
    "SnapshotImporterSuite/SnapshotImporterSuite_importAccountWithCode":                                             { "isSkip": false, "expect": "pass" },
    "SnapshotImporterSuite/SnapshotImporterSuite_importAccountsWithEqualCode":                                       { "isSkip": false, "expect": "pass" },
    "SnapshotImporterSuite/SnapshotImporterSuite_commitStateOnceEveryChunk":                                         { "isSkip": false, "expect": "pass" },
    "SnapshotImporterSuite/SnapshotImporterSuite_importEmptyBlock":                                                  { "isSkip": false, "expect": "pass" },
    "SnapshotImporterSuite/SnapshotImporterSuite_importBlockWithTransactions":                                       { "isSkip": false, "expect": "pass" },
    "StateUnitTests":                                                                                                { "isSkip": false, "expect": "pass" },
    "StateUnitTests/Basic":                                                                                          { "isSkip": false, "expect": "pass" },
    "StateUnitTests/LoadAccountCode":                                                                                { "isSkip": false, "expect": "pass" },
    "StateUnitTests/StateAddressRangeTests":                                                                         { "isSkip": false, "expect": "fail" },
    "StateUnitTests/StateAddressRangeTests/addressesReturnsAllAddresses":                                            { "isSkip": false, "expect": "pass" },
    "StateUnitTests/StateAddressRangeTests/addressesReturnsNoMoreThanRequested":                                     { "isSkip": false, "expect": "pass" },
    "StateUnitTests/StateAddressRangeTests/addressesDoesntReturnDeletedInCache":                                     { "isSkip": false, "expect": "pass" },
    "StateUnitTests/StateAddressRangeTests/addressesReturnsCreatedInCache":                                          { "isSkip": false, "expect": "pass" },
    "libethereum":                                                                                                   { "isSkip": false, "expect": "pass" },
    "libethereum/TransactionGasRequired":                                                                            { "isSkip": false, "expect": "pass" },
    "libethereum/ExecutionResultOutput":                                                                             { "isSkip": false, "expect": "pass" },
    "libethereum/transactionExceptionOutput":                                                                        { "isSkip": false, "expect": "pass" },
    "libethereum/toTransactionExceptionConvert":                                                                     { "isSkip": false, "expect": "pass" },
    "libethereum/GettingSenderForUnsignedTransactionThrows":                                                         { "isSkip": false, "expect": "pass" },
    "libethereum/GettingSignatureForUnsignedTransactionThrows":                                                      { "isSkip": false, "expect": "pass" },
    "libethereum/StreamRLPWithSignatureForUnsignedTransactionThrows":                                                { "isSkip": false, "expect": "pass" },
    "libethereum/CheckLowSForUnsignedTransactionThrows":                                                             { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite":                                                                                         { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite/TransactionEIP86":                                                                        { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite/tqMaxNonce":                                                                              { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite/tqPriority":                                                                              { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite/tqFuture":                                                                                { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite/tqLimits":                                                                                { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite/tqImport":                                                                                { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite/tqDrop":                                                                                  { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite/tqLimit":                                                                                 { "isSkip": false, "expect": "pass" },
    "TransactionQueueSuite/tqEqueue":                                                                                { "isSkip": false, "expect": "pass" },
    "LegacyVMSuite":                                                                                                 { "isSkip": false, "expect": "pass" },
    "LegacyVMSuite/LegacyVMCreate2Suite":                                                                            { "isSkip": false, "expect": "pass" },
    "LegacyVMSuite/LegacyVMCreate2Suite/LegacyVMCreate2worksInConstantinople":                                       { "isSkip": false, "expect": "pass" },
    "LegacyVMSuite/LegacyVMCreate2Suite/LegacyVMCreate2isInvalidBeforeConstantinople":                               { "isSkip": false, "expect": "pass" },
    "LegacyVMSuite/LegacyVMCreate2Suite/LegacyVMCreate2succeedsIfAddressHasEther":                                   { "isSkip": false, "expect": "pass" },
    "LegacyVMSuite/LegacyVMCreate2Suite/LegacyVMCreate2doesntChangeContractIfAddressExists":                         { "isSkip": false, "expect": "pass" },
    "LegacyVMSuite/LegacyVMCreate2Suite/LegacyVMCreate2isForbiddenInStaticCall":                                     { "isSkip": false, "expect": "pass" },
    "SkaledInterpreterSuite":                                                                                           { "isSkip": false, "expect": "pass" },
    "SkaledInterpreterSuite/SkaledInterpreterCreate2Suite":                                                             { "isSkip": false, "expect": "pass" },
    "SkaledInterpreterSuite/SkaledInterpreterCreate2Suite/SkaledInterpreterCreate2worksInConstantinople":               { "isSkip": false, "expect": "pass" },
    "SkaledInterpreterSuite/SkaledInterpreterCreate2Suite/SkaledInterpreterCreate2isInvalidBeforeConstantinople":       { "isSkip": false, "expect": "pass" },
    "SkaledInterpreterSuite/SkaledInterpreterCreate2Suite/SkaledInterpreterCreate2succeedsIfAddressHasEther":           { "isSkip": false, "expect": "pass" },
    "SkaledInterpreterSuite/SkaledInterpreterCreate2Suite/SkaledInterpreterCreate2doesntChangeContractIfAddressExists": { "isSkip": false, "expect": "pass" },
    "SkaledInterpreterSuite/SkaledInterpreterCreate2Suite/SkaledInterpreterCreate2isForbiddenInStaticCall":             { "isSkip": false, "expect": "pass" },
    "BlockChainTestSuite":                                            { "isSkip": false, "expect": "pass" },
    "BlockChainTestSuite/fillingExpectationOnMultipleNetworks":       { "isSkip": false, "expect": "fail" },
    "BlockChainTestSuite/fillingExpectationOnSingleNetwork":          { "isSkip": false, "expect": "fail" },
    "BlockChainTestSuite/fillingWithWrongExpectation":                { "isSkip": false, "expect": "fail" },
    "TestHelperSuite":                                                { "isSkip": false, "expect": "pass" },
    "TestHelperSuite/translateNetworks_gtHomestead":                  { "isSkip": false, "expect": "pass" },
    "TestHelperSuite/translateNetworks_geHomestead":                  { "isSkip": false, "expect": "pass" },
    "TestHelperSuite/translateNetworks_ltHomestead":                  { "isSkip": false, "expect": "pass" },
    "TestHelperSuite/translateNetworks_ltTest":                       { "isSkip": false, "expect": "pass" },
    "TestHelperSuite/translateNetworks_leHomestead":                  { "isSkip": false, "expect": "pass" },
    "TestHelperSuite/translateNetworks_leFrontier":                   { "isSkip": false, "expect": "pass" },
    "memDB":                                                          { "isSkip": false, "expect": "pass" },
    "memDB/kill":                                                     { "isSkip": false, "expect": "pass" },
    "memDB/purgeMainMem":                                             { "isSkip": false, "expect": "pass" },
    "memDB/purgeMainMem_Refs":                                        { "isSkip": false, "expect": "pass" },
    "memDB/purgeAuxMem":                                              { "isSkip": false, "expect": "pass" },
    "memDB/copy":                                                     { "isSkip": false, "expect": "pass" },
    "memDB/lookUp":                                                   { "isSkip": false, "expect": "pass" },
    "memDB/stream":                                                   { "isSkip": false, "expect": "pass" },
    "OverlayDBTests":                                                 { "isSkip": false, "expect": "pass" },
    "OverlayDBTests/basicUsage":                                      { "isSkip": false, "expect": "pass" },
    "OverlayDBTests/auxMem":                                          { "isSkip": false, "expect": "pass" },
    "OverlayDBTests/rollback":                                        { "isSkip": false, "expect": "pass" },
    "AccountHolderTest":                                              { "isSkip": false, "expect": "pass" },
    "AccountHolderTest/ProxyAccountUseCase":                          { "isSkip": false, "expect": "pass" },
    "ClientTests":                                                    { "isSkip": false, "expect": "pass" },
    "ClientTests/Personal":                                           { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite":                                                   { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/jsonrpc_gasPrice":                                  { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/jsonrpc_accounts":                                  { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/jsonrpc_number":                                    { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/jsonrpc_setMining":                                 { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/jsonrpc_stateAt":                                   { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/eth_coinbase":                                      { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/eth_sendTransaction":                               { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/eth_sendRawTransaction_validTransaction":           { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/eth_sendRawTransaction_errorZeroBalance":           { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/eth_sendRawTransaction_errorInvalidNonce":          { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/eth_sendRawTransaction_errorInsufficientGas":       { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/eth_sendRawTransaction_errorDuplicateTransaction":  { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/eth_signTransaction":                               { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/simple_contract":                                   { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/contract_storage":                                  { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/web3_sha3":                                         { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/test_setChainParams":                               { "isSkip": false, "expect": "pass" },
    "JsonRpcSuite/test_importRawBlock":                               { "isSkip": false, "expect": "pass" },
    "ConsensusTests":                                                 { "isSkip": false, "expect": "pass" },
    "ConsensusTests/OneTransaction":                                  { "isSkip": false, "expect": "pass" },
    "ConsensusTests/TwoTransactions":                                 { "isSkip": false, "expect": "pass" },
    "ConsensusTests/DifferentTransactions":                           { "isSkip": false, "expect": "pass" },
    "ConsensusTests/MissingTransaction1":                             { "isSkip": false, "expect": "pass" },
    "ConsensusTests/MissingTransaction2":                             { "isSkip": false, "expect": "pass" },
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

let strBuildDir = path.resolve( __dirname + "/../build" );
log.write( cc.info("Build") + cc.debug(" directory is ") + cc.notice(strBuildDir) + "\n" );
let strTestEthDir = path.resolve( strBuildDir + "/test" );
let strTestEthExe = path.resolve( strTestEthDir + "/testeth" );
let strTestsPath
if (process.argv.length > 2) {
    strTestEthExe = process.argv[2]
    strTestEthDir = path.dirname(strTestEthExe)
    if (process.argv.length > 3) {
        strTestsPath = process.argv[3]
    }
}
log.write( cc.info("Test") + cc.debug(" executable is ") + cc.notice(strTestEthExe) + "\n" );

// load tests tree as array of lines
let s = "";
try {
    const child = child_process.spawnSync( strTestEthExe, [ "--list_content" ] );
    //console.log( "error",  child.error  );
    //console.log( "stdout", child.stdout );
    //console.log( "stderr", child.stderr );
    s = child.stderr.toString();
} catch( e ) {
}
//log.write( cc.debug("Run result is:\n") + cc.notice(s) + "\n" );
let lines = s.split( "\n" );

// parse lines into tree
function computeIndentByTexts( s, arrLevelSpaces ) {
    var idxLevel, cntPossibleLevels = arrLevelSpaces.length;
    if( cntPossibleLevels == 0 )
        return 0;
    for( idxLevel = 0; idxLevel < cntPossibleLevels; ++ idxLevel ) {
        var idxLevelEffective = cntPossibleLevels - idxLevel - 1;
        var strLevelSpace = arrLevelSpaces[ idxLevelEffective ];
        var speaceLength = strLevelSpace.length;
        pos = s.indexOf( strLevelSpace );
        if( pos == 0 )
            return idxLevelEffective + 1;
    }
    return 0;
}
function composeIndent( nIndent, strLevelSpace ) {
    var i, ret = "";
    for( i = 0; i < nIndent; ++ i )
        ret += strLevelSpace;
    return ret;
}
function printTestNode( joTestNode, nIndent, strIndent ) {
    nIndent = nIndent || 0;
    strIndent = strIndent || "    ";
    if( joTestNode.joParent != null )
        log.write( composeIndent(nIndent-1,strIndent) + joTestNode.strName + "\n" );
    ++ nIndent;
    var i, cnt = joTestNode.arrChildren.length;
    for( i = 0; i < cnt; ++ i )
        printTestNode( joTestNode.arrChildren[i], nIndent, strIndent );
}
function isTestNodeLastOnLevel( joTestNode ) {
    if( joTestNode && joTestNode.joParent ) {
        var joLast = joTestNode.joParent.arrChildren[ joTestNode.joParent.arrChildren.length - 1 ];
        if( joLast == joTestNode )
            return true;
    }
    return false;
}
function calcTestNodeIndent( joTestNode ) {
    var nIndent = 0;
    while( joTestNode.joParent ) {
        joTestNode = joTestNode.joParent;
        ++ nIndent;
    }
    return nIndent;
}
function isRunableTestNode( joTestNode, isSkipExpecationCheck ) {
    isSkipExpecationCheck = isSkipExpecationCheck || false;
    var joExpectation = isSkipExpecationCheck ? null : mapTestExpectations[calcTestNodeNameSpec(joTestNode)];
    if( isSkipExpecationCheck || ( joExpectation && (!joExpectation.isSkip) ) ) {
        var nIndent = calcTestNodeIndent( joTestNode );
        if( ( nIndent == 1 && joTestNode.arrChildren.length == 0 ) || nIndent == 2 )
            return true;
    }
    return false;
}
function calcTestNodeNameSpec( joTestNode ) {
    var s = "";
    while( joTestNode ) {
        if( joTestNode.strName.length == 0 )
            break;
        if( s.length == 0 )
            s = joTestNode.strName;
        else
            s = joTestNode.strName + "/" + s;
        joTestNode = joTestNode.joParent;
    }
    return s;
}
function printTestNodeSpenName( joTestNode ) {
    if( joTestNode.joParent != null )
        console.log( calcTestNodeNameSpec( joTestNode ) );
    var i, cnt = joTestNode.arrChildren.length;
    for( i = 0; i < cnt; ++ i )
        printTestNodeSpenName( joTestNode.arrChildren[i] );
}
function calcTestNodeOutline( joTestNode ) {
    var s = "", n = 0;
    var v1 = "+---";
    var v2 = "|   ";
    var v3 = "    ";
    while( joTestNode && joTestNode.joParent ) {
        if( n == 0 ) {
            s = v1 + s;
        } else if( isTestNodeLastOnLevel(joTestNode) )
            s = v3 + s;
        else
            s = v2 + s;
        joTestNode = joTestNode.joParent;
        ++ n;
    }
    return s;
}
function printTestNodeWithOutline( joTestNode, opts ) {
    opts = opts || { };
    if( joTestNode.joParent != null ) {
        var nIndent = calcTestNodeIndent( joTestNode );
        var s =
            cc.debug(calcTestNodeOutline(joTestNode))
            + ( ( nIndent == 1 ) ? cc.sunny(joTestNode.strName) : ( ( nIndent == 2 ) ? cc.bright(joTestNode.strName) : cc.warn(joTestNode.strName) ) )
            ;
        if( joTestNode.arrChildren.length > 0 && checkBoolOpt( opts, "showChildrenCount" ) )
            s += cc.debug(", ") + cc.info(joTestNode.arrChildren.length) + cc.debug( ( joTestNode.arrChildren.length > 1 ) ? " children" : " child" );
        if( isRunableTestNode( joTestNode, true ) ) {
            if( checkBoolOpt( opts, "showRunableMark" ) )
                s += cc.debug(", ") + cc.info("runable");
            if( checkBoolOpt( opts, "showExpectationMark" ) ) {
                var joExpectation = mapTestExpectations[calcTestNodeNameSpec(joTestNode)];
                var strExpectation = "";
                if( joExpectation.expect == "fail" )
                    strExpectation = cc.error("should fail");
                else
                    strExpectation = cc.success("should "+joExpectation.expect);
                if( strExpectation.length > 0 )
                    s += cc.debug(", ") + strExpectation;
                if( joExpectation.isSkip )
                    s += cc.debug(", ") + cc.warn("skip");
            }
        }
        log.write( s + "\n" );
    }
    var i, cnt = joTestNode.arrChildren.length;
    for( i = 0; i < cnt; ++ i )
        printTestNodeWithOutline( joTestNode.arrChildren[i], opts );
}
function checkAllTestsKnown( joTestNode ) {
    if( joTestNode.joParent != null ) {
        var strTestNameSpec = calcTestNodeNameSpec(joTestNode);
        try {
            var joExpectation = mapTestExpectations[strTestNameSpec];
            if( ! joExpectation ) {
                log.write( cc.error("Unknown test ") + cc.info(strTestNameSpec) + cc.error(", no idea about its expectations") + "\n" );
                process.exit( 1 );
            }
        } catch( e ) {
            log.write( cc.error("Unknown test ") + cc.info(strTestNameSpec) + cc.error(", exception info: ") + cc.warn(e.message) + "\n" );
            process.exit( 1 );
        }
    }
    var i, cnt = joTestNode.arrChildren.length;
    for( i = 0; i < cnt; ++ i )
        checkAllTestsKnown( joTestNode.arrChildren[i] );
}
let g_arrTestsAll = [];
let g_arrTestsRunned = [];
let g_arrTestsSkipped = [];
let g_arrTestsFail = [];
let g_arrTestsFailExpected = [];
let g_arrTestsFailUnexpected = [];
let g_arrTestsSuccess = [];
let g_arrTestsSuccessExpected = [];
let g_arrTestsSuccessUnexpected = [];
function runTestNode( joTestNode ) {
    if( isRunableTestNode( joTestNode ) ) {
        var strTestNameSpec = calcTestNodeNameSpec(joTestNode);
        var joExpectation = mapTestExpectations[strTestNameSpec];
        var isFailExpected = ( joExpectation.expect == "fail" ) ? true : false;
        log.write( cc.debug(strHorizontalSeparator) + "\n" );
        log.write( cc.debug("Running test ") + cc.info(strTestNameSpec) + cc.debug("...") + "\n" );
        if( isFailExpected )
            log.write( cc.warn("NOTICE: this test should ") + cc.error("FAIL") + "\n" );
        try {
            call_params = { "cwd": strTestEthDir, stdio: "inherit"}
            if (typeof strTestsPath != "undefined") {
                call_params.env = {"ETHEREUM_TEST_PATH": strTestsPath}
            }
            const child = child_process.spawnSync( strTestEthExe, [ "-t", strTestNameSpec, "--", "--all" ], call_params);
            // //console.log( "error",  child.error  );
            // //console.log( "stdout", child.stdout );
            // //console.log( "stderr", child.stderr );
            // var strOut = ensure_endl(child.stdout.toString());
            // var strErr = ensure_endl(child.stderr.toString());
            // if( strOut.length > 0 )
            //     log.write( cc.debug(strOut) );
            // if( strErr.length > 0 )
            //     log.write( cc.error(strErr) );
            g_arrTestsAll.push(strTestNameSpec);
            g_arrTestsRunned.push(strTestNameSpec);
            if( child.status != null && child.status != 0 ) {
                g_arrTestsFail.push(strTestNameSpec);
                if( isFailExpected ) {
                    g_arrTestsFailExpected.push(strTestNameSpec);
                } else {
                    g_arrTestsFailUnexpected.push(strTestNameSpec);
                }
                log.write(
                    ( isFailExpected ? ( cc.success("EXPECTED") + cc.error(" TEST FAIL") ) : cc.fatal("TEST FAIL") )
                    + cc.error(" with exit status ") + cc.warn(child.status)
                    + "\n" );
            } else {
                g_arrTestsSuccess.push(strTestNameSpec);
                if( isFailExpected ) {
                    g_arrTestsSuccessUnexpected.push(strTestNameSpec);
                    log.write( cc.fatal("UNEXPECTED") + cc.success(" TEST SUCCESS") + "\n" );
                } else {
                    g_arrTestsSuccessExpected.push(strTestNameSpec);
                    log.write( cc.success("TEST SUCCESS") + "\n" );
                }
            }
        } catch( e ) {
            log.write( cc.error("Exception info: ") + cc.warn(e.message) + "\n" );
        }
    } else if( joTestNode.joParent ) {
        var strTestNameSpec = calcTestNodeNameSpec(joTestNode);
        var joExpectation = mapTestExpectations[strTestNameSpec];
        if( joExpectation.isSkip ) {
            g_arrTestsAll.push(strTestNameSpec);
            g_arrTestsSkipped.push(strTestNameSpec);
        }
    }
    var i, cnt = joTestNode.arrChildren.length;
    for( i = 0; i < cnt; ++ i )
        runTestNode( joTestNode.arrChildren[i] );
}

let joTestRoot = {
    "joParent": null
    , "strName": ""
    , "arrChildren": []
};
let arrLevelSpaces = [
    "    ",
    "        "
    ];
let i, cnt = lines.length, joTestCurrent = joTestRoot, currentIndent = 0;
for( i = 0; i < cnt; ++ i ) {
    s = lines[ i ];
    if( s.length == 0 )
        continue;
    s = replaceAll( s, "\t", "    " );
    var strName = s;
//console.log( strName );
    strName = replaceAll( strName, " " );
    strName = replaceAll( strName, "*" );
    var newNode =  {
        "joParent": null
        , "strName": strName
        , "arrChildren": []
    };
//console.log( newNode.strName );
    var nIndent = computeIndentByTexts( s, arrLevelSpaces );
//console.log( composeIndent(nIndent,"  ") + newNode.strName );
    if( nIndent == currentIndent ) {
        newNode.joParent = joTestCurrent;
        joTestCurrent.arrChildren.push( newNode );
        continue;
    }
    if( nIndent < currentIndent ) {
        while( nIndent < currentIndent ) {
            -- currentIndent;
            joTestCurrent = joTestCurrent.joParent;
            if( ! joTestCurrent ) {
                log.write( cc.error("Tree building error(1)") + "\n" );
                process.exit( 1 );
            }
        }
        newNode.joParent = joTestCurrent;
        joTestCurrent.arrChildren.push( newNode );
        continue;
    }
    if( nIndent == (currentIndent+1) ) {
        if( joTestCurrent.arrChildren.length == 0 ) {
            log.write( cc.error("Tree building error(2)") + "\n" );
            process.exit( 1 );runTestNode
        }
        var joLast = joTestCurrent.arrChildren[ joTestCurrent.arrChildren.length - 1 ];
        ++ currentIndent;
        joTestCurrent = joLast;
        newNode.joParent = joTestCurrent;
        joTestCurrent.arrChildren.push( newNode );
        continue;
    }
    log.write( cc.error("Tree building error(23), current indent is ") + cc.warn(currentIndent) + cc.error(", new indent is ") + cc.warn(nIndent) + "\n" );
    process.exit( 1 );
} // for( i = 0; i < cnt; ++ i
log.write( cc.debug(strHorizontalSeparator) + "\n" );
log.write( cc.debug("Loaded test tree with expecations:") + "\n" );
checkAllTestsKnown( joTestRoot );
//printTestNodeSpenName( joTestRoot );
printTestNodeWithOutline( joTestRoot, { "showChildrenCount": true, "showRunableMark": true, "showExpectationMark": true } );

runTestNode( joTestRoot );
log.write(
    cc.debug(strHorizontalSeparator) + "\n"
    + cc.notice ("Test summary results:") + "\n"
    + cc.info   ("Tests all") + cc.debug("......................") + cc.info(g_arrTestsAll.length) + "\n"
    + cc.info   ("Tests runned") + cc.debug("...................") + cc.info(g_arrTestsRunned.length) + "\n"
    + cc.warn   ("Tests skipped") + cc.debug("..................") + cc.info(g_arrTestsSkipped.length) + "\n"
    + cc.error  ("Tests failed") + cc.debug("...................") + cc.info(g_arrTestsFail.length) + "\n"
    + cc.error  ("Tests failed(expected)") + cc.debug(".........") + cc.info(g_arrTestsFailExpected.length) + "\n"
    + cc.error  ("Tests failed(unexpected)") + cc.debug(".......") + cc.info(g_arrTestsFailUnexpected.length) + "\n"
    + cc.success("Tests succeded") + cc.debug(".................") + cc.info(g_arrTestsSuccess.length) + "\n"
    + cc.success("Tests succeded(expected)") + cc.debug(".......") + cc.info(g_arrTestsSuccessExpected.length) + "\n"
    + cc.success("Tests succeded(unexpected)") + cc.debug(".....") + cc.info(g_arrTestsSuccessUnexpected.length) + "\n"
    );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

log.write( cc.debug(strHorizontalSeparator) + "\n" );
log.write( "\n" );

if(    g_arrTestsFailUnexpected.length > 0
    || g_arrTestsSuccessUnexpected.length > 0
    )
    process.exit( 1 ); // if we have something unexpected
else
    process.exit( 0 );
