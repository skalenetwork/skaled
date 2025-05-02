# Introduction

The BITE (Blockchain Integrated Threshold Encryption) protocol is an extension of the SKALE provably secure consensus protocol. Nodes participating in a SKALE consensus committee share a common threshold encryption (TE) public key and possess a set of TE private key shares. The size of the SKALE committee is typically `3t + 1`, where `t` is an integer. A user can encrypt plaintext `P` using the TE public key. To decrypt the resulting ciphertext `C`, a threshold decryption protocol must be executed by a supermajority of `2t + 1` nodes. During the protocol, each node uses its private key share to generate a decryption share, which it then broadcasts to its peers. A total of `2t + 1` decryption shares are required to reconstruct the original plaintext `P`. For example, if the committee size is 100, at least 67 nodes must cooperate to recover `P`.

# Ciphertext Format

Every encrypted transaction's data can be split into four parts:

1. **`MAGIC_NUMBER`** - `f3a9c7b1e4d5f28c7b1e9a3f5d2c8b00` - Allows the nodes running a chain to identify a transaction as a BITE transaction and validate and handle it during the consensus phase.

2. **`EPOCH_ID`** - 8-byte integer - The number of the epoch when a transaction is sent. The number is incremented with every committee rotation.

3. **Encrypted `AES` Key** - The `AES` key encrypted using the `Threshold Encryption` algorithm. This is of fixed size, always 224 bytes, and consists of three parts of sizes 128 bytes, 32 bytes, and 64 bytes, respectively.

4. **Encrypted Original Data** - The original data encrypted with an `AES` key. Its size depends on the original data size.

# Transaction Flow

1. The transaction is encrypted by a client and sent to the blockchain.

2. The transaction is validated and added to the transaction queue. If the first 16 bytes of the transaction's data do not match the `MAGIC_NUMBER`, it will be processed as a regular transaction. If the transaction matches the `MAGIC_NUMBER` but does not match the `EPOCH_ID` or the cipher cannot be validated, it should be rejected.

3. If a malicious party includes an invalid BITE transaction in their proposal, such a proposal should be rejected.

4. After a block is decided, consensus runs a decryption round to decrypt all BITE transactions that occurred in the block. If any transaction cannot be decrypted, it is passed to `skaled` in encrypted format.

5. Consensus passes to `skaled` the list of transactions with encrypted data along with the list of the decrypted data fields for corresponding transactions.

6. Transactions are validated once again on the `skaled` side. When a BITE transaction is executed inside the EVM, `skaled` swaps the encrypted data with the decrypted data.

# Storing Inside the Database

Decrypted data fields are passed from consensus to `skaled` and later used during EVM execution. Transactions are stored in the database in the format they were sent to `skaled` (encrypted format). However, for every valid BITE transaction, its decrypted data is stored separately and is available through the JSON-RPC API.

# New JSON-RPC Methods

1. **`skale_getCommonPublicKey`** - Returns the current common `BLS` public key for a chain from a given node as a 128-byte hexadecimal string. Note that if a node is in a catch-up state, it may return an outdated key.

2. **`skale_getDecryptedTransactionData`** - Receives a transaction hash as an input parameter and returns the decrypted data associated with the given transaction. If such a transaction does not exist or does not have any decrypted data associated with it, the method throws an error.
