<!-- SPDX-License-Identifier: (GPL-3.0-only OR CC-BY-4.0) -->

# JSON-RPC Interface Specification

This doc describes all supported JSON-RPC methods, except `skale_*` and debug methods. Doc describes methods' parameters and return values.

This doc does NOT describe all possible erroneous situations.

Parameters marked as `OPTIONAL` are optional, otherwise they are required!

## `web3_*` Methods

### `web3_clientVersion`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Mostly same |
| Historic vs ETH | Mostly same |

#### Description
Get detailed `skaled` version
#### Parameters
None
#### Return format
`String` representing exact build of `skaled`

Example: "skaled/3.19.0+commit.859d742c/linux/gnu9.5.0/debug"

### `web3_sha3`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get `sha3` (`keccak256`) hash of input data
#### Parameters
1. Input data represented as a "0x"-prefixed hex `String`
#### Return format
Output data represented as a "0x"-prefixed hex `String` (32 bytes)

## `net_*` Methods

### `net_version`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Returns chainID from config.json
#### Parameters
None
#### Return format
Decimal number as `String`

### `net_listening`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Returns `true`
#### Parameters
None
#### Return format
Boolean literal `true`

### `net_peerCount`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

#### Description
Returns 0
#### Parameters
None
#### Return format
`String` value "0x0"

## `eth_*` Methods

### `eth_protocolVersion`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE uses hex, ETH uses dec |
| Historic vs ETH | SKALE uses hex, ETH uses dec |

#### Description
Returns `0x3f`
#### Parameters
None
#### Return format
`String` value "0x3f"

### `eth_syncing`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE uses dec, ETH uses hex |
| Historic vs ETH | SKALE uses dec, ETH uses hex |

#### Description
Get node's sync status.

If node has all up-to-date blocks - returns `false`.

If it's behind others and is catching up - retuns object:
 - "startingBlock": decimal literal;
 - "highestBlock": decimal literal;
 - "currentBlock": decimal literal.

#### Parameters
None
#### Return format
Boolean literal `false`

### `eth_coinbase`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Returns sChainOwner address from config.json (it is used as coinbase address)
#### Parameters
None
#### Return format
"0x"-prefixed hex `String` (20 bytes)

### `eth_chainId`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Returns chainID from config.json as hex string
#### Parameters
None
#### Return format
"0x"-prefixed hex `String`

### `eth_mining`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

#### Description
Returns `false`
#### Parameters
None
#### Return format
Boolean literal `false`

### `eth_hashrate`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

#### Description
There is no hashrate for SKALE s-chains, always returns 0
#### Parameters
None
#### Return format
`String` literal "0x0"

### `eth_gasPrice`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get current minimum gas price needed for transaction to be accepted into the Transaction Queue. Gas price is dynamically adjusted from 100k wei and above as load grows
#### Parameters
None
#### Return format
"0x"-prefixed hex `String` representing current gas price

### `eth_accounts`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get list of accounts with locally-stored private keys
#### Parameters
None
#### Return format
`Array` of "0x"-prefixed hex `String`s, 20 bytes each

### `eth_blockNumber`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get the number of most recent block
#### Parameters
None
#### Return format
"0x"-prefixed hex `String` representing block number

### `eth_getBalance`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

#### Description
Get the balance of the account of given address
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes
2. Block number: `String` that is interpreted differently for normal and historic builds:

Normal build: parameter ignored, latest balance is always returned.

Historic build:
 - "latest" or "pending" - latest balance is returned;
 - "earliest" - balance before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Return format
"0x"-prefixed hex `String` representing balance in wei

### `eth_getStorageAt`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

#### Description
Get the value from a storage position at a given account
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Position: `String` representation of an integer storage position, either decimal or "0x"-prefixed hexadecimal;
3. Block number: `String` that is interpreted differently for normal and historic builds:

Normal build: parameter ignored, latest value is always returned.

Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Return format
"0x"-prefixed hex `String` (32 bytes)

