#include "SkipInvalidTransactionsPatch.h"

using namespace dev::eth;

bool SkipInvalidTransactionsPatch::hasPotentialInvalidTransactionsInBlock(
    dev::eth::BlockNumber _bn, const dev::eth::BlockChain& _bc ) {
    if ( _bn == 0 )
        return false;

    time_t activationTimestamp = _bc.chainParams().getPatchTimestamp( getEnum() );

    if ( activationTimestamp == 0 )
        return true;

    if ( _bn == dev::eth::PendingBlock )
        return !isEnabled( _bc );

    if ( _bn == dev::eth::LatestBlock )
        _bn = _bc.number();

    return !isEnabled( _bc, _bn );
}
