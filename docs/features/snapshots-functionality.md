
# Snapshots Functionality

## Why do we need it

As known, node rotation is one of the main features of the SKALE Network. Briefly, if node A leaves a schain S for any reason, another node B will be chosen to replace it. Once node B is chosen, it needs all the information about previous blocks, transactions, etc., on schain S. SKALE solves this issue by periodically taking snapshots of the file system on each node, so they can share the latest snapshot with the rotated node B. Also, a node can be restarted from a snapshot if it was offline for a long period and cannot catch up with other nodes using the SKALE-CONSENSUS catch-up algorithm.

## How do we implement it

Firstly, all nodes in the SKALE NETWORK support the Btrfs file system, and we use Btrfs to take snapshots.

### Assumptions:

- The first block on the schain occurred at `TfirstBlock`
- A node takes a snapshot every `TsnapshotInterval` seconds (a configurable number stored in the schain config, so it is similar for all nodes in the schain)

Assume `k` snapshots were already taken. Let's see when the `k+1`-th snapshot will be done and ready to be used:

The `k+1`-th snapshot will be done once another block’s timestamp crosses a boundary.

![Snapshot Timing](https://github.com/skalenetwork/internal-support/blob/main/docs/specifications/.doc-images-diagrams/images/skaled_snapshot.png)

- The node updates `last_snapshoted_block_with_hash` with the `k-th` snapshot block number
- If it is time to take a snapshot and the node already has 3 snapshots stored, it deletes the oldest one
- The node creates a snapshot `S_latest`
- The node updates `last_snapshot_creation_time` with the block’s timestamp
- The node calculates the hash of `S_latest` in a separate thread (computing the hash of every file in this snapshot, including filestorage), assuming it will be successfully calculated before the next snapshot is done. So the `k+1`-th snapshot will be ready to be used only when the `k+2`-th snapshot is being performed
- The node updates the `stateRoot` field with the `k-th` snapshot hash

![Process Diagram](http://www.plantuml.com/plantuml/png/VL6xReCm5DtvYgDCxH14qWaaKDIfS_C5BhZOAlWY-rAfVr_isag8A9DzZyxZFYwEXULv7B9fUNMh9s4OXzU1sY-qLWhMP0uG3XyUMhEkHanE4Q-9Bg9RNCrGmYcqD4upQzC7XOUokiQeEwDxnQZhp1fY2VgMDYO3VqsTYw9OYYiBbdWduWhgv35fkY3A0l_Of3ugt4qZktldHF5-yPlytvQU5cTVuKaRJ9PwAvrxlkKaDOuDLA0dawsiC2HkaoU0Fk5MsSaVkpiuxU_WKLeXp_DjT2UcBClzc8UZXTiUcsWUjwSTh1JTp_yvBnidFQzoBijl7eTV)

#### Snapshot Downloading

To start from a snapshot, a node must be sure that the snapshot is valid. To prevent downloading snapshots from malicious nodes, the following procedure was designed:

- A node chooses a random node from the schain and asks it for the last snapshotted block number
- A node asks all nodes from the schain to send a snapshot hash signed with the corresponding BLS key (if a node is starting from a snapshot after rotation, then old nodes should use their old BLS keys, and the new node should use the old `BLSCommonPublicKey` and `BLSPublicKey` to verify signatures). New keys are generated once another DKG is done on this schain.
- Once a node receives all hashes and signatures, it tries to choose a hash H that is identical on at least ⅔ + 1 nodes, then collects their BLS signatures into one and verifies it. If one of the last three steps fails, the node does not start.
- A node chooses a random node from those ⅔ + 1 nodes and downloads the snapshot from it, computes its hash, and checks whether it is similar to H. If it is, the node starts from this snapshot. Otherwise, it tries to download a snapshot from another node.

## Shared Space

Shared space is the directory on a SKALE node used to download or upload snapshots. This directory is shared by all SKALE chains on one node. While one node is using the shared space, other nodes cannot access it.

The shared space directory is created when the first SKALE chain on the node is initialized, and this directory is never deleted.

If a node downloads a snapshot:

- The shared space directory is locked
- `skaled` creates file `N` in the shared space directory, where `N` is the snapshot block number being downloaded
- `skaled` downloads the snapshot in chunks, saves it into shared space, then moves it to the snapshots directory and restores it into the data directory
- After a successful restore, the shared space is unlocked

If a node uploads a snapshot:

- If the last snapshot uploading procedure started later than `SNAPSHOT_DOWNLOAD_TIMEOUT`, the shared space directory is cleaned and a new snapshot uploading procedure starts. Otherwise, `skaled` returns an error.
- The shared space directory is cleaned and unlocked
- The shared space directory is locked. If the lock isn’t successful, `skaled` returns an error
- `skaled` creates a snapshot file and stores it in the shared space directory
- `skaled` uploads the snapshot to another node in chunks
- If there are no requests for another chunk for `SNAPSHOT_DOWNLOAD_INACTIVE_TIMEOUT`, `skaled` unlocks the shared space and deletes the snapshot file. `skaled` is ready to upload another snapshot immediately afterward

## Snapshot Downloading Procedure

Client:

1. Asks node 1 for `skale_getLatestSnapshotBlockNumber`
2. In case of error, chooses the next node
3. If all nodes fail, exits with an error
4. Asks all nodes for `skale_getSnapshotSignature(number)`
5. If not enough signatures are obtained to verify the threshold signature, repeat from step 2
6. If enough, selects a random node from those who sent correct signatures
7. Tries to download the snapshot from it via `skale_getSnapshot(number)`
8. In case of error, chooses the next node and tries again
9. If all attempts fail, repeat from step 2

Snapshot download steps:

1. Client calls `skale_getSnapshot(number)`
2. Server creates a serialized snapshot in the shared space
3. Server reports file size and max chunk size in the response
4. If `skale_getSnapshot(number)` is called a second time, an error is returned
5. Client makes multiple calls to `skale_downloadSnapshotFragment(offset, size)`
6. Server frees shared space after 1 hour or after 1 min of inactivity

> **NOTE:** Step 2 can take up to 20 minutes, potentially causing an RPC timeout

After downloading a regular snapshot, a node downloads a snapshot for block 0 if it does not already have it. This is needed for correct information about the genesis block. Archive and indexer nodes can also start from the snapshot for block 0 and catch up. The download procedure is the same.

Proposed new steps:

1. Client calls `skale_getSnapshot(number)`
2. Server starts creating a serialized snapshot asynchronously and sends a response immediately
3. If a snapshot creation has already started (for the same block number), the response is the same
4. If a snapshot creation has already started for a different block number, an error is returned
5. Client retries periodically and switches to another node after a 1-hour timeout
6. If successful, the same `skale_downloadSnapshotFragment(offset, size)` is used
7. Client must retry frequently enough to avoid server timeout (1 min inactivity)

> **NOTE:** This introduces incompatibility with current implementations. If an "old" node tries to download a snapshot from a "new" node, it will fail and switch, potentially blocking shared space until another compatible node is found.

> **NOTE:** `stateRoot` is used to detect software issues. If the `stateRoot` of a new block does not match that of a node, a restart from a snapshot is triggered.

## Archive and Indexer Nodes

Snapshot functionality is extended for archive and indexer nodes for better stability. Differences include:

- `historic_roots` and `historic_state` submodules exist for archive nodes
- Archive/indexer nodes calculate the snapshot hash only for core subvolumes, ensuring the same `stateRoot` as core nodes
- Archive/indexer nodes can only send snapshots to other archive/indexer nodes, though this could be extended
- No limit on snapshot downloading time for archive/indexer nodes

## JSON-RPC Snapshot Methods

### `skale_getSnapshot`

**Parameters**

- `blockNumber`: integer, block number
- `autoCreate`: Boolean, create snapshot if it does not exist

**Returns**

- `dataSize`: integer, size of snapshot in bytes
- `maxAllowedChunkSize`: integer, max chunk size in bytes

**Example**

```bash
curl -X POST --data '{ "jsonrpc": "2.0", "method": "skale_getSnapshot", "params": { "blockNumber": 68,  "autoCreate": false, "forArchiveNode": false }, "id": 73 }'
```

```json
{ 
    "id": 73,
    "dataSize": 12345,
    "maxAllowedChunkSize": 1234
}
```

### `skale_downloadSnapshotFragment`

Returns a snapshot fragment.

**Parameters**

- `blockNumber`: block number or "latest"
- `from`: offset in bytes
- `size`: integer, size in bytes
- `isBinary`: Boolean

**Returns**

- `size`: integer, size in bytes
- `data`: base64 encoded snapshot data

**Example**

```bash
curl -X POST --data '{ "jsonrpc": "2.0", "method": "skale_downloadSnapshotFragment", "params": { "blockNumber": "latest", "from": 0, "size": 1024, "isBinary": false }, "id": 73 }'
```

```json
{ 
    "id": 73,
    "size": 1234,
    "data": "base64 here"
}
```

### `skale_getSnapshotSignature`

Returns a snapshot hash signature for a given block.

**Parameters**

- `blockNumber`: integer

**Returns**

- `X`, `Y`: signature components
- `helper`: for BLS verification
- `hash`: snapshot hash
- `signerIndex`: node's index

**Example**

```bash
curl -X POST --data '{ "jsonrpc": "2.0", "method": "skale_getSnapshotSignature", "params": { "blockNumber": 14 }, "id": 73 }'
```

```json
{ 
    "id": 73,
    "X": "3213213131313566131315664653132135156165496800065461326",
    "Y": "3164968456435613216549864300564646631198986113213166",
    "helper": 1,
    "hash": "aef45664dcb5636",
    "signerIndex": 1
}
```

### `skale_getLatestSnapshotBlockNumber`

Returns the latest snapshotted block number.

**Parameters**

None

**Returns**

- `blockNumber`: latest snapshotted block

**Example**

```bash
curl -X POST --data '{ "jsonrpc": "2.0", "method": "skale_getLatestSnapshotBlockNumber", "params": { }, "id": 73 }'
```

```json
{ 
    "id": 73,
    "blockNumber": 15
}
```