### `eth_getTransactionCount`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

#### Description
Get the number of transactions sent from an address.
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Block number: `String` that is interpreted differently for normal and historic builds:

Normal build: parameter ignored, latest value is always returned.

Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Return format
"0x"-prefixed hex `String` representing transaction count


### `eth_getBlockTransactionCountByHash`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get number of transactions in a block
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
#### Return format
"0x"-prefixed hex `String` representing transaction count

### `eth_getBlockTransactionCountByNumber`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get number of transactions in a block
#### Parameters
1. Block number:
 - "earliest", "latest", or "pending";
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Return format
"0x"-prefixed hex `String` representing transaction count

### `eth_getUncleCountByBlockHash`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

#### Description
Always returns 0.

#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
#### Return format
`String` literal "0x0"

### `eth_getUncleCountByBlockNumber`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

#### Description
Always returns 0.

#### Parameters
1. Block number:
 - "earliest", "latest", or "pending";
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Return format
`String` literal "0x0"

### `eth_getCode`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

#### Description
Get code at a given address
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Block number: `String` that is interpreted differently for normal and historic builds:

Normal build: parameter ignored, latest value is always returned.

Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
#### Return format
"0x"-prefixed hex `String`

### `eth_sign`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Not supported in SKALE|
| Historic vs ETH | Not supported in SKALE |

#### Description
Not supported

### `eth_signTransaction`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Sign transaction using stored private key and make raw transaction
#### Parameters
1. Object same as `eth_sendTransaction`
#### Return format
Object:
 - "raw": "0x"-prefixed hex `String`;
 - "tx": same as in `eth_inspectTransaction`

### `eth_sendTransaction`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Create new transaction from the provided fields, sign it with the specified `from` address and submit it to the Transaction Queue
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
#### Return format
 "0x"-prefixed hex `String`, 32 bytes - hash of the transaction.
 If transaction was attributed to a proxy account, empty hash is returned.

### `eth_sendRawTransaction`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Submit pre-signed transaction into the Transaction Queue
#### Parameters
1. "0x"-prefixed hex `String` - transaction bytes.
#### Return format
"0x"-prefixed hex `String`, 32 bytes - hash of the transaction.

### `eth_call`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | Same |

#### Description
Execute read-only contract call
#### Parameters
1. Same object as in `eth_sendTransaction`.
2. Block number: `String` that is interpreted differently for normal and historic builds:

Normal build: parameter ignored, latest value is always returned.

Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal: the `State` after executing of the specified block is used to execute call.
#### Return format
"0x"-prefixed hex `String` or "0x", call result

### `eth_estimateGas`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | SKALE ignores block number |
| Historic vs ETH | SKALE ignores block number |

#### Description
Execute transaction on a temporary state without committing to DB and return gas usage
#### Parameters
1. Same object as in `eth_sendTransaction`.
2. OPTIONAL Ignored
#### Return format
"0x"-prefixed hex `String`, gas estimate

### `eth_getBlockByHash`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get details about block
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
2. Include transactions: boolean literal - true/false
#### Return format
`null` if block is absent or rotated out.

