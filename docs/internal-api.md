## `skale_*` Methods

### skale_protocolVersion
Returns "0.2"

### skale_receiveTransaction
Can be used for receiving broadcasted transactions from other nodes, but currenlty substituted by ZMQ

### skale_shutdownInstance
### skale_getSnapshot
Triggers snapshot serialization on local hard drive

### skale_downloadSnapshotFragment
Can be used after `skale_getSnapshot`

### skale_getSnapshotSignature
### skale_getLatestSnapshotBlockNumber
Returns block number of latest snapshot which can be downloaded. Usually it is the snapshot that was created at 00:00 yesterday.

### skale_getLatestBlockNumber
Same as `eth_blockNumber` but returns decimal literal.

### skale_getDBUsage

## Debug Methods
Enabled by special flag only

### debug_pauseConsensus
Receives parameter `true` or `false` to pause or unpause block processing after consensus.

### debug_pauseBroadcast
Receives parameter `true` or `false` to pause or unpause broadcast.

### debug_forceBlock
Temporary sets consensus' empty block interval to 50 ms, waits until block from consensus, then sets empty block interval back.

### debug_forceBroadcast
### debug_interfaceCall
Allows to pause and resume skaled in some key points of operation

### debug_getVersion
Returns string similar to "3.19.0+commit.859d742c"

### debug_getArguments
Probably returns command-line as string

### debug_getConfig
Returns full config.json

### debug_getSchainName
Returns `chainParams.sChain.name`

### debug_getSnapshotCalculationTime
### debug_getSnapshotHashCalculationTime
### debug_doStateDbCompaction
### debug_doBlocksDbCompaction

## Other Methods
### setSchainExitTime
### debug_getFutureTransactions
