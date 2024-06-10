<!-- SPDX-License-Identifier: (GPL-3.0-only OR CC-BY-4.0) -->

# JSON-RPC Interface Specification

This doc describes all supported JSON-RPC methods, their parameters and return values.

This doc does NOT describe all possible erroneous situations.

## `web3_*` Methods

### `web3_clientVersion`
Returns detailed `skaled` version
#### Parameters
None
#### Returns
`String` representing exact build of `skaled`

Example: "skaled/3.19.0+commit.859d742c/linux/gnu9.5.0/debug"

### `web3_sha3`
Returns `sha3` (`keccak256`) hash of input data
#### Parameters
1. Input data represented as a "0x"-prefixed hex `String`
#### Returns
Output data represented as a "0x"-prefixed hex `String` (32 bytes)

## `net_*` Methods

### `net_version`
Returns chainID from config.json
#### Parameters
None
#### Returns
Decimal number as `String`

### `net_listening`
Returns `true`
#### Parameters
None
#### Returns
Boolean literal `true`

### `net_peerCount`
Returns 0
#### Parameters
None
#### Returns
`String` value "0x0"

## `eth_*` Methods

### `eth_protocolVersion`
Returns `0x3f`
#### Parameters
None
#### Returns
`String` value "0x3f"

### `eth_syncing`
Returns `false`
#### Parameters
None
#### Returns
Boolean literal `false`

### `eth_coinbase`
Returns sChainOwner address from config.json (it is used as coinbase address)
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` (20 bytes)

### `eth_mining`
Returns `false`
#### Parameters
None
#### Returns
Boolean literal `false`

### `eth_hashrate`
There is no hashrate for SKALE s-chains, always returns 0
#### Parameters
None
#### Returns
`String` literal "0x0"

### `eth_gasPrice`
Returns current minimum gas price needed for transaction to be accepted into the Transaction Queue. Gas price is dynamically adjusted from 100k wei and above as load grows
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` representing current gas price

### `eth_accounts`
Get list of accounts with locally-stored private keys
#### Parameters
None
#### Returns
`Array` of "0x"-prefixed hex `String`s, 20 bytes each