Otherwise - object with the following fields:
 - "hash": "0x"-prefixed hex `String`, 32 bytes;
 - "parentHash": "0x"-prefixed hex `String`, 32 bytes;
 - "sha3Uncles": "0x"-prefixed hex `String`, 32 bytes;
 - "author": "0x"-prefixed hex `String`, 20 bytes - address of schainOwner;
 - "stateRoot": "0x"-prefixed hex `String`, 32 bytes;
 - "transactionsRoot": "0x"-prefixed hex `String`, 32 bytes;
 - "receiptsRoot": "0x"-prefixed hex `String`, 32 bytes;
 - "number": decimal literal;
 - "gasUsed": decimal literal;
 - "gasLimit": decimal literal;
 - "extraData": `String` "0x736b616c65" ("skale" in ASCII);
 - "logsBloom": "0x"-prefixed hex `String`, 256 bytes;
 - "timestamp": decimal literal;
 - "miner": same as `author`;
 - "nonce": `String` "0x0000000000000000";
 - "seedHash": `String` "0x0000000000000000000000000000000000000000000000000000000000000000", 32 bytes;
 - "mixHash": `String` "0x0000000000000000000000000000000000000000000000000000000000000000", 32 bytes;
 - "boundary": `String` "0x0000000000000000000000000000000000000000000000000000000000000000", 32 bytes;
 - "difficulty": decimal literal 0;
 - "totalDifficulty": decimal literal 0;
 - "size": decimal literal;
 - "uncles": empty arra;
 - "baseFeePerGas": "0x"-prefixed hex `String`, gas price in block, field is present only if EIP-1559 is enabled; for rotated out old blocks this field is set to latest gasPrice;
 - "transactions": array of transaction hashes ("0x"-prefixed hex `String`s, 32 bytes) OR detaled transactions info (same as in `eth_getTransactionByHash`) if 2nd argument was `true`;

### `eth_getBlockByNumber`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get details about block
#### Parameters
1. Block number:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
2. Include transactions: boolean literal - true/false
#### Return format
Same as `eth_getBlockByHash`

### `eth_getTransactionByHash`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get details about transaction
#### Parameters
1. Transaction hash: "0x"-prefixed hex `String`, 32 bytes
#### Return format
`null` if transaction not found.

Otherwise, object with the following fields:
 - "blockHash": "0x"-prefixed hex `String`, 32 bytes;
 - "blockNumber": decimal literal;
 - "from": "0x"-prefixed hex `String`, 20 bytes;
 - "gas": decimal literal;
 - "gasPrice": decimal literal;
 - "hash": "0x"-prefixed hex `String`, 32 bytes;
 - "input": "0x"-prefixed hex `String` OR "0x" if there is no input;
 - "nonce": decimal literal;
 - "to": "0x"-prefixed hex `String`, 20 bytes OR `null` if contract creation;
 - "transactionIndex": decimal literal;
 - "value": decimal literal;
 - "v": "0x"-prefixed hex `String`;
 - "r": "0x"-prefixed hex `String`, 32 bytes;
 - "s": "0x"-prefixed hex `String`, 32 bytes;
 - "type": "0x"-prefixed hex `String`: "0x0", "0x1", or "0x2";
 - "yParity": "0x"-prefixed hex `String` (present in `type 1` and `type 2`);
 - "accessList": present in `type 1` and `type 2`;
 - "maxPriorityFeePerGas": "0x"-prefixed hex `String` (present in `type 2` only);
 - "maxFeePerGas": "0x"-prefixed hex `String` (present in `type 2` only);

### `eth_getTransactionByBlockHashAndIndex`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get details about transaction
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
2. Transaction index: either decimal or "0x"-prefixed hexadecimal `String`
#### Return format
Same as `eth_getTransactionByHash`

### `eth_getTransactionByBlockNumberAndIndex`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get details about transaction
#### Parameters
1. Block number:
 - "latest" or "pending" - latest block is used;
 - "earliest" - block 0;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
2. Transaction index: either decimal or "0x"-prefixed hexadecimal `String`
#### Return format
Same as `eth_getTransactionByHash`

### `eth_getTransactionReceipt`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get transaction receipt
#### Parameters
1. Transaction hash: "0x"-prefixed hex `String`, 32 bytes
#### Return format
`null` if transaction not mined.

