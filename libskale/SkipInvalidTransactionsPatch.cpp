#include "SkipInvalidTransactionsPatch.h"

using namespace dev::eth;

time_t SkipInvalidTransactionsPatch::activationTimestamp;
const dev::eth::Interface* SkipInvalidTransactionsPatch::client;

bool SkipInvalidTransactionsPatch::isEnabled() {
    if ( activationTimestamp == 0 )
        return false;
    return client->blockInfo( client->number() ).timestamp() >= activationTimestamp;
}

// TODO better start to apply patches from 1st block after timestamp, not second
bool SkipInvalidTransactionsPatch::isActiveInBlock( BlockNumber _bn ) {
    if ( _bn == 0 )
        return false;

    if ( _bn == PendingBlock )
        return isEnabled();

    if ( _bn == LatestBlock )
        _bn = client->number();

    time_t prev_ts = client->blockInfo( _bn - 1 ).timestamp();

    return prev_ts >= activationTimestamp;
}
