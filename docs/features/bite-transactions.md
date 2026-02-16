# Introduction

The BITE (Blockchain Integrated Threshold Encryption) protocol is an extension of the SKALE provably secure consensus protocol. Nodes participating in a SKALE consensus committee share a common threshold encryption (TE) public key and possess a set of TE private key shares. The size of the SKALE committee is typically `3t + 1`, where `t` is an integer. A user can encrypt plaintext `P` using the TE public key. To decrypt the resulting ciphertext `C`, a threshold decryption protocol must be executed by a supermajority of `2t + 1` nodes. During the protocol, each node uses its private key share to generate a decryption share, which it then broadcasts to its peers. A total of `2t + 1` decryption shares are required to reconstruct the original plaintext `P`. For example, if the committee size is 100, at least 67 nodes must cooperate to recover `P`.

# Transaction Flow

1. The transaction is encrypted by a client and sent to the blockchain.

2. The transaction is validated and added to the transaction queue. If the `TO` field of the transaction matches the `BITE_MAGIC_ADDRESS`, then the transaction's data is expected to be encrypted and to include the original plaintext `TO` destination address. Otherwise, it is processed as a normal transaction. If the `TO` field matches the `BITE_MAGIC_ADDRESS` but the data field is not RLP encoded, does not match the `EPOCH_ID`, or the cipher cannot be validated, it should be rejected. If the transaction's `EPOCH_ID` doesn't match but it has 2 encrypted AES keys in the payload, this transaction is considered to be sent for epoch `EPOCH_ID + 1`. 

3. If a malicious party includes an invalid BITE transaction in their proposal, such a proposal should be rejected.

4. After a block is decided, consensus runs a decryption round to decrypt all BITE transactions that occurred in the block. If any transaction cannot be decrypted, it is passed to `skaled` in encrypted format.

5. Consensus passes to `skaled` the list of transactions with encrypted data along with the list of the decrypted `data` and `to` fields for corresponding transactions.

6. Transactions are validated once again on the `skaled` side. When a BITE transaction is executed inside the EVM, `skaled` swaps the encrypted data with the decrypted data, as well as the `BITE_MAGIC_ADDRESS` in the `to` field with the original plaintext destination address.

# Ciphertext Format

Each encrypted transaction's data is RLP-encoded as `RLP([EPOCH_ID, ENCRYPTED_BITE_DATA])`, where:

1. **`EPOCH_ID`** – The identifier of the epoch during which the transaction is submitted. This value increments with each committee rotation.

2. **`ENCRYPTED_BITE_DATA`** – The original data encrypted by a client. Can be split as follows:
    - **`Encrypted AES Key(s)`** - The AES key(s) encrypted using the `Threshold Encryption` algorithm. Each key is of fixed size, always 224 bytes, and consists of three parts of sizes 128 bytes, 32 bytes, and 64 bytes, respectively. If two keys are present, then they are keys using committees before and after rotation. If one key is present, it is just the key before the rotation. In case of 2 keys, `skaled` will proceed with the first key if `EPOCH_ID` matches the current epoch ID, and will proceed with the second key and epoch ID `EPOCH_ID + 1` otherwise.
    - **`Encrypted Original Data`** - The original data encrypted with an AES key. Includes the plaintext `TO` address. Its size depends on the original data size.

# Storing in the Database

Decrypted data fields are passed from consensus to `skaled` and later used during EVM execution. Transactions are stored in the database in the format they were sent to `skaled` (encrypted format). However, for every valid BITE transaction, its decrypted data is stored separately and is available through the JSON-RPC API.

# New JSON-RPC Methods

1. **`bite_getCommitteesInfo`** - Returns a JSON object containing the current common `BLS` public key for a chain from a given node as a 128-byte hexadecimal string and current epoch ID. If committee rotation is scheduled for the next 3 minutes, a node returns 2 sets of data (`commonBLSPublicKey` + `epochId`). Note that if a node is in a catch-up state, it may return outdated information.

2. **`bite_getDecryptedTransactionData`** - Receives a transaction hash as an input parameter and returns the decrypted `data` and `to` fields associated with the given transaction. If such a transaction does not exist or does not have any decrypted data associated with it, the method throws an error.
