#include "SchainPatch.h"

dev::eth::ChainOperationParams SchainPatch::chainParams;
time_t SchainPatch::latestBlockTimestamp;

void SchainPatch::init( const dev::eth::ChainOperationParams& _cp ) {
    chainParams = _cp;
}

void SchainPatch::useLatestBlockTimestamp( time_t _timestamp ) {
    latestBlockTimestamp = _timestamp;
}
