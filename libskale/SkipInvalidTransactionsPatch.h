#ifndef SKIPINVALIDTRANSACTIONSPATCH_H
#define SKIPINVALIDTRANSACTIONSPATCH_H

#include <libethcore/BlockHeader.h>
#include <libethereum/BlockChain.h>
#include <libethereum/Interface.h>
#include <libethereum/SchainPatch.h>
#include <libethereum/Transaction.h>

namespace dev {
namespace eth {
class Client;
}
}  // namespace dev

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

// TODO better start to apply patches from 1st block after timestamp, not second

class SkipInvalidTransactionsPatch : public SchainPatch {
public:
    static SchainPatchEnum getEnum() { return SchainPatchEnum::SkipInvalidTransactionsPatch; }
    static bool isEnabledInWorkingBlock() { return isPatchEnabledInWorkingBlock( getEnum() ); }
    static bool isEnabledWhen( time_t _committedBlockTimestamp ) {
        return isPatchEnabledWhen( getEnum(), _committedBlockTimestamp );
    }
    static bool isEnabledInBlock(
        const dev::eth::BlockChain& _bc, dev::eth::BlockNumber _bn = dev::eth::PendingBlock ) {
        time_t timestamp = _bc.chainParams().getPatchTimestamp( getEnum() );
        return _bc.isPatchTimestampActiveInBlockNumber( timestamp, _bn );
    }
    // returns true if block N can contain invalid transactions
    // returns false if this block was created with SkipInvalidTransactionsPatch and they were
    // skipped
    static bool hasPotentialInvalidTransactionsInBlock(
        dev::eth::BlockNumber _bn, const dev::eth::BlockChain& _bc );
};

#endif  // SKIPINVALIDTRANSACTIONSPATCH_H
