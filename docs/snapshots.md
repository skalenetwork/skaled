<!-- SPDX-License-Identifier: (GPL-3.0-only OR CC-BY-4.0) -->

# Snapshots

-   [Introduction](#introduction)
-   [Design](#design)    
-   [JSON-RPC Snapshot Methods](#json-rpc-snapshot-methods)
    -   [skale_getSnapshot](#skale_getsnapshot)
    -   [skale_downloadSnapshotFragment](#skale_downloadsnapshotfragment)
    -   [skale_getSnapshotSignature](#skale_getsnapshotsignature)
    -   [skale_getLatestSnapshotBlockNumber](#skale_getlatestsnapshotblocknumber)

## Introduction

One of the main features of SKALE Network is node rotation, and this is performed through a BLS Snapshot process. Briefly, if node `A` leaves a SKALE Chain for any reason (e.g. random rotation, node exit, etc.), another node `B` will be chosen to replace it.  Once node `B` is chosen, it needs all the information about previous blocks, transactions, etc. from the SKALE chain. 

SKALE Network solves this by periodically performing snapshots of the SKALE chain file system on each node so that nodes can share the latest snapshot with incoming node `B`.

Additionally, a node can be restarted from a snapshot if the node was offline for a long period of time, if it cannot catch up with other nodes using SKALE Consensus' catch-up algorithm.

## Design

Skaled uses the btrfs file system to create snapshots.

Assumptions:
1. First block on SKALE chain occurred at T<sub>firstBlock</sub>
2. node does snapshot every T<sub>snapshotInterval</sub> seconds (configurable number, stored in SKALE chain config, so it is similar for all nodes in SKALE chain )

Assume `k` snapshots were already done. Let's see when `k+1`-th will be done and ready to be used:
1. `k+1`-th snapshot will be done once another block’s B timestamp crosses boundary (\[T<sub>firstBlock</sub> / T<sub>snapshotInterval</sub>] + 1) \* T<sub>snapshotInterval</sub>.
2. Node updates `last_snapshoted_block_with_hash` with `k`-th snapshot block number.
3. If it is time to do snapshot and node already has 3 snapshots stored it deletes the latest of them.
4. Node creates a snapshot `S_latest`.
5. Node updates `last_snapshot_creation_time with` `B`'s timestamp.
6. Node calculates `S_latest`'s hash in separate thread (computes hash of every file in this snapshot including filestorage), assuming it will be successfully calculated before next snapshot is done. So `k+1`-th snapshot will be ready to be used only when `k+2`-th snapshot will be performing.
7. Node updates `stateRoot` field with `k`-th snapshot hash

To start from a snapshot, a node must confirm whether a snapshot is valid. To prevent downloading of snapshots from malicious nodes, the following procedure was designed:

1.  Node `A` chooses a random node from SKALE chain and requests the last snapshot block number.
2.  Node `A` requests all nodes from the SKALE chain to send a snapshot hash signed with its corresponding BLS key.
3.  Once node `A` receives all hashes and signatures, `A` tries to choose a hash `H` similar on at least 2/3 + 1 nodes, then `A` collects their BLS signatures into one and verifies it. (If steps 1 – 3 fails, a node will not start.)
4.  Node `A` chooses a random node from those 2/3 + 1 nodes and downloads a snapshot from it, computes its hash and confirms whether it is similar to `H`. If it is similar then a node starts from this snapshot, otherwise node `A` will attempt to download a snapshot from another node.

NOTE: `stateRoot` is needed to determine whether there are any node software issues. Each time a new block is passed from consensus, a node compares its own stateRoot with the stateRoot of the incoming block (so snapshot hashes of the last block from different nodes are compared). If the stateRoots fail to match, then a software issue is assumed and the node must restart from a snapshot.

## JSON-RPC Snapshot Methods

### skale_getSnapshot

Parameters

-   `blockNumber`: integer, a block number
-   `autoCreate`: `Boolean`, create snapshot if it does not exist

Returns

-   `dataSize`: integer, the size of snapshot in bytes
-   `maxAllowedChunkSize`: integer, the maximum chunk size in bytes

Example

```sh
// Request

curl -X POST --data '{ "jsonrpc": "2.0", "method": "skale_getSnapshot", "params": { "blockNumber": 68,  "autoCreate": false }, "id": 73 }'

// Result
{ 
    "id": 73,
    "dataSize": 12345,
    "maxAllowedChunkSize": 1234
}
```

### skale_downloadSnapshotFragment

Returns a snapshot fragment.

Parameters

-   `blockNumber`: a block number, or the string "latest"
-   `from`: a block number
-   `size`: integer, the size of fragment in bytes
-   `isBinary`: `Boolean`

Returns

-   `size`: integer, the size of chunk in bytes
-   `data`: `base64`, btrfs data

Example

```sh
// Request

curl -X POST --data '{ "jsonrpc": "2.0", "method": "skale_downloadSnapshotFragment", "params": { "blockNumber": "latest", "from": 0, "size": 1024, "isBinary": false }, "id": 73 }'

// Result
{ 
    "id": 73,
    "size": 1234,
    "data": "base64 here"
}
```

### skale_getSnapshotSignature

Returns signature of snapshot hash on given block number.

Parameters

-   `blockNumber`: integer, a block number

Returns

-   `X`: string, X coordinate of signature
-   `Y`: string, Y coordinate of signature
-   `helper`: integer, minimum number such that Y=(X+helper)^3 is a square in F<sub>q</sub>
-   `hash`: string, hash of a snapshot on given block number
-   `signerIndex`: integer, receiver's index in SKALE chain

Example

```sh
// Request

curl -X POST --data '{ "jsonrpc": "2.0", "method": "skale_getSnapshotSignature", "params": [ 14 ], "id": 73 }'

// Result
{ 
    "id": 73,
    "X": 3213213131313566131315664653132135156165496800065461326,
    "Y": 3164968456435613216549864300564646631198986113213166,
    "helper": 1,
    "hash": aef45664dcb5636,
    "signerIndex": 1
}
```

### skale_getLatestSnapshotBlockNumber

Returns the latest snapshotted block's number.

Parameters

NULL

Returns

-   `blockNumber`: integer, the latest snapshotted block's number

Example

```sh
// Request

curl -X POST --data '{ "jsonrpc": "2.0", "method": "skale_getLatestSnapshotBlockNumber", "params": { }, "id": 73 }'

// Result
{ 
    "id": 73,
    "blockNumber": 15
}
```
