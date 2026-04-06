# BITE2 Overview

BITE V2 extends BITE V1 by enabling smart contracts to store encrypted data and request decryption directly from within Solidity and the EVM.
Smart contract developers can invoke the threshold decryption function provided by the BITE Solidity library, passing the encrypted data as input. The BITE protocol then performs decryption using a consensus committee, identical to the mechanism used in BITE V1.
When the decryption function is called in block X, the decryption process is executed as part of SKALE consensus for block X + 1. Upon completion, the decrypted data is automatically delivered back to the requesting smart contract via a special-purpose transaction included in block X + 1.
With BITE V2, each block can include Conditional Transactions (CTXs) — transactions initiated by smart contracts execution in the previous block.
CTXs enable smart contracts to decrypt data and perform actions automatically on this data.

# Conditional Transactions (CTXs)

## Overview 

With BITE V2, each block can include Conditional Transactions (CTXs) — transactions initiated by smart contracts execution in the previous blocks.
CTXs enable smart contracts to decrypt data and perform actions automatically on this data.

## Encrypted argument spec

Each CTX's encrypted argument has the same RLP format as for BITE Phase 1 encrypted data field but use smart contract address as additional authentication data (AAD) for encryption. When the data is decrypted it uses `ctx.to` value as AAD, and if decryption wasn't successful the transaction will not be executed. However, user is still charged for submitting CTX.

## Smart Contract Requirements for Working with CTX (OnDecrypt)

To interact with CTX, a smart contract must implement the onDecrypt() callback function.
If a smart contract defines an onDecrypt() function, it can initiate a decryption in Block N. The decryption results are passed to the onDecrypt() function in the next blocks with respect to block gas limit.
This enables an asynchronous execution model where:
- the decryption request is triggered in one block
- the result is securely returned in the next blocks
- and the contract can continue its logic using the decrypted data inside onDecrypt().

## BITE2 Transaction queue

CTXs created in `submitCTX` precompiled are stored in separate `BITE2TransactionQueue`. When a node creates transaction list for proposal, it picks CTXs until block gas limit is reached or there are no CTXs left in `BITE2TransactionQueue`. If all CTXs couldn't fit into one block, they could be scheduled for the next blocks.

## CTX origin and backup

To support better tracking CTXs tracking, UX and backup options skaled stores additional information in its database:
- `craftedCTXs` list for every block - CTXs created by every transaction in a block. 
- `ctxOrigin` for every CTX added to blockchain.
- `pendingCTXs` - list of CTXs that were not added to blockchain yet but were already scheduled for execution and corresponding `CTXOrigin`s. 

## BITE V2 precompiled contracts

### submitCTX 

Creates a CTX from the input parameters and submits it into BITE2TransactionQueue. BITE2TransactionQueue is storing BITE2 transactions created during execution. SubmitCTX returns an address used to send the associated BITE2 confidential transaction (CTX). SubmitCTX precompiled smart contract that will be called from external smart contract. SubmitCTX receives 2 arguments encoded using abi.encode - uint256 gasLimit, bytes data, where data is encoded using abi.encode(bytes[] encryptedArgs, bytes[] plaintextArgs).
Input format:
- bytes `_in` — ABI-encoded parameters: abi.encode(uint256 gasLimit, bytes data). Here, `gasLimit` is the limit for the scheduled CTX, and `data` is the payload to be passed within the CTX (ABI-encoded as  abi.encode(bytes[] encryptedArgs, bytes[] plaintextArgs)).
Output format:
- bytes `_out` — address (20 bytes).

### submitCTX error codes

`submitCTX` raises different error codes depending on the actual error that happenned during execution:
- SUCCESS = 1 - no errors during execution
- INPUT_TOO_SHORT = 2 - every CTX payload must be at least 96 bytes long - gasLimit(32) + offset_to_data(32) + data_length(32) + data_bytes
- INVALID_DESTINATION = 3 - `submitCTX` call is executed on behalf `ZeroAddress`. means an error happened before on EVM side
- INVALID_GAS_LIMIT = 4 - gasLimit provided in `submitCTX` call is negative or exceeds standard block gas limit
- DATA_OFFSET_OUT_OF_BOUNDS = 5 - couldn't read data as offset is too big - means data encoding is invalid 
- DATA_TOO_SHORT = 6 - couldn't read data as data length os too big - means data encoding is invalid 
- ABI_TO_RLP_CONVERSION_FAILED = 7 - error while converting abi encoded arrays to rlp
- ABI_TO_RLP_UNKNOWN_ERROR = 8 - unknown error while converting abi encoded arrays to rlp 
- INVALID_SIGNATURE = 9 - couldn't generate random signature for CTX as `getBlockRandom` call finished with error
- INVALID_TRANSACTION = 10 - exception occured when constructing CTX object, probably because of wrong rlp format
- COULD_NOT_VERIFY_TRANSACTION = 11 - CTX signature or gas limit are not verified - provided gas limit either not enough to submit transaction or extends block gas limit

## CTX requirements