### `eth_blockNumber`
Returns the number of most recent block
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` representing block number

### `eth_getBalance`
Returns the balance of the account of given address
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes
2. Block number: `String` that is interpreted differently for normal and historic builds:
Normal build: parameter ignored, latest balance is always returned.
Historic build:
 - "latest" or "pending" - latest balance is returned;
 - "earliest" - balance before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Returns
"0x"-prefixed hex `String` representing balance in wei

### `eth_getStorageAt`
Returns the value from a storage position at a given account
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Position: `String` representation of an integer storage position, either decimal or "0x"-prefixed hexadecimal;
3. Block number: `String` that is interpreted differently for normal and historic builds:
Normal build: parameter ignored, latest value is always returned.
Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Returns
"0x"-prefixed hex `String` (32 bytes)

### `eth_getTransactionCount`
Returns the number of transactions sent from an address.
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Block number: `String` that is interpreted differently for normal and historic builds:
Normal build: parameter ignored, latest value is always returned.
Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Returns
"0x"-prefixed hex `String` representing transaction count

eth_getBlockTransactionCountByHash
eth_getBlockTransactionCountByNumber

### `eth_getUncleCountByBlockHash`
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
#### Returns
`String` literal "0x0"

### `eth_getUncleCountByBlockNumber`
#### Parameters
1. Block number:
 - "earliest", "latest", or "pending";
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Returns
`String` literal "0x0"

### `eth_getCode`
Returns code at a given address
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Block number: `String` that is interpreted differently for normal and historic builds:
Normal build: parameter ignored, latest value is always returned.
Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Returns
"0x"-prefixed hex `String`

### `eth_sign`
Not supported

### `eth_sendTransaction`
Creates new transaction from the provided fields, signs it with the specified `from` address and submits it to the Transaction Queue
#### Parameters
1. JSON object with the following fields:
 - "from": OPTIONAL "0x"-prefixed hex `String`, 20 bytes OR null; if omitted or null, personal account with the largest balance is used;
 - "to": OPTIONAL "0x"-prefixed hex `String`, 20 bytes OR "0x" OR null; omitted/null/""/"0x" means contract creation;
 - "value": OPTIONAL decimal `String` OR "0x"-prefixed hexadecimal `String` OR integer literal OR "" OR "0x" OR null; defaults to 0;
 - "gas": OPTIONAL decimal `String` OR "0x"-prefixed hexadecimal `String` OR integer literal OR null; defaults to 90000 if omitted or null;
 - "gasPrice": OPTIONAL decimal `String` OR "0x"-prefixed hexadecimal `String` OR integer literal OR null; defaults to eth_gasPrice() if omitted or null;
 - "maxFeePerGas": OPTOPNAL same as "gasPrice", used if "gasPrice" is absent or null;
 - "code": OPTIONAL same as "data";
 - "data": OPTIONAL "0x"-prefixed hex `String` OR "0x" OR "" OR null; defaults to empty;
 - "input": OPTIONAL same as "data";
 - "nonce": OPTIONAL decimal `String` OR "0x"-prefixed hexadecimal `String` OR integer literal OR null; defaults to current nonce of the sender OR maximum nonce of the sender's transactions in the Transaction Queue if it is larger (default is applied if omitted or null).
#### Returns
 "0x"-prefixed hex `String`, 32 bytes - hash of the transaction.
 If transaction was attributed to a proxy account, empty hash is returned.

 TODO What the heck is proxy account?

### `eth_sendRawTransaction`
Submit pre-signed transaction into the Transaction Queue
#### Parameters
1. "0x"-prefixed hex `String` - transaction bytes.
#### Returns
"0x"-prefixed hex `String`, 32 bytes - hash of the transaction.

### `eth_call`
Execute read-only contract call
#### Parameters
1. Same object as in `eth_sendTransaction`. TODO Find out if decimal literals are allowed in ETH
2. Block number: `String` that is interpreted differently for normal and historic builds:
Normal build: parameter ignored, latest value is always returned.
Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal: the `State` after executing of the specified block is used to execute call.
#### Returns
"0x"-prefixed hex `String` or "0x", call result

### `eth_estimateGas`
Execute transaction on a temporary state without committing to DB and return gas usage
#### Parameters
1. Same object as in `eth_sendTransaction`.
2. OPTIONAL Ignored
#### Returns
"0x"-prefixed hex `String`, gas estimate

### `eth_getBlockByHash`
Return details about block
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
2. Include transactions: boolean literal - true/false
#### Returns
`null` if block is absent or rotated out.
Otherwise - object with the following fields:
TODO add descriptions
 - "hash"
 - "parentHash"
 - "sha3Uncles"
 - "author"
 - "stateRoot"
 - "transactionsRoot"
 - "receiptsRoot"
 - "number"
 - "gasUsed"
 - "gasLimit"
 - "extraData"
 - "logsBloom"
 - "timestamp"
 - "miner"
 - "nonce"
 - "seedHash"
 - "mixHash"
 - "boundary"
 - "difficulty"
 - "totalDifficulty"
 - "size"
 - "uncles"
 - "baseFeePerGas"
 - "transactions" TODO document skipping of invalid

### `eth_getBlockByNumber`
Return details about block
#### Parameters
1. Block number:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
2. Include transactions: boolean literal - true/false
#### Returns
Same as `eth_getBlockByHash`
#### Exceptions
TODO Check that it Raises "block not found" error if block is "rotated out"

### `eth_getTransactionByHash`
Return details about transaction
#### Parameters
1. Transaction hash: "0x"-prefixed hex `String`, 32 bytes
#### Returns
`null` if transaction not found.
Otherwise, object with the following fields:
TODO describe
 - "blockHash"
 - "blockNumber"
 - "from"
 - "gas"
 - "gasPrice"
 - "hash"
 - "input"
 - "nonce"
 - "to"
 - "transactionIndex"
 - "value"
 - "v"
 - "r"
 - "s"
 - "type"
 - "yParity"
 - "accessList"
 - "maxPriorityFeePerGas"
 - "maxFeePerGas"

### `eth_getTransactionByBlockHashAndIndex`
Return details about transaction
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
2. Transaction index: either decimal or "0x"-prefixed hexadecimal `String`
#### Returns
Same as `eth_getTransactionByHash`

### `eth_getTransactionByBlockNumberAndIndex`
Return details about transaction
#### Parameters
1. Block number:
 - "latest" or "pending" - latest block is used;
 - "earliest" - block 0;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
3. Transaction index: either decimal or "0x"-prefixed hexadecimal `String`
#### Returns
Same as `eth_getTransactionByHash`

### `eth_getTransactionReceipt`
Get transaction receipt
#### Parameters
1. Transaction hash: "0x"-prefixed hex `String`, 32 bytes
#### Returns
`null` if transaction not mined.
Otherwise - object with the following fields:
TODO descrition
 - "from"
 - "to"
 - "transactionHash"
 - "transactionIndex"
 - "blockHash"
 - "blockNumber"
 - "cumulativeGasUsed"
 - "gasUsed"
 - "contractAddress"
 - "logs"
 - "logsBloom"
 - "status"
 - "revertReason" OPTIONAL
 - "type"
 - "effectiveGasPrice"

### `eth_getUncleByBlockHashAndIndex`
Return `null`
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
2. Uncle index: either decimal or "0x"-prefixed hexadecimal `String`
#### Returns
`null`

### `eth_getUncleByBlockNumberAndIndex`
Return `null`
#### Parameters
1. Block number:
 - "latest" or "pending" - latest block;
 - "earliest" - block 0;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
3. Uncle index: either decimal or "0x"-prefixed hexadecimal `String`
#### Returns
`null`

### `eth_compile*` and `eth_getCompilers`
Not supported

### `eth_newFilter`
Creates new logs (events) filter and returns it's ID

| Variability |   |
|-----|-----------|
| ETH | Same |
| Historic | Same |

#### Parameters
1. Object:
 - "fromBlock" OPTIONAL defaults to "earliest" with values:
  - "earliest", "latest" or "pending";
  - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
 - "toBlock" OPTIONAL defaults to "pending" with values:
  - "earliest", "latest" or "pending";
  - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
 - "address" OPTIONAL: "0x"-prefixed hex `String`, 20 bytes;
 - "topics" OPTIONAL: `Array`, see https://ethereum.org/en/developers/docs/apis/json-rpc/#eth_newfilter
#### Returns
"0x"-prefixed hex `String` - ID of the filter
#### Notes
Any non-WS filter is deleted if it's not being polled for 20 seconds

### `eth_newBlockFilter`
Create filter for monitoring new blocks and returns it's ID
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` - ID of the filter

