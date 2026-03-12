/*
    Modifications Copyright (C) 2018-2026 SKALE Labs

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
/** @file BITECommon.h
 * @author SKALE Labs
 * @date 2026
 *
 * BITE-specific constants & algorithms.
 */

#pragma once

#include <libdevcore/Address.h>
#include <libdevcore/CommonData.h>

#ifdef BITE
#include <libconsensus/node/ConsensusInterface.h>
#endif

#ifdef BITE

namespace dev {

class RLPStream;

namespace bite {

struct BITEVerificationData {
    uint64_t epochId;
    dev::bytes aadTE;
};

inline bool isCiphertextValidationEnabled = false;

// Validate BITE ciphertext against given epochId
void validateBITECiphertext(
    const dev::bytes& _ciphertext, const BITEVerificationData& _verificationData );

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
constexpr uint64_t ON_DECRYPT_FUNCTION_SELECTOR_SIZE_BYTES = 4;
static inline const dev::bytes ON_DECRYPT_FUNCTION_SELECTOR = { 0x57, 0x98, 0x3a, 0xc8 };

// Construct CTX with all arguments decrypted from original CTX data and list of decrypted arguments
dev::bytes constructDecryptedCTXData(
    const dev::bytes& _txData, const DecryptedCTXArgs& _decryptedCTXArgs );

// Parse ABI-encoded bytes array and validates encrypted elements
std::pair< RLPStream, size_t > parseAbiEncodedBytesArray( bytesConstRef dataRef,
    bigint const& arrayOffset, const std::string& arrayName,
    std::optional< const BITEVerificationData* > _verificationData = std::nullopt );

// Convert ABI-encoded arrays to RLP format
std::pair< dev::bytes, size_t > abiEncodedArraysToRlp(
    const dev::bytes& _abiEncodedArrays, const BITEVerificationData& _verificationData );

// Error codes for submitCTX precompiled contract
namespace SubmitCTXStatus {
constexpr uint64_t SUCCESS = 1;
constexpr uint64_t INPUT_TOO_SHORT = 1;
constexpr uint64_t INVALID_DESTINATION = 2;
constexpr uint64_t INVALID_GAS_LIMIT = 3;
constexpr uint64_t DATA_OFFSET_OUT_OF_BOUNDS = 4;
constexpr uint64_t DATA_TOO_SHORT = 5;
constexpr uint64_t ABI_TO_RLP_CONVERSION_FAILED = 6;
constexpr uint64_t ABI_TO_RLP_UNKNOWN_ERROR = 7;
constexpr uint64_t INVALID_SIGNATURE = 8;
constexpr uint64_t INVALID_TRANSACTION = 9;
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

}  // namespace bite
}  // namespace dev
#endif  // BITE
