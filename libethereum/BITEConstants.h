#pragma once

#include <libdevcore/Address.h>

#ifdef BITE2
#include <libconsensus/node/ConsensusInterface.h>
#endif

#ifdef BITE2
// Solidity adds 12 left-padded zero bytes when encoding an address parameter in the ABI format.
// Format: address to, uint256 gasLimit, uint256 biteDataOffset, uint256 biteDataLength, bytes
// biteData
static constexpr uint64_t BITE2_WALLET_GENERATION_INPUT_DATA_MIN_LEN =
    12 + dev::Address::size + dev::h256::size * 3 + BITE_CIPHERTEXT_MIN_LEN;

// address(20) + r(32) + s(32) + v(32)
static constexpr size_t WALLET_AND_SIGNATURE_LENGTH = dev::Address::size + 3 * dev::h256::size;

// Format: offset_to_walletAndSignature(32) + destination(32) +
// gasLimit(32) + offset_to_data(32) + walletAndSignature_data(116) + data_data
// walletAndSignature = wallet_address(20) + r(32) + s(32) + v(32)
static constexpr uint64_t BITE2_TRANSACTION_SUBMITION_INPUT_DATA_MIN_LEN =
    BITE2_WALLET_GENERATION_INPUT_DATA_MIN_LEN + WALLET_AND_SIGNATURE_LENGTH + dev::h256::size;

// keccak(onDecrypt(bytes[],bytes[])) - first 4 bytes: 0x57983ac8
static inline const dev::bytes ON_DECRYPT_FUNCTION_SELECTOR = { 0x57, 0x98, 0x3a, 0xc8 };

// Error codes for submitCTX precompiled contract
namespace SubmitCTXStatus {
constexpr uint64_t SUCCESS = 1;
constexpr uint64_t INPUT_TOO_SHORT = 1;
constexpr uint64_t INVALID_DESTINATION = 2;
constexpr uint64_t INVALID_GAS_LIMIT = 3;
constexpr uint64_t WALLET_AND_SIG_OFFSET_OUT_OF_BOUNDS = 4;
constexpr uint64_t INVALID_WALLET_AND_SIG_LENGTH = 5;
constexpr uint64_t WALLET_AND_SIG_DATA_TOO_SHORT = 6;
constexpr uint64_t INVALID_WALLET_ADDRESS = 7;
constexpr uint64_t WALLET_ALREADY_ACTIVE = 8;
constexpr uint64_t INVALID_SIGNATURE = 9;
constexpr uint64_t DATA_OFFSET_OUT_OF_BOUNDS = 10;
constexpr uint64_t DATA_TOO_SHORT = 11;
constexpr uint64_t ABI_TO_RLP_CONVERSION_FAILED = 12;
constexpr uint64_t ABI_TO_RLP_UNKNOWN_ERROR = 13;
constexpr uint64_t INVALID_TRANSACTION = 14;
}  // namespace SubmitCTXStatus

// Error codes for getRandomWalletAndSignatureForCTX precompiled contract
namespace GetRandomWalletStatus {
constexpr uint64_t INPUT_TOO_SHORT = 1;
constexpr uint64_t INVALID_DESTINATION = 2;
constexpr uint64_t DATA_OFFSET_OUT_OF_BOUNDS = 3;
constexpr uint64_t DATA_TOO_SHORT = 4;
constexpr uint64_t EMPTY_DATA = 5;
constexpr uint64_t ABI_TO_RLP_CONVERSION_FAILED = 6;
constexpr uint64_t ABI_TO_RLP_UNKNOWN_ERROR = 7;
constexpr uint64_t INSUFFICIENT_GAS_LIMIT = 8;
constexpr uint64_t INVALID_TRANSACTION = 9;
constexpr uint64_t WALLET_ALREADY_ACTIVE = 10;
}  // namespace GetRandomWalletStatus

#endif