To interact with CTX, a smart contract must implement the onDecrypt() callback function.
If a smart contract defines an onDecrypt() function, it can initiate a decryption in Block N. The decryption results are passed to the onDecrypt() function in the next blocks.
This enables an asynchronous execution model where:
- the decryption request is triggered in one block
- the result is securely returned in the next blocks
- and the contract can continue its logic using the decrypted data inside onDecrypt().

## CTX Flow

- A Smart Contract in block N calls `submitCTX` precompile passing an encryptedArguments array and an plaintextArguments array of plaintext arguments and gasLimit for the future CTX transaction.
- `submitCTX` precompile creates signature for CTX, verifies encrypted payload and sets `ctxOrigin`. if CTX creation was successful, `submitCTX` returns `CTX` sender address.
- A CTX transaction is added into `BITE2TransactionQueue` and scheduled for execution in the next blocks. CTX transactions are placed in front of regular transactions in the block with respect to block gas limit.
- In order for CTX to pass, the smart-contract has to top up the wallet W of `CTX` sender. The wallet W is generated based on RNG and can be predicted at the time CTX is submitted to submitCTX precompiled contract.
- CTX transaction to field is the smart-contract that originated it. The smart-contract sends a transaction to itself.
- CTX transaction always calls onDecrypt function of the smart-contract that originated it.
- CTX transactions are decrypted in the same batch decrypt as the BITE Phase 1 transaction, during finalization of block N.
- `ctx.to` address is used as AAD to verify that data is only decrypted by the contract that owns it.
- when CTX arrives from consensus to skaled for execution, skaled verifies `CTXOrigin` and decryption status. if decryption status is successful, decrypted data is added into transaction body and transaction is executed.

## Confidential token

### Confidential token precompiled contracts

#### encryptTE

This process encrypts data using the network's BLS threshold encryption public key, employing deterministic seeding via block randomness and utilizing the smart contract address as Additional Authentication Data (AAD).

Nodes generate a common random value, R, during the consensus round.
R is used as the seed for the ThresholdEncryption algorithm to guarantee that all nodes generate the identical ciphertext for a given plaintext.
Nodes execute the standard ThresholdEncryption algorithm (as described in this document), using the ConfidentialToken contract address as the AAD for the ThresholdEncryption algorithm input.
Input format:
- bytes `_in` — data to be encrypted.
- data - Plaintext data to encrypt (max 64KB)
Output format:
- bytes `_out` — encrypted ciphertext. Serialized Ciphertext bytes that can only be decrypted by the network's threshold signature holders.

#### encryptECIES

Encrypts the provided data using the account's `secp256k1` public key. A comprehensive description is provided here and further elaborated in the Mathematical Model section below. This scheme allows storing data that can be decrypted by its owner in a smart contract.
Input format:
- bytes `_in` — data to be encrypted and 64-byte data consisting of the `x` and `y` coordinates of the user's public key: abi.encode(bytes data, bytes32 pubKeyX, bytes32 pubKeyY). 
- data - Plaintext data to encrypt (max 64KB)
- pubKeyX,pubKeyY - Recipient's uncompressed public key coordinates
Output format:
- bytes `_out` — ECIES-encrypted ciphertext (AES-256-CBC with PKCS7 padding) alongside associated data: [ IV (16 bytes) ] [ Ephemeral Public Key (33 bytes) ] [ Ciphertext (N bytes) ] .

### Confidential token flow

The following describes the flow for a confidential token transfer (sender: Alice, receiver: Bob, amount: `N`, smart contract address: `A`).
- Alice encrypts amount using the bite-ts library.
- Alice generates the payload for transfer(to, ENCRYPTED_AMOUNT) and encrypts it using the bite-ts library. A transaction with this payload is added to the transaction queue, decrypted by consensus, and then included in a block (standard BITE transaction flow).
- The decrypted transaction is included in the block (the recipient is now plaintext). However, Alice’s and Bob’s balances remain encrypted as well as the transfer amount.
- The token contract calls BITE.decryptAndExecute, which should:
- Call the submitCTX precompile with the following parameters: abi.encode(STANDARD_GAS_LIMIT_FOR_TRANSFER_TRANSACTION, abi.encode(bytes[] encryptedArgs, bytes[] plaintextArgs)), where
encryptedArgs = [Alice’s balance, Bob’s balance, amount, …] and plaintextArgs = [Alice’s address, Bob’s address, …].
- submitCTX adds a new CTX to the BITE2 transaction queue and returns the address (CTX_SENDER), which will be used to submit the CTX.
- Top up the balance of CTX_SENDER.
- The CTX is pulled by consensus; all encryptedArgs are decrypted, and the CTX is included in the next block.
- The token contract executes the transfer using the decrypted balances and associated metadata.
- The token contract calls encryptTE(ALICE_UPDATED_BALANCE), encryptECIES(ALICE_UPDATED_BALANCE,ALICE_PUBLIC_KEY), encryptTE(BOB_UPDATED_BALANCE), and encryptECIES(BOB_UPDATED_BALANCE, BOB_PUBLIC_KEY)), and stores the re‑encrypted balances in state.
