# BITE2 Functionality

This document describes BITE2 (CTX) behavior as implemented in `skaled` and `skale-consensus` integration.

## Overview

BITE2 adds Conditional Transactions (CTXs), which are crafted during execution of a regular transaction and executed asynchronously in a later block.

At a high level:
1. A contract calls `submitCTX` with encrypted and plaintext argument arrays.
2. `submitCTX` creates a signed transaction that targets the calling contract and prefixes the call data with `onDecrypt(bytes[],bytes[])` selector.
3. The CTX is stored in the dedicated BITE2 queue.
4. During block proposal, queued CTXs are emitted first (before regular mempool transactions), up to block gas limit.
5. Consensus decrypts CTX encrypted arguments and passes decrypted data to `skaled` for execution.

## Patch Gating

BITE2 behavior is patch-gated by `Bite2Patch` / `bite2PatchTimestamp`.

When the patch is disabled:
- BITE2-only precompiles are blocked (`submitCTX`, `encryptECIES`, `encryptTE`).
- CTX detection is not enabled in transaction parsing.

When the patch is enabled:
- CTX parsing and processing are enabled.
- Imported committed blocks are expected to carry BITE2 reencryption metadata.

## CTX Identification and Semantics

CTX detection is selector-based: a transaction is treated as CTX when its data starts with
`onDecrypt(bytes[],bytes[])` selector (`0x57983ac8`).

Important behavior:
- `submitCTX` sets CTX destination to the calling contract (`msg.sender` from precompile context).
- `submitCTX` prepends the `onDecrypt` selector automatically.
- A CTX therefore calls `onDecrypt` on the same contract that created it.
- If the callback reverts, gas is still consumed as in regular EVM execution.

## Encrypted Arguments and AAD

Encrypted CTX arguments are validated/decoded via BITE helpers (`abiEncodedArraysToRlp`).
The contract address (`ctx.to`) is used as AAD for threshold-encrypted argument validation.

If ciphertext validation or decoding fails during `submitCTX`, the precompile returns an error code
and no CTX is queued.

## BITE2 Queue Behavior

CTXs are stored in a dedicated `BITE2TransactionQueue`.

Queue properties:
- Temporary CTXs created by currently executing tx are committed only if the tx succeeds.
- On transaction revert, temporary CTXs from that tx are discarded.
- Queue head is consumed in strict order as CTXs are included in blocks.
- On startup, queue is restored from persisted `pendingCTXs` state.

During proposal creation, `skaled` sends queued CTXs first, then regular transactions, all under the
same block gas limit.

## `submitCTX` Precompile

Input ABI:
- `abi.encode(uint256 gasLimit, bytes data)`
- `data = abi.encode(bytes[] encryptedArgs, bytes[] plaintextArgs)`

Output:
- On success: 20-byte CTX sender address.
- On failure: error code encoded as `uint256`.

Notes:
- `gasLimit` must be `> 0` and `<= blockGasLimit`.
- Transaction signature is derived from `getBlockRandom` and current tx index.
- Precompile verifies constructed transaction before queue insertion.
- In read-only mode (`eth_call`, tracing, estimate): precompile returns the computed sender address
  but does not enqueue CTX.

### `submitCTX` Error Codes

Defined in `libethcore/BITECommon.h`:

| Code | Name | Description |
|------|------|-------------|
| 1 | `SUCCESS` | CTX was created and queued successfully. |
| 2 | `INPUT_TOO_SHORT` | Input is shorter than the minimum 96 bytes (`gasLimit` + `offset` + `dataLength`). |
| 3 | `INVALID_DESTINATION` | Caller address (`msg.sender`) is the zero address. |
| 4 | `INVALID_GAS_LIMIT` | Gas limit is zero or exceeds the block gas limit. |
| 5 | `DATA_OFFSET_OUT_OF_BOUNDS` | ABI data offset points beyond input bounds. |
| 6 | `DATA_TOO_SHORT` | Input is too short to contain the declared data length at the given offset. |
| 7 | `ABI_TO_RLP_CONVERSION_FAILED` | `abiEncodedArraysToRlp` threw an exception converting encrypted/plaintext argument arrays to RLP. |
| 8 | `ABI_TO_RLP_UNKNOWN_ERROR` | `abiEncodedArraysToRlp` threw an unknown (non-std) exception. |
| 9 | `INVALID_SIGNATURE` | `getBlockRandom` precompile call failed; could not derive CTX signature. |
| 10 | `INVALID_TRANSACTION` | Constructed CTX transaction failed internal validity checks. |
| 11 | `COULD_NOT_VERIFY_TRANSACTION` | Seal engine `verifyTransaction` rejected the CTX (e.g., signature or gas validation failure). |

