#include "CorrectForkInPowPatch.h"

time_t CorrectForkInPowPatch::activationTimestamp;
time_t CorrectForkInPowPatch::lastBlockTimestamp;

bool CorrectForkInPowPatch::isEnabled() {
    if ( activationTimestamp == 0 ) {
        return false;
    }
    return activationTimestamp <= lastBlockTimestamp;
}
