# Skaled and Consensus Databases

## Introduction

This document outlines the structure, functionality, and rotation process of the`skaled` and `consensus` databases.

## Skaled

### Core Nodes

#### `state.db`

This database stores information about all accounts in the network.

Each account contains:
- `balance`
- `nonce`
- `code hash` (default if the account is not a user contract)
- `storage used` (0 if the account is not a user contract)

In addition, `state.db` stores the code of every deployed contract.

This database is **never rotated**, but it is limited by `contractStorageLimit`. The total storage used by all contracts in the network must not exceed this limit.

#### `blocks_and_extras.db`

Detailed information about this database can be found here ().


### Archive Nodes

Historic nodes have two additional databases: `historic_state` and `historic_roots`.

#### `historic_state`

Stores all historical data for every account on a given chain. Every time an account's data changes (e.g., nonce, balance, or storage), the new data is written without deleting the old data. The previous data remains accessible if the `root` value is known. This database is **not rotated**.

#### `historic_roots`

Maps block numbers to their corresponding `root` values. This database is also **not rotated**.


## Consensus

`totalStorageLimitBytes` is passed to the Consensus Engine by `skaled` during initialization:

```cpp
ConsensusEngine::ConsensusEngine(block_id _lastId, uint64_t _totalStorageLimitBytes)
```

### Rotated Databases

Consensus has **13 rotated databases**. Each database is divided into **four LevelDB shards**:

```cpp
LEVELDB_SHARDS = 4
```

The size of each rotated database is computed as follows:

```cpp
unit = _totalStorageLimitBytes / (LEVELDB_SHARDS * (1000 + 10 * 10 + 100));

BLOCK_DB_SIZE = 1000 * unit;
RANDOM_DB_SIZE = 10 * unit;
PRICE_DB_SIZE = 10 * unit;
PROPOSAL_HASH_DB_SIZE = 10 * unit;
PROPOSAL_VECTOR_DB_SIZE = 10 * unit;
OUTGOING_MSG_DB_SIZE = 10 * unit;
CONSENSUS_STATE_DB_SIZE = 10 * unit;
BLOCK_SIG_SHARE_DB_SIZE = 10 * unit;
DA_SIG_SHARE_DB_SIZE = 10 * unit;
DA_PROOF_DB_SIZE = 10 * unit;
BLOCK_PROPOSAL_DB_SIZE = 100 * unit;
INTERNAL_INFO_DB_SIZE = 1 * unit;
#ifdef BITE
INCOMING_MSG_DB_SIZE = 1 * unit;
TE_DECRYPTION_DB_SIZE = 8 * unit;
#else
INCOMING_MSG_DB_SIZE = 9 * unit;
```

#### Example

If `totalStorageLimitBytes` = 1,000,000,000:

Then `unit` = 208,333  
Thus, each shard in `BLOCKS_DB` will be:

```
BLOCK_DB_SIZE = 208,333,000 bytes
```

### Archive and Indexer Nodes

For archive and indexer nodes, the consensus databases have the same structure as core nodes, with one exception:

- The `blocks.db` database **does not rotate** blocks out. Instead, it stores them in a 4th database after the first three shards reach their capacity.


## Extensions

Each shard is suffixed with an extension: `0`, `1`, `2`, `3`, `4`, etc.

At any point in time, active shards might include, for example: `5`, `6`, `7`, `8`.

When a new shard `9` is created, the oldest (`5`) may be dropped.


## `skale_getDBUsage` Call

The `skale_getDBUsage` method is available via the `skaled` JSON-RPC API. It returns storage usage (in bytes) for **all** `skaled` and `consensus` databases, without per-shard breakdowns for rotated databases.

### Example Output

```json
{
  "consensusDBUsage": {
    "block_proposal.db_disk_usage": 47061467,
    "block_sigshare.db_disk_usage": 123,
    "blocks.db_disk_usage": 253165303,
    "consensus_state.db_disk_usage": 123,
    "da_proof.db_disk_usage": 5746940,
    "da_sigshare.db_disk_usage": 7168259,
    "incoming_msg.db_disk_usage": 123,
    "internal_info.db_disk_usage": 166,
    "outgoing_msg.db_disk_usage": 1639766,
    "price.db_disk_usage": 1007933,
    "proposal_hash.db_disk_usage": 6750622,
    "proposal_vector.db_disk_usage": 1523550,
    "random.db_disk_usage": 123
  },
  "skaledDBUsage": {
    "blocks.db_disk_usage": 333332200,
    "contractStorageUsed": 984433472,
    "pieceUsageBytes": 162816792,
    "state.db_disk_usage": 2396572785
  }
}
```


