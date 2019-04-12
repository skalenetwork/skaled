# Changelog

All notable changes to this project will be documented in this file.

## [skale broadcast]

### Added
- Skale web3 server interface  
*Note: this implements skale_receiveTransaction call*
    - libweb3jsonrpc/Skale.h
    - libweb3jsonrpc/Skale.cpp
    - libweb3jsonrpc/SkaleFace.h 
    - libweb3jsonrpc/SkaleFace.cpp
     
         
- Skale web3 client interface  
*Note: this sends broadcast to skale_receiveTransaction call*  
    - libweb3jsonrpc/SkaleClient.h


- SkaleHost class for nodes communication (it actually calls lib-json-rpc client)
    -  libethereum/SkaleHost.h
    -  libethereum/SkaleHost.cpp

### Changed

Note: code modifications are marked by `///skale` comment

- Modified main():
    - aleth/main.cpp
        - added `-skale` parameter
        - added passing SkaleFace to RPC server

        
- Modified Genesis config classes:
    - libethashseal/GenesisInfo.cpp                 
    - libethashseal/GenesisInfo.h 
    - libethashseal/genesis/skale.cpp *#Note: default skale config for `-skale` option*
    - libethcore/ChainOperationParams.h *#Note: added skale structures for configuration*
    - libethereum/ChainParams.cpp *#Note: added skale config parsing from json*

- Ethereum eth_sendRawTransaction web3 call  
*Note: removed ethereum logic for transaction verification and added saving to custom ThreadSafeQueue*
    - libweb3jsonrpc/Eth.cpp
- Modified Interface class
    - libethereum/Interface.h
        - Added `broadcast()`
        - Added `addToPendingQueue()`
          
- Modified Client class:
    - libethereum/Client.cpp
        - Added `pendingQueue` and `verificationQueue` fields
        - Implemented `broadcast()` method
        - Implemented `addToPendingQueue()` method

