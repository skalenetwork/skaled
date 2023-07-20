# State Root calculation

State Root is used to ensure blockchain state is the same on all the schain nodes. 
StateRoot is calculated on each snapshot (after doing [n]-th snapshot we calculate its hash and use [n-1] -th snapshot’s hash as StateRoot).

    [StateRoot = SHA256(Filestorage's hash, LastPrice's hash, Blocks' hash, State's hash)]

### Filestorage hash
Filestorage hash consists of hashes of all filenames and their content.

Filestorage hash affects:
- file names
- file content
- absolute path to mount directory (will be removed soon)

### LastPrice hash
LastPrice hash - hash of gas price for the last block in consensus.

Last price affects:
- last price

### Blocks hash

Blocks hash consists of hashes of all blocks' data (all block fields).

Blocks hash affects:
- all block fields

### State hash

State hash consists of hashes of all accounts' data (if account - nonce, balance etc, if contract - code etc).

State hash affects:
- state of all accounts in blockchain including predeployed contracts, nodes' accounts etc (some of this data generated on startup and pass in config file)
