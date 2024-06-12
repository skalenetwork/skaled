<!-- SPDX-License-Identifier: (GPL-3.0-only OR CC-BY-4.0) -->

# JSON-RPC Interface Specification

This doc describes all supported JSON-RPC methods, their parameters and return values.

This doc does NOT describe all possible erroneous situations.

## `web3_*` Methods

### `web3_clientVersion`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Mostly same |
| Historic vs ETH | Mostly same |

Returns detailed `skaled` version
#### Parameters
None
#### Returns
`String` representing exact build of `skaled`

Example: "skaled/3.19.0+commit.859d742c/linux/gnu9.5.0/debug"

### `web3_sha3`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Returns `sha3` (`keccak256`) hash of input data
#### Parameters
1. Input data represented as a "0x"-prefixed hex `String`
#### Returns
Output data represented as a "0x"-prefixed hex `String` (32 bytes)

## `net_*` Methods

### `net_version`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Returns chainID from config.json
#### Parameters
None
#### Returns
Decimal number as `String`

### `net_listening`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Returns `true`
#### Parameters
None
#### Returns
Boolean literal `true`

### `net_peerCount`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

Returns 0
#### Parameters
None
#### Returns
`String` value "0x0"

## `eth_*` Methods

### `eth_protocolVersion`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE uses hex, ETH uses dec |
| Historic vs ETH | SKALE uses hex, ETH uses dec |

Returns `0x3f`
#### Parameters
None
#### Returns
`String` value "0x3f"

### `eth_syncing`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE uses dec, ETH uses hex |
| Historic vs ETH | SKALE uses dec, ETH uses hex |

If node has all up-to-date blocks - returns `false`.

If it's behind others and is catching up - retuns object:
 - "startingBlock": decimal literal;
 - "highestBlock": decimal literal;
 - "currentBlock": decimal literal.

#### Parameters
None
#### Returns
Boolean literal `false`

### `eth_coinbase`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Returns sChainOwner address from config.json (it is used as coinbase address)
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` (20 bytes)

### `eth_chainId`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Returns chainID from config.json as hex string
#### Parameters
None
#### Returns
"0x"-prefixed hex `String`

### `eth_mining`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

Returns `false`
#### Parameters
None
#### Returns
Boolean literal `false`

### `eth_hashrate`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

There is no hashrate for SKALE s-chains, always returns 0
#### Parameters
None
#### Returns
`String` literal "0x0"

### `eth_gasPrice`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Returns current minimum gas price needed for transaction to be accepted into the Transaction Queue. Gas price is dynamically adjusted from 100k wei and above as load grows
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` representing current gas price

### `eth_accounts`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Get list of accounts with locally-stored private keys
#### Parameters
None
#### Returns
`Array` of "0x"-prefixed hex `String`s, 20 bytes each

### `eth_blockNumber`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Returns the number of most recent block
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` representing block number

### `eth_getBalance`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

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


### `eth_getBlockTransactionCountByHash`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
#### Returns
"0x"-prefixed hex `String` representing transaction count

### `eth_getBlockTransactionCountByNumber`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Parameters
1. Block number:
 - "earliest", "latest", or "pending";
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Returns
"0x"-prefixed hex `String` representing transaction count

### `eth_getUncleCountByBlockHash`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
#### Returns
`String` literal "0x0"

### `eth_getUncleCountByBlockNumber`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

#### Parameters
1. Block number:
 - "earliest", "latest", or "pending";
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Returns
`String` literal "0x0"

### `eth_getCode`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Not supported |
| Historic vs ETH | Not supported |

Not supported

### `eth_signTransaction`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Sign transaction using stored private key and make raw transaction
#### Parameters
1. Object same as `eth_sendTransaction`
#### Returns
Object:
 - "raw": "0x"-prefixed hex `String`;
 - "tx": same as in `eth_inspectTransaction`

### `eth_sendTransaction`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Submit pre-signed transaction into the Transaction Queue
#### Parameters
1. "0x"-prefixed hex `String` - transaction bytes.
#### Returns
"0x"-prefixed hex `String`, 32 bytes - hash of the transaction.

### `eth_call`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | SKALE ignores block number |

Execute transaction on a temporary state without committing to DB and return gas usage
#### Parameters
1. Same object as in `eth_sendTransaction`.
2. OPTIONAL Ignored
#### Returns
"0x"-prefixed hex `String`, gas estimate

### `eth_getBlockByHash`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Return details about transaction
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
2. Transaction index: either decimal or "0x"-prefixed hexadecimal `String`
#### Returns
Same as `eth_getTransactionByHash`

### `eth_getTransactionByBlockNumberAndIndex`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Return details about transaction
#### Parameters
1. Block number:
 - "latest" or "pending" - latest block is used;
 - "earliest" - block 0;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
2. Transaction index: either decimal or "0x"-prefixed hexadecimal `String`
#### Returns
Same as `eth_getTransactionByHash`

### `eth_getTransactionReceipt`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

Return `null`
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
2. Uncle index: either decimal or "0x"-prefixed hexadecimal `String`
#### Returns
`null`

### `eth_getUncleByBlockNumberAndIndex`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

Return `null`
#### Parameters
1. Block number:
 - "latest" or "pending" - latest block;
 - "earliest" - block 0;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
3. Uncle index: either decimal or "0x"-prefixed hexadecimal `String`
#### Returns
`null`

### `eth_newFilter`

| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Creates new logs (events) filter and returns it's ID

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Create filter for monitoring new blocks and returns it's ID
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` - ID of the filter

