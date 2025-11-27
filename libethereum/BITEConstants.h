#pragma once

#include <libdevcore/Address.h>

#ifdef BITE2
#include <libconsensus/node/ConsensusInterface.h>
#endif

#ifdef BITE2
// Solidity adds 12 left-padded zero bytes when encoding an address parameter in the ABI format.
static constexpr uint64_t BITE2_INPUT_DATA_MIN_LEN =
    12 + dev::Address::size + dev::h256::size * 3 + BITE_CIPHERTEXT_MIN_LEN;
// keccak(onDecrypt(bytes[],bytes[])) - first 4 bytes: 0x57983ac8
static inline const dev::bytes ON_DECRYPT_FUNCTION_SELECTOR = { 0x57, 0x98, 0x3a, 0xc8 };
#endif
