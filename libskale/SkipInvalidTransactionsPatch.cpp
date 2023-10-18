#include "SkipInvalidTransactionsPatch.h"

using namespace dev::eth;

time_t SkipInvalidTransactionsPatch::activationTimestamp;
time_t SkipInvalidTransactionsPatch::lastBlockTimestamp;

bool SkipInvalidTransactionsPatch::isEnabled() {
    if ( activationTimestamp == 0 )
        return false;
    return lastBlockTimestamp >= activationTimestamp;
}