### `eth_newPendingTransactionFilter`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Create filter for monitoring new transactins and returns it's ID
#### Parameters
None
#### Returns
"0x"-prefixed hex `String` - ID of the filter

### `eth_uninstallFilter`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Remove previously created filter
#### Parameters
1. Decimal or "0x"-prefixed hex `String` - ID of the filter
#### Returns
`true` if filter was successfully found and uninstalled, `false` if it's not found

### `eth_getFilterChanges`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

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
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

Get all events matching a filter
#### Parameters
1.  Decimal or "0x"-prefixed hex `String` - ID of the filter
#### Returns
Same as `eth_getFilterChanges`
TODO Compare
#### Exceptions
Throws `INVALID_PARAMS` if filter cannot be found or if response size is exceeded

### `eth_getLogs`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

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

### `eth_compile*` and `eth_getCompilers`
Not supported

### `eth_getWork`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

Returns three magic hashes
#### Parameters
None
#### Returns
`Array` of three "0x"-prefixed hex `String`s, 32 bytes each

### `eth_submitWork`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

Weird legacy method
#### Parameters
1. nonce: decimal or "0x"-prefixed hexadecimal number, 8 bytes;
2. powHash: decimal or "0x"-prefixed hexadecimal number, 32 bytes;
3. mixDigest: decimal or "0x"-prefixed hexadecimal number, 32 bytes.
#### Returns
`true`

### `eth_submitHashrate`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

Weird legacy method
#### Parameters
1. hashrate: decimal or "0x"-prefixed hexadecimal number, 32 bytes;
2. miner id: decimal or "0x"-prefixed hexadecimal number, 32 bytes.
#### Returns
`true`

### `eth_getProof`
Not supported

### `eth_register`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

Add proxy account, whatever that can mean
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes
#### Returns
"0x"-prefixed hex `String` - id of added account

### `eth_unregister`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

Remove proxy account
#### Parameters
1. id: decimal or "0x"-prefixed hexadecimal number, previously obtained through `eth_register`
#### Returns
`true` if account was found and removed, `false` otherwise

### `eth_feeHistory`
### `eth_fetchQueuedTransactions`

### `eth_flush`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

Weird mystery method. Probably it will mine a block with PoW
#### Parameters
None
#### Returns
`true`

### `eth_createAccessList`
Returns empy access list
#### Parameters
1. Transaction object same as in `eth_sendTransaction`;
2. `String` block hash or number (ignored).
#### Returns
Object:
 - "accessList" - empty list;
 - "gasUsed" - "0x" prefixed hex `String` - result of `eth_estimateGas`.

### `eth_getBlockTransactionCountByHash`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

Get number of transactions in a block
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
#### Returns
"0x"-prefixed hex `String`

### `eth_getBlockTransactionCountByNumber`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

Get number of transactions in a block
#### Parameters
1. Block number:
 - "latest" or "pending" - latest block is used;
 - "earliest" - block 0;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
#### Returns
"0x"-prefixed hex `String`

### `eth_getFilterChangesEx`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

Same as `eth_getFilterChanges`

### `eth_getStorageRoot`

| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unsupported in both |
| Historic vs ETH | Supported in Historic, unsupported in ETH |

