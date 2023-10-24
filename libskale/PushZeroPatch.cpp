#include "PushZeroPatch.h"

time_t PushZeroPatch::pushZeroPatchTimestamp;
time_t PushZeroPatch::lastBlockTimestamp;

bool PushZeroPatch::isEnabled() {
    if ( pushZeroPatchTimestamp == 0 ) {
        return false;
    }
    return pushZeroPatchTimestamp <= lastBlockTimestamp;
}