Otherwise - object with the following fields:
 - "from": "0x"-prefixed hex `String`, 20 bytes;
 - "to": "0x"-prefixed hex `String`, 32 bytes (contains all 0s if contract deployment);
 - "transactionHash": "0x"-prefixed hex `String`, 32 bytes;
 - "transactionIndex": decimal literal;
 - "blockHash": "0x"-prefixed hex `String`, 32 bytes;
 - "blockNumber": decimal literal;
 - "cumulativeGasUsed": decimal literal;
 - "gasUsed": decimal literal;
 - "contractAddress": "0x"-prefixed hex `String`, 20 bytes if deployment OR `null` if not;
 - "logs": `Array`;
 - "logsBloom": "0x"-prefixed hex `String`, 256 bytes;
 - "status": `String` "0x0" or "0x1";
 - "revertReason" OPTIONAL: `String`;
 - "type": "0x"-prefixed hex `String`: "0x0", "0x1", or "0x2";
 - "effectiveGasPrice": "0x"-prefixed hex `String`.

### `eth_getUncleByBlockHashAndIndex`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

#### Description
Return `null`
#### Parameters
1. Block hash: "0x"-prefixed hex `String`, 32 bytes
2. Uncle index: either decimal or "0x"-prefixed hexadecimal `String`
#### Return format
`null`

### `eth_getUncleByBlockNumberAndIndex`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Compatible, but makes no sense in SKALE |
| Historic vs ETH | Compatible, but makes no sense in SKALE |

#### Description
Return `null`
#### Parameters
1. Block number:
 - "latest" or "pending" - latest block;
 - "earliest" - block 0;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal;
3. Uncle index: either decimal or "0x"-prefixed hexadecimal `String`
#### Return format
`null`

### `eth_newFilter`

| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Create new logs (events) filter and return it's ID

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
#### Return format
"0x"-prefixed hex `String` - ID of the filter
#### Notes
Any non-WS filter is deleted if it's not being polled for 20 seconds

### `eth_newBlockFilter`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Create filter for monitoring new blocks and returns it's ID
#### Parameters
None
#### Return format
"0x"-prefixed hex `String` - ID of the filter

### `eth_newPendingTransactionFilter`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Create filter for monitoring new transactins and returns it's ID
#### Parameters
None
#### Return format
"0x"-prefixed hex `String` - ID of the filter

### `eth_uninstallFilter`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Remove previously created filter
#### Parameters
1. Decimal or "0x"-prefixed hex `String` - ID of the filter
#### Return format
`true` if filter was successfully found and uninstalled, `false` if it's not found

### `eth_getFilterChanges`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
Get changes in filter results since previous call (or filter creation)

Ignores logs that originated from blocks that were "rotated out"
#### Parameters
1.  Decimal or "0x"-prefixed hex `String` - ID of the filter
#### Return format
`Array` of changes (can be empty).

For a block filter, array items are block hashes.

For a transaction filter, array items are transaction hashes.

For an event filter, array items are objects with the following fields:
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

#### Description
Get all events matching a filter
#### Parameters
1.  Decimal or "0x"-prefixed hex `String` - ID of the filter
#### Return format
Same as `eth_getFilterChanges`
#### Exceptions
Throws `INVALID_PARAMS` if filter cannot be found or if response size is exceeded
#### Notes
Response size is limited by number of consecutive blocks that can be requested. It is set in `getLogsBlocksLimit` config parameter, which currenly is 2000 blocks.

### `eth_getLogs`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Same |
| Historic vs ETH | Same |

#### Description
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
#### Return format
Same as `eth_getFilterChanges`
#### Exceptions
Throws `INVALID_PARAMS` if block does not exist, if response size is exceeded, or `fromBlock` or `toBlock` are present together with `blockHash`
#### Notes
Response size limit is determined by `getLogsBlocksLimit` config parameter, and currenly is 2000 logs per request.

### `eth_compile*` and `eth_getCompilers`
Not supported

### `eth_getWork`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

#### Description
Returns three magic hashes
#### Parameters
None
#### Return format
`Array` of three "0x"-prefixed hex `String`s, 32 bytes each

### `eth_submitWork`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

#### Parameters
1. nonce: decimal or "0x"-prefixed hexadecimal number, 8 bytes;
2. powHash: decimal or "0x"-prefixed hexadecimal number, 32 bytes;
3. mixDigest: decimal or "0x"-prefixed hexadecimal number, 32 bytes.
#### Return format
`true`

