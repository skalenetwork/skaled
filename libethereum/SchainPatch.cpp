#include "SchainPatch.h"

using namespace dev::eth;

ChainOperationParams SchainPatch::chainParams;
time_t SchainPatch::committedBlockTimestamp;

void SchainPatch::init( const dev::eth::ChainOperationParams& _cp ) {
    chainParams = _cp;
}

void SchainPatch::useLatestBlockTimestamp( time_t _timestamp ) {
    committedBlockTimestamp = _timestamp;
}

EVMSchedule PushZeroPatch::makeSchedule( const EVMSchedule& _base ) {
    EVMSchedule ret = _base;
    ret.havePush0 = true;
    return ret;
}
