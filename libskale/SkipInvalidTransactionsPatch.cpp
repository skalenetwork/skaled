#include "SkipInvalidTransactionsPatch.h"

using namespace dev::eth;

time_t SkipInvalidTransactionsPatch::activationTimestamp;
time_t SkipInvalidTransactionsPatch::lastBlockTimestamp;

bool SkipInvalidTransactionsPatch::activateTimestampPassed() {
    if ( activationTimestamp == 0 )
        return false;
    return lastBlockTimestamp >= activationTimestamp;
}

bool SkipInvalidTransactionsPatch::needToKeepTransaction( bool excepted ) {
    if ( !activateTimestampPassed() )
        return true;
    return !excepted;
}
