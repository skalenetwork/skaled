#ifndef SKIPINVALIDTRANSACTIONSPATCH_H
#define SKIPINVALIDTRANSACTIONSPATCH_H

#include <libethcore/BlockHeader.h>
#include <libethereum/ChainParams.h>
#include <libethereum/Interface.h>
#include <libethereum/SchainPatch.h>
#include <libethereum/Transaction.h>

// What this patch does:
// 1. "Invalid" transactions that came with winning block proposal from consensus
// are skipped, and not included in block.
// Their "validity is determined in Block::syncEveryone:
// a) Transactions should have gasPrice >= current block min gas price
// b) State::execute should not throw (it causes WouldNotBeInBlock exception).
//    Usually this exception is caused by Executive::verifyTransaction() failure.
//
// 2. Specifically for historic node - we ignore "invalid" transactions that
//    are already in block as though they never came.
// This affects following JSON-RPC calls:
// 1) eth_getBlockByHash/Number
// 2) eth_getTransactionReceipt (affects "transactionIndex" field)
// 3) eth_getBlockTransactionCountByHash/Number
// 4) eth_getTransactionByHash (invalid transactions are treated as never present)
// 5) eth_getTransactionByBlockHash/NumberAndIndex
// Transactions are removed from Transaction Queue as usually.

class SkipInvalidTransactionsPatch : public SchainPatch {
public:
    static void init( const dev::eth::ChainParams& _cp, const dev::eth::Interface* _client ) {
        activationTimestamp = _cp.sChain.skipInvalidTransactionsPatchTimestamp;
        printInfo( __FILE__, activationTimestamp );
        assert( _client );
        client = _client;
    }
    static bool needToKeepTransaction( bool _excepted );
    static bool isActiveInBlock( dev::eth::BlockNumber _bn );

private:
    static bool activateTimestampPassed();
    static time_t activationTimestamp;
    static time_t lastBlockTimestamp;
    static const dev::eth::Interface* client;
};

#endif  // SKIPINVALIDTRANSACTIONSPATCH_H
