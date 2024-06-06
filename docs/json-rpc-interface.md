<!-- SPDX-License-Identifier: (GPL-3.0-only OR CC-BY-4.0) -->

# JSON-RPC Interface Compatibility 

## `web3_*` Methods

### `web3_clientVersion`
Returns detailed `skaled` version
### Parameters
None
### Returns
`String` representing exact build of `skaled`

Example: "skaled/3.19.0+commit.859d742c/linux/gnu9.5.0/debug"

### `web3_sha3`
Returns `sha3` (`keccak256`) hash of input data
### Parameters
1. Input data represented as a "0x"-prefixed hex `String`
### Returns
Output data represented as a "0x"-prefixed hex `String` (32 bytes)

## `net_*` Methods

### `net_version`
Returns chainID from config.json
### Parameters
None
### Returns
Decimal number as `String`

### `net_listening`
Returns `true`
### Parameters
None
### Returns
Boolean literal `true`

### `net_peerCount`
Returns 0
### Parameters
None
### Returns
`String` value "0x0"

## `eth_*` Methods

### `eth_protocolVersion`
Returns `0x3f`
### Parameters
None
### Returns
`String` value "0x3f"

### `eth_syncing`
Returns `false`
### Parameters
None
### Returns
Boolean literal `false`

### `eth_coinbase`
Returns sChainOwner address from config.json (it is used as coinbase address)
### Parameters
None
### Returns
"0x"-prefixed hex `String` (20 bytes)

### `eth_mining`
Returns `false`
### Parameters
None
### Returns
Boolean literal `false`

### `eth_hashrate`
There is no hashrate for SKALE s-chains, always returns 0
### Parameters
None
### Returns
`String` literal "0x0"

### `eth_gasPrice`
Returns current minimum gas price needed for transaction to be accepted into the Transaction Queue. Gas price is dynamically adjusted from 100k wei and above as load grows
### Parameters
None
### Returns
"0x"-prefixed hex `String` representing current gas price

### `eth_accounts`
Get list of accounts with locally-stored private keys
### Parameters
None
### Returns
`Array` of "0x"-prefixed hex `String`s, 20 bytes each

### `eth_blockNumber`
Returns the number of most recent block
### Parameters
None
### Returns
"0x"-prefixed hex `String` representing block number

### `eth_getBalance`
Returns the balance of the account of given address
### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes
2. Block number: `String` that is interpreted differently for normal and historic builds:
Normal build: parameter ignored, latest balance is always returned.
Historic build:
 - "latest" or "pending" - latest balance is returned;
 - "earliest" - balance before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
### Returns
"0x"-prefixed hex `String` representing balance in wei

## `eth_getStorageAt`
Returns the value from a storage position at a given account
### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Position: `String` representation of an integer storage position, either decimal or "0x"-prefixed hexadecimal;
3. Block number: `String` that is interpreted differently for normal and historic builds:
Normal build: parameter ignored, latest value is always returned.
Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
### Returns
"0x"-prefixed hex `String` (32 bytes)

## `eth_getTransactionCount`
Returns the number of transactions sent from an address.
### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Block number: `String` that is interpreted differently for normal and historic builds:
Normal build: parameter ignored, latest value is always returned.
Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
### Returns
"0x"-prefixed hex `String` representing transaction count

eth_getBlockTransactionCountByHash
eth_getBlockTransactionCountByNumber

eth_getUncleCountByBlockHash
eth_getUncleCountByBlockNumber
### Returns
`String` literal "0x0"

## `eth_getCode`
Returns code at a given address
### Parameters
1. Address: "0x"-prefixed hex `String`, 20 bytes;
2. Block number: `String` that is interpreted differently for normal and historic builds:
Normal build: parameter ignored, latest value is always returned.
Historic build:
 - "latest" or "pending" - latest value is returned;
 - "earliest" - value before block 1 is returned;
 - `String` representation of an integer block number, either decimal or "0x"-prefixed hexadecimal.
### Returns
"0x"-prefixed hex `String`

## `eth_sign`
Not supported

## `eth_sendTransaction`
Creates new transaction from the provided fields, signs it with the specified `from` address and submits it to the Transaction Queue
### Parameters
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
 - "nonce": OPTIONAL decimal `String` OR "0x"-prefixed hexadecimal `String` OR integer literal OR null; defaults to current nonce of the sender OR maximum nonce of the sender's transactions in the Transaction Queue (if omitted or null).

| eth_sendRawTransaction                  |      Supported      |                                                                               |
| eth_call                                | Partially supported | Second parameter is ignored and always set to "latest"                        |
| eth_estimateGas                         |      Supported      | But does not use binary search                                                |
| eth_getBlockByHash                      |      Supported      | Old blocks are "rotated out"                                                  |
| eth_getBlockByNumber                    |      Supported      | Raises "block not found" error if block is "rotated out"                      |
| eth_getTransactionByHash                |      Supported      |                                                                               |
| eth_getTransactionByBlockHashAndIndex   |      Supported      |                                                                               |
| eth_getTransactionByBlockNumberAndIndex |      Supported      |                                                                               |
| eth_getTransactionReceipt               |      Supported      |                                                                               |
| eth_getUncleByBlockHashAndIndex         |      Supported      | There are no uncles in SKALE s-chains                                         |
| eth_getUncleByBlockNumberAndIndex       |      Supported      | There are no uncles in SKALE s-chains                                         |
| eth_getCompilers                        |    Not supported    |                                                                               |
| eth_compileSolidity                     |    Not supported    |                                                                               |
| eth_compileLLL                          |    Not supported    |                                                                               |
| eth_compileSerpent                      |    Not supported    |                                                                               |
| eth_newFilter                           | Partially supported | Ignores logs that originated from blocks that were "rotated out"              |
| eth_newBlockFilter                      |      Supported      |                                                                               |
| eth_newPendingTransactionFilter         |      Supported      |                                                                               |
| eth_uninstallFilter                     |      Supported      |                                                                               |
| eth_getFilterChanges                    |      Supported      |                                                                               |
| eth_getFilterLogs                       |      Supported      |                                                                               |
| eth_getLogs                             | Partially supported | Ignores logs that originated from blocks that were "rotated out"              |
| eth_getWork                             |      Supported      |                                                                               |
| eth_submitWork                          |    Not supported    |                                                                               |
| eth_submitHashrate                      |      Supported      |                                                                               |
| eth_getProof                            |    Not supported    |                                                                               |

## `personal_*` Methods
Not supported

## `db_*` Methods
Not supported

## `shh_*` Methods
Not supported
