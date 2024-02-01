#include "PushZeroPatch.h"
/*
time_t PushZeroPatch::pushZeroPatchTimestamp;
time_t PushZeroPatch::lastBlockTimestamp;

bool PushZeroPatch::isEnabled() {
    if ( pushZeroPatchTimestamp == 0 ) {
        return false;
    }
    return pushZeroPatchTimestamp <= lastBlockTimestamp;
}
*/

using namespace dev::eth;

EVMSchedule PushZeroPatch::makeSchedule( const EVMSchedule& _base ) {
    EVMSchedule ret = _base;
    ret.havePush0 = true;
    return ret;
}