### `eth_newPendingTransactionFilter`
Create filter for monitoring new transactins and returns it's ID
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` - ID of the filter

### `eth_uninstallFilter`
Remove previously created filter
#### Parameters
1. Decimal or "0x"-prefixed hex `String` - ID of the filter
#### Returns
`true` if filter was successfully found and uninstalled, `false` if it's not found

### `eth_getFilterChanges`
Get changes in filter results since previous call (or filter creation)
Ignores logs that originated from blocks that were "rotated out"
#### Parameters
1.  Decimal or "0x"-prefixed hex `String` - ID of the filter
#### Returns
`Array` of changes (can be empty).

For a block filter, array items are block hashes.

For a transaction filter, array items are transaction hashes.

For an event filter, array items are objects with the following fields:
TODO description
 - "data"
 - "address"
 - "topics"
 - "polarity"
 - "type": "mined" or "pending"
 - "blockNumber"
 - "blockHash"
 - "logIndex"
 - "transactionHash"
 - "transactionIndex"

#### Exceptions
Throws `INVALID_PARAMS` if filter cannot be found

#### Notes
If a block is removed from the DB due to block rotation, it doesn't affect this call results

### `eth_getFilterLogs`
Get all events matching a filter
#### Parameters
1.  Decimal or "0x"-prefixed hex `String` - ID of the filter
#### Returns
Same as `eth_getFilterChanges`
TODO Compare
#### Exceptions
Throws `INVALID_PARAMS` if filter cannot be found or if response size is exceeded

### `eth_getLogs`
Same as `eth_getFilterLogs`, but doesn't require filter creation
#### Parameters

1. Object:
 - "fromBlock" OPTIONAL defaults to "earliest" with values:
  - "earliest", "latest" or "pending";
  - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
 - "toBlock" OPTIONAL defaults to "pending" with values:
  - "earliest", "latest" or "pending";
  - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
 - "address" OPTIONAL: "0x"-prefixed hex `String`, 20 bytes;
 - "topics" OPTIONAL: `Array`, see https://ethereum.org/en/developers/docs/apis/json-rpc/#eth_newfilter
 - "blockHash" OPTIONAL: "0x"-prefixed hex `String`, 32 bytes; if this field is present, then `fromBlock` and `toBlock` are not allowed.
#### Returns
Same as `eth_getFilterChanges`
#### Exceptions
Throws `INVALID_PARAMS` if block does not exist, if response size is exceeded, or `fromBlock` or `toBlock` are present together with `blockHash`

### `eth_getWork`
Returns three magic hashes
#### Parameters
None
#### Returns
`Array` of three "0x"-prefixed hex `String`s, 32 bytes each

### `eth_submitWork`
Weird legacy method
#### Parameters
1. nonce: decimal or "0x"-prefixed hexadecimal number, 8 bytes;
2. powHash: decimal or "0x"-prefixed hexadecimal number, 32 bytes;
3. mixDigest: decimal or "0x"-prefixed hexadecimal number, 32 bytes.
#### Returns
`true`

### `eth_submitHashrate`
Weird legacy method
#### Parameters
1. hashrate: decimal or "0x"-prefixed hexadecimal number, 32 bytes;
2. miner id: decimal or "0x"-prefixed hexadecimal number, 32 bytes.
#### Returns
`true`

### `eth_getProof`
Not supported

### `eth_unregister`
### `eth_unsubscribe`
### `eth_chainId`
### `eth_feeHistory`
### `eth_fetchQueuedTransactions`
### `eth_flush`
### `eth_createAccessList`
### `eth_getBlockTransactionCountByHash`
### `eth_getBlockTransactionCountByNumber`
### `eth_getFilterChangesEx`
### `eth_getStorageRoot`
### `eth_inspectTransaction`
### `eth_maxPriorityFeePerGas`
### `eth_notePassword`
### `eth_pendingTransactions`
### `eth_register`
### `eth_signTransaction`
### `eth_subscribe`

## `personal_*` Methods
Not supported

## `db_*` Methods
Not supported

## `shh_*` Methods
Not supported