## Additional BITE2-Related Precompiles

### `encryptTE`

Threshold-encrypts input using network TE context and uses the calling contract address as AAD for threshold encryption.

Input ABI:
- `abi.encode(bytes data)`
- Layout: `[offset(32)] [dataLength(32)] [data(N padded to 32)]`
- Offset must equal `32`.

Output:
- On success: RLP-encoded list `[epochId, ciphertextBytes]`.
- On failure: error code encoded as `uint256`.

Read-only behavior (`eth_call`, tracing, estimate):
- Encryption counter is not advanced; repeated calls in the same block context return the same
  ciphertext.

Error codes:

| Code | Meaning |
|------|---------|
| 1 | Input too large (`> 64 KB`) |
| 2 | Input too short (`< 64 bytes`) |
| 3 | Input size not 32-byte aligned |
| 4 | Invalid ABI offset (`!= 32`) |
| 5 | Data length out of bounds |
| 6 | Trailing padding contains non-zero bytes |

### `encryptECIES`

Encrypts input using a user-supplied secp256k1 public key via ECIES-CBC.

Input ABI:
- `abi.encode(bytes data, bytes32 x, bytes32 y)`
- Layout: `[offset(32)] [pubKeyX(32)] [pubKeyY(32)] [dataLength(32)] [data(N padded to 32)]`
- Offset must equal `96`.
- `x` and `y` are the uncompressed secp256k1 public key coordinates of the recipient.

Output:
- On success: raw ciphertext bytes laid out as `[IV(16)] [compressedEphemeralPubKey(33)] [ciphertext]`.
- On failure: error code encoded as `uint256`.

Read-only behavior (`eth_call`, tracing, estimate):
- Encryption counter is not advanced; repeated calls in the same block context return the same
  ciphertext.

Error codes:

| Code | Meaning |
|------|---------|
| 1 | Input too large (`> 64 KB`) |
| 2 | Input too short (`< 128 bytes`) |
| 3 | Input size not 32-byte aligned |
| 4 | Invalid ABI offset (`!= 96`) |
| 5 | Data length out of bounds |
| 6 | Trailing padding contains non-zero bytes |
| 7 | Invalid secp256k1 public key |
| 8 | Encryption failed / empty result |

## Ordering Constraints in Consensus

When BITE2 patch is enabled, consensus parsing expects CTXs at the beginning of proposed
transaction list; parsing switches to regular transactions at the first non-CTX entry.

This matches `skaled` proposal behavior that emits queued CTXs before regular mempool txs.

## Persistence and Observability

`skaled` stores and exposes CTX lineage for debugging and recovery:
- `craftedCTXs`: CTX hashes crafted by each tx in a block.
- `ctxOrigin`: origin tx hash for a CTX.
- `pendingCTXs`: persisted queue of not-yet-executed CTXs with origins.

Related RPC methods:
- `bite_getCraftedCtxs`
- `bite_getCtxOrigin`
- `debug_getPendingBITE2Transactions`

## End-to-End Flow (Reference)

1. Contract executes in block `N` and calls `submitCTX`.
2. `submitCTX` validates input, constructs CTX (`to = caller`, `data = onDecrypt(...)`), and queues
	it if not read-only.
3. At proposal time, queued CTXs are inserted first, subject to block gas limit.
4. Consensus decrypts encrypted CTX arguments in the same decryption pipeline used for encrypted
	transactions.
5. `skaled` executes CTX with decrypted payload.
6. CTX metadata (`craftedCTXs`, `ctxOrigin`, `pendingCTXs`) is persisted for tracing/restart.