### `eth_submitHashrate`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

#### Parameters
1. hashrate: decimal or "0x"-prefixed hexadecimal number, 32 bytes;
2. miner id: decimal or "0x"-prefixed hexadecimal number, 32 bytes.
#### Return format
`true`

### `eth_getProof`
#### Description
Not supported

### `eth_register`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

#### Description
Add proxy account
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes
#### Return format
"0x"-prefixed hex `String` - id of added account

### `eth_unregister`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

#### Description
Remove proxy account
#### Parameters
1. id: decimal or "0x"-prefixed hexadecimal number, previously obtained through `eth_register`
#### Return format
`true` if account was found and removed, `false` otherwise

### `eth_feeHistory`
### `eth_fetchQueuedTransactions`

### `eth_flush`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

#### Description
Probably it will mine a block with PoW
#### Parameters
None
#### Return format
`true`

### `eth_createAccessList`
Returns empty access list
#### Parameters
1. Transaction object same as in `eth_sendTransaction`;
2. `String` block hash or number (ignored).
#### Return format
Object:
 - "accessList" - empty list;
 - "gasUsed" - "0x" prefixed hex `String` - result of `eth_estimateGas`.

### `eth_getFilterChangesEx`
| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unknown |
| Historic vs ETH | Unknown |

#### Description
Same as `eth_getFilterChanges`

### `eth_getStorageRoot`

| Compatibility |   |
|-----|-----------|
| Core vs ETH | Unsupported in both |
| Historic vs ETH | Supported in Historic, unsupported in ETH |

#### Description
Get account's `storageRoot` (according to Yellow Paper)

Normal node - call always throws exception.

Historic node - see below.
#### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Block number:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal - value after execution of specified block is returned.
#### Return format
"0x"-prefixed hex `String` (32 bytes). If account cannot be found - `sha3( rlp( "" ) )` is returned

### `eth_inspectTransaction`

| Compatibility |   |
|-----|-----------|
| Core vs ETH |  Unsupported in ETH |
| Historic vs ETH | Unsupported in ETH |

#### Description
Parse binary transaction into fields
#### Parameters
1. Raw transaction: "0x"-prefixed hex `String`
#### Return format
Object, see `eth_getTransactionByHash`

### `eth_maxPriorityFeePerGas`
#### Parameters
None
#### Return format
"0x0"

### `eth_notePassword`
#### Parameters
1. `String` - ignored
#### Return format
`false`

### `eth_pendingTransactions`
#### Description
Get transaction queue
#### Parameters
None
#### Return format
`Array` of `Object`s, see `eth_getTransactionByHash`. `sighash` equals to `hash`

### `eth_subscribe`
#### Description
HTTP(S): unsupported, always throws exception

WS(S): subscribe for new events/transactions/blocks/stats
#### Parameters
1. `String` subscription type:
 - "logs";
 - "newPendingTransactions";
 - "newHeads";
 - "skaleStats".
2. Used only when type="logs": object, format is the same as in `eth_newFilter`
#### Return format
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
#### Description
HTTP(S): unsupported, always throws exception

WS(S): unsubscribe from events/transactions/blocks/stats (see `eth_subscribe`)
#### Parameters
1. id: decimal or "0x"-prefixed hexadecimal number `String` OR number literal - previously obtained through `eth_subscribe`
#### Return format
None

## `personal_*` Methods
Not supported

## `db_*` Methods
Not supported

## `shh_*` Methods
Not supported

## `debug_*` Methods
### debug_accountRangeAt
#### Description
Not supported, always throws exception

### debug_traceTransaction
### debug_storageRangeAt
#### Description
Not supported, always throws exception

### debug_preimage
#### Description
Not supported, always throws exception

### debug_traceBlockByNumber
### debug_traceBlockByHash
### debug_traceCall


## Non-standard Methods
### oracle_submitRequest
### oracle_checkResult