Get account's `storageRoot` (according to Yellow Paper)
Noramal node - call always throws exception.
Historic node - see below.
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Block number:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal - value after execution of specified block is returned.
#### Returns
"0x"-prefixed hex `String` (32 bytes). If account cannot be found - `sha3( rlp( "" ) )` is returned

### `eth_inspectTransaction`

| Compatibility |   |
|-----|-----------|
| Core vs ETH |  Unsupported in ETH |
| Historic vs ETH | Unsupported in ETH |

Parse binary transaction into fields
#### Parameters
1. Raw transaction: "0x"-prefixed hex `String`
#### Returns
Object:
 - "to"
 - "from"
 - "gas"
 - "gasPrice"
 - "value"
 - "data" 
 - "nonce"
 - "r"
 - "s"
 - "v"
 - "type"
 - "yParity"
 - "accessList"
 - "maxPriorityFeePerGas"
 - "maxFeePerGas"

### `eth_maxPriorityFeePerGas`
#### Parameters
None
#### Returns
"0x0"

### `eth_notePassword`
Weird legacy method
#### Parameters
1. `String` - ignored
#### Returns
`false`

### `eth_pendingTransactions`
Get transaction queue
#### Parameters
None
#### Returns
 - "to"
 - "from"
 - "gas"
 - "gasPrice"
 - "value"
 - "data"
 - "nonce"
 - "r"
 - "s"
 - "v"
 - "type"
 - "yParity"
 - "accessList"
 - "maxPriorityFeePerGas"
 - "maxFeePerGas"
 - "hash"
 - "sighash"

### `eth_subscribe`
HTTP(S): unsupported, always throws exception

WS(S): subscribe for new events/transactions/blocks/stats
#### Parameters
1. `String` subscription type:
 - "logs";
 - "newPendingTransactions";
 - "newHeads";
 - "skaleStats".
2. Used only when type="logs": object, format is the same as in `eth_newFilter`
#### Returns
"0x"-prefixed hex `String` - subscription id

#### Events format
Depending on the type of subscription, different data is pushed back to the client when requested event happens.
1. type="logs":
```
{
"jsonrpc":"2.0",
"method":"eth_subscription",
"params":{
    "result":{
        "address":"0x"-prefixed hex `String`, 20 bytes,
        "blockHash":"0x"-prefixed hex `String`, 32 bytes,
        "blockNumber":"0x"-prefixed hex `String`,
        "data":"0x"-prefixed hex `String`,
        "logIndex": integer literal,
        "topics": array of "0x"-prefixed hex `String`s, 32 bytes each,
        "transactionHash":"0x"-prefixed hex `String`, 32 bytes,
        "transactionIndex": integer literal
    },
    "subscription": "0x"-prefixed hex `String`, subscription id
    }
}
```
2. type="newHeads"
```
{
"jsonrpc":"2.0",
"method":"eth_subscription",
"params":{
    "result": see `eth_getBlockByHash`, includeTransactions=false,
    "subscription": "0x"-prefixed hex `String`, subscription id
    }
}
```

3. type="newPendingTransactions"
```
{
"jsonrpc":"2.0",
"method":"eth_subscription",
"params":{
    "result": "0x"-prefixed hex `String`, 32 bytes - transaction hash,
    "subscription": "0x"-prefixed hex `String`, subscription id
    }
}
```

### `eth_unsubscribe`
HTTP(S): unsupported, always throws exception

WS(S): unsubscribe from events/transactions/blocks/stats (see `eth_subscribe`)
#### Parameters
1. id: decimal or "0x"-prefixed hexadecimal number `String` OR number literal - previously obtained through `eth_subscribe`
#### Returns
None

## `personal_*` Methods
Not supported

## `db_*` Methods
Not supported

## `shh_*` Methods
Not supported

## Non-standard Methods
### setSchainExitTime
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
### oracle_submitRequest
### oracle_checkResult

### debug_accountRangeAt
Not supported, always throws

### debug_traceTransaction
### debug_storageRangeAt
Not supported, always throws

### debug_preimage
Not supported, always throws

### debug_traceBlockByNumber
### debug_traceBlockByHash
### debug_traceCall

### debug_getFutureTransactions
Get future transaction queue
#### Parameters
None
#### Returns
 - "to"
 - "from"
 - "gas"
 - "gasPrice"
 - "value"
 - "data"
 - "nonce"
 - "r"
 - "s"
 - "v"
 - "type"
 - "yParity"
 - "accessList"
 - "maxPriorityFeePerGas"
 - "maxFeePerGas"
 - "hash"
 - "sighash"

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
