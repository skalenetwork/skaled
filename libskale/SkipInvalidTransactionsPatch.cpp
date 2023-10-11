#include "SkipInvalidTransactionsPatch.h"

using namespace dev::eth;

time_t SkipInvalidTransactionsPatch::activationTimestamp;
const dev::eth::Interface* SkipInvalidTransactionsPatch::client;

bool SkipInvalidTransactionsPatch::activateTimestampPassed() {
    if ( activationTimestamp == 0 )
        return false;
    return client->blockInfo( client->number() ).timestamp() >= activationTimestamp;
}

bool SkipInvalidTransactionsPatch::needToKeepTransaction( bool _excepted ) {
    if ( !activateTimestampPassed() )
        return true;
    return !_excepted;
}

// TODO better start to apply patches from 1st block after timestamp, not second
bool SkipInvalidTransactionsPatch::isActiveInBlock( BlockNumber _bn ) {
    assert( _bn != PendingBlock );
    assert( _bn != 0 );

    if ( _bn == LatestBlock )
        _bn = client->number();

    time_t prev_ts = client->blockInfo( _bn - 1 ).timestamp();

    return prev_ts >= activationTimestamp;
}
